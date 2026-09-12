#include "sd_reader.h"
#include "board_pins.h"
#include "airports_data.h"
#include "TCA9554PWR.h"
#include <FS.h>
#include <SD_MMC.h>
#include <set>

FlightDataManager flightData;

FlightDataManager::FlightDataManager()
    : _sdMounted(false), _isDemoMode(false), _totalDistanceNM(0.0f), _totalDistanceKM(0.0f) {}

bool FlightDataManager::initSD() {
    Serial.println("[SD] Initializing SD Card (1-bit SD_MMC)...");
    _sdMounted = false;

    // 1. Enable SD Card power/data path via PCA9554 (EXIO4 = pin 4 = SD_D3_EN)
    Set_EXIO(EXIO_PIN4, High);
    vTaskDelay(pdMS_TO_TICKS(50));

    // 2. Configure 1-bit SD_MMC pins: CLK=2, CMD=1, D0=42
    if (!SD_MMC.setPins(2, 1, 42)) {
        Serial.println("[SD] Error: SD_MMC.setPins failed!");
        return false;
    }

    // 3. Mount SDMMC in 1-bit mode
    if (!SD_MMC.begin("/sdcard", true, false, 20000)) {
        Serial.println("[SD] 20MHz mount failed, retrying at 10MHz...");
        if (!SD_MMC.begin("/sdcard", true, false, 10000)) {
            Serial.println("[SD] Warning: Failed to mount SD card. Check card insertion and FAT32 format.");
            return false;
        }
    }

    uint8_t cardType = SD_MMC.cardType();
    if (cardType == CARD_NONE) {
        Serial.println("[SD] Error: No SD card detected!");
        return false;
    }

    const char* typeStr = "UNKNOWN";
    if (cardType == CARD_MMC) typeStr = "MMC";
    else if (cardType == CARD_SD) typeStr = "SDSC";
    else if (cardType == CARD_SDHC) typeStr = "SDHC";

    Serial.printf("[SD] Mounted SD card successfully! Type: %s, Size: %u MB\n", typeStr, (uint32_t)(SD_MMC.cardSize() / (1024 * 1024)));

    _sdMounted = true;
    return true;
}


bool FlightDataManager::findAirport(const char* code, LatLon& outCoords, std::string& outName) {
    if (!code || strlen(code) < 3) return false;
    char upper[4];
    for (int i = 0; i < 3; i++) {
        char c = code[i];
        if (c >= 'a' && c <= 'z') c -= 32;
        upper[i] = c;
    }
    upper[3] = '\0';
    std::string sCode(upper);

    // 1. Check custom airports loaded from SD
    auto it = _customAirports.find(sCode);
    if (it != _customAirports.end()) {
        outCoords.lat = it->second.lat;
        outCoords.lon = it->second.lon;
        outName = it->second.name;
        return true;
    }

    // 2. Check built-in global IATA database
    float lat, lon;
    if (getAirportCoords(upper, lat, lon)) {
        outCoords.lat = lat;
        outCoords.lon = lon;
        outName = sCode;
        return true;
    }

    return false;
}

void FlightDataManager::loadCustomAirports(const char* filename) {
    if (!_sdMounted) return;
    File f = SD_MMC.open(filename, "r");
    if (!f) return;

    Serial.printf("[SD] Loading custom airports from %s...\n", filename);
    while (f.available()) {
        String line = f.readStringUntil('\n');
        line.trim();
        if (line.length() == 0 || line.startsWith("#") || line.startsWith("//")) continue;

        int firstComma = line.indexOf(',');
        if (firstComma == -1) continue;
        int secondComma = line.indexOf(',', firstComma + 1);
        if (secondComma == -1) continue;

        String codeStr = line.substring(0, firstComma);
        codeStr.trim();
        codeStr.toUpperCase();

        String latStr = line.substring(firstComma + 1, secondComma);
        latStr.trim();

        String lonStr, nameStr = "";
        int thirdComma = line.indexOf(',', secondComma + 1);
        if (thirdComma != -1) {
            lonStr = line.substring(secondComma + 1, thirdComma);
            nameStr = line.substring(thirdComma + 1);
            nameStr.trim();
        } else {
            lonStr = line.substring(secondComma + 1);
        }
        lonStr.trim();

        if (codeStr.length() == 3) {
            CustomAirport ca;
            ca.code = codeStr.c_str();
            ca.lat = latStr.toFloat();
            ca.lon = lonStr.toFloat();
            ca.name = nameStr.length() > 0 ? nameStr.c_str() : codeStr.c_str();
            _customAirports[ca.code] = ca;
            Serial.printf("[SD] Added custom airport %s (%.2f, %.2f)\n", ca.code.c_str(), ca.lat, ca.lon);
        }
    }
    f.close();
}

bool FlightDataManager::parseLine(const String& rawLine, String& outOrigin, String& outDest) {
    String line = rawLine;
    line.trim();
    if (line.length() < 7 || line.startsWith("#") || line.startsWith("//")) {
        return false;
    }

    // Replace common delimiters "->", "-", ",", space with a single separator
    line.replace("->", "-");
    line.replace(",", "-");
    line.replace(" ", "-");

    int dashIdx = line.indexOf('-');
    if (dashIdx == -1) return false;

    outOrigin = line.substring(0, dashIdx);
    outDest = line.substring(dashIdx + 1);

    outOrigin.trim();
    outDest.trim();
    outOrigin.toUpperCase();
    outDest.toUpperCase();

    // Strip trailing comments if any
    int commentIdx = outDest.indexOf('#');
    if (commentIdx != -1) {
        outDest = outDest.substring(0, commentIdx);
        outDest.trim();
    }

    return (outOrigin.length() == 3 && outDest.length() == 3);
}

bool FlightDataManager::loadFlights(const char* filename) {
    _routes.clear();
    _uniqueAirports.clear();
    _totalDistanceNM = 0.0f;
    _totalDistanceKM = 0.0f;
    _isDemoMode = false;

    if (!_sdMounted) {
        loadDemoFlights();
        return false;
    }

    loadCustomAirports("/airports.txt");
    loadCustomAirports("/airports.csv");

    File file = SD_MMC.open(filename, "r");
    if (!file) file = SD_MMC.open("/flights.txt", "r");
    if (!file) file = SD_MMC.open("/FLIGHTS.TXT", "r");
    if (!file) file = SD_MMC.open("/Flights.txt", "r");
    if (!file) file = SD_MMC.open("/flights.csv", "r");
    if (!file) file = SD_MMC.open("/FLIGHTS.CSV", "r");

    if (!file) {
        Serial.printf("[SD] Flight file '%s' not found on SD card.\n", filename);
        File root = SD_MMC.open("/");
        if (root) {
            Serial.println("[SD] Files found in root directory of SD card:");
            File entry = root.openNextFile();
            while (entry) {
                Serial.printf("[SD]   - %s (%d bytes)\n", entry.name(), (int)entry.size());
                entry = root.openNextFile();
            }
            root.close();
        }
        Serial.println("[SD] Loading demo flights instead.");
        loadDemoFlights();
        return false;
    }

    Serial.printf("[SD] Reading flights from %s...\n", filename);
    int lineNum = 0;
    int loaded = 0;

    while (file.available()) {
        String line = file.readStringUntil('\n');
        lineNum++;
        String origStr, destStr;
        if (!parseLine(line, origStr, destStr)) continue;

        LatLon origCoords, destCoords;
        std::string origName, destName;

        bool origOk = findAirport(origStr.c_str(), origCoords, origName);
        bool destOk = findAirport(destStr.c_str(), destCoords, destName);

        if (!origOk) {
            Serial.printf("[SD] Warning: Line %d unknown airport '%s'\n", lineNum, origStr.c_str());
            continue;
        }
        if (!destOk) {
            Serial.printf("[SD] Warning: Line %d unknown airport '%s'\n", lineNum, destStr.c_str());
            continue;
        }

        FlightRoute r;
        strncpy(r.origin, origStr.c_str(), 4);
        strncpy(r.dest, destStr.c_str(), 4);
        r.originCoords = origCoords;
        r.destCoords = destCoords;
        r.distanceNM = calcDistanceNM(origCoords.lat, origCoords.lon, destCoords.lat, destCoords.lon);
        r.distanceKM = calcDistanceKM(origCoords.lat, origCoords.lon, destCoords.lat, destCoords.lon);
        r.arcPoints = generateGreatCircleArc(origCoords, destCoords, 32, 0.06f);

        _routes.push_back(r);
        loaded++;
        Serial.printf("[SD] Added route #%d: %s -> %s (%.0f NM)\n", loaded, r.origin, r.dest, r.distanceNM);
    }
    file.close();

    if (_routes.empty()) {
        Serial.println("[SD] No valid routes found in file. Falling back to demo.");
        loadDemoFlights();
        return false;
    }

    updateStats();
    Serial.printf("[SD] Successfully loaded %d flight routes!\n", (int)_routes.size());
    return true;
}

void FlightDataManager::loadDemoFlights() {
    _isDemoMode = true;
    _routes.clear();
    Serial.println("[DEMO] Loading demo flight routes...");

    const char* demoPairs[][2] = {
        {"ORD", "CMI"},
        {"ORD", "SFO"},
        {"ORD", "JFK"},
        {"JFK", "LHR"},
        {"LHR", "HND"},
        {"HND", "SFO"},
        {"SFO", "ORD"}
    };

    for (size_t i = 0; i < sizeof(demoPairs) / sizeof(demoPairs[0]); i++) {
        const char* origStr = demoPairs[i][0];
        const char* destStr = demoPairs[i][1];

        LatLon origCoords, destCoords;
        std::string origName, destName;

        if (findAirport(origStr, origCoords, origName) && findAirport(destStr, destCoords, destName)) {
            FlightRoute r;
            strncpy(r.origin, origStr, 4);
            strncpy(r.dest, destStr, 4);
            r.originCoords = origCoords;
            r.destCoords = destCoords;
            r.distanceNM = calcDistanceNM(origCoords.lat, origCoords.lon, destCoords.lat, destCoords.lon);
            r.distanceKM = calcDistanceKM(origCoords.lat, origCoords.lon, destCoords.lat, destCoords.lon);
            r.arcPoints = generateGreatCircleArc(origCoords, destCoords, 32, 0.06f);
            _routes.push_back(r);
        }
    }

    updateStats();
}

void FlightDataManager::updateStats() {
    _totalDistanceNM = 0.0f;
    _totalDistanceKM = 0.0f;
    std::set<std::string> uniqueSet;

    for (const auto& r : _routes) {
        _totalDistanceNM += r.distanceNM;
        _totalDistanceKM += r.distanceKM;
        uniqueSet.insert(std::string(r.origin));
        uniqueSet.insert(std::string(r.dest));
    }

    _uniqueAirports.clear();
    for (const auto& code : uniqueSet) {
        _uniqueAirports.push_back(code);
    }
}
