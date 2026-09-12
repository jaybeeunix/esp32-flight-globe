#include "sd_reader.h"
#include "board_pins.h"
#include "airports_data.h"
#include "TCA9554PWR.h"
#include <FS.h>
#include <SD_MMC.h>
#include <set>

FlightDataManager flightData;

FlightDataManager::FlightDataManager()
    : _sdMounted(false), _isDemoMode(false), _totalDistanceNM(0.0f), _totalDistanceKM(0.0f),
      _cycleIntervalSec(10) {}

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

void FlightDataManager::loadConfig(const char* filename) {
    if (!_sdMounted) return;

    File f = SD_MMC.open(filename, "r");
    if (!f) f = SD_MMC.open("/config.txt", "r");
    if (!f) f = SD_MMC.open("/CONFIG.TXT", "r");
    if (!f) f = SD_MMC.open("/Config.txt", "r");
    if (!f) f = SD_MMC.open("/config.ini", "r");
    if (!f) f = SD_MMC.open("/CONFIG.INI", "r");

    if (!f) {
        Serial.printf("[CONFIG] No %s found. Using default cycle time: %u seconds.\n", filename, (unsigned int)_cycleIntervalSec);
        return;
    }

    Serial.printf("[CONFIG] Reading configuration from %s...\n", filename);
    while (f.available()) {
        String line = f.readStringUntil('\n');
        line.trim();
        if (line.length() == 0 || line.startsWith("#") || line.startsWith("//") || line.startsWith(";")) {
            continue;
        }

        int eqIdx = line.indexOf('=');
        if (eqIdx == -1) eqIdx = line.indexOf(':');

        if (eqIdx != -1) {
            String key = line.substring(0, eqIdx);
            String val = line.substring(eqIdx + 1);
            key.trim();
            val.trim();
            key.toLowerCase();

            if (key == "cycle_time" || key == "cycle_interval" || key == "cycletime" || key == "interval" || key == "time") {
                int sec = val.toInt();
                if (sec > 0) {
                    _cycleIntervalSec = (uint32_t)sec;
                    Serial.printf("[CONFIG] Set route cycle interval to %u seconds.\n", (unsigned int)_cycleIntervalSec);
                }
            }
        }
    }
    f.close();
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

bool FlightDataManager::parseLineToHops(const String& rawLine, std::vector<std::string>& outHops) {
    outHops.clear();
    String line = rawLine;
    line.trim();
    if (line.length() < 3 || line.startsWith("#") || line.startsWith("//")) {
        return false;
    }

    // Strip comments (# or ;)
    int commentIdx = line.indexOf('#');
    if (commentIdx != -1) line = line.substring(0, commentIdx);
    commentIdx = line.indexOf(';');
    if (commentIdx != -1) line = line.substring(0, commentIdx);
    line.trim();
    if (line.length() < 3) return false;

    // Normalize delimiters: "->", ">", ",", tabs, spaces to '-'
    line.replace("->", "-");
    line.replace(">", "-");
    line.replace(",", "-");
    line.replace("\t", "-");
    line.replace(" ", "-");

    int start = 0;
    int len = line.length();
    while (start < len) {
        int dashIdx = line.indexOf('-', start);
        String token;
        if (dashIdx == -1) {
            token = line.substring(start);
            start = len;
        } else {
            token = line.substring(start, dashIdx);
            start = dashIdx + 1;
        }
        token.trim();
        token.toUpperCase();
        if (token.length() == 0) continue;

        // Valid 3-letter IATA alphabetic code
        if (token.length() == 3 && isAlpha(token[0]) && isAlpha(token[1]) && isAlpha(token[2])) {
            outHops.push_back(std::string(token.c_str()));
        } else {
            outHops.clear();
            return false;
        }
    }

    return (outHops.size() >= 2);
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

    loadConfig("/config.txt");
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
        std::vector<std::string> hops;
        if (!parseLineToHops(line, hops)) continue;

        // Build route with all legs
        FlightRoute r;
        r.stopCodes = hops;
        r.totalDistanceNM = 0.0f;
        r.totalDistanceKM = 0.0f;

        strncpy(r.origin, hops.front().c_str(), 4);
        strncpy(r.dest, hops.back().c_str(), 4);

        // Build itinerary string e.g. "BMI-DFW-HKG-DFW-BMI"
        std::string itinStr = "";
        for (size_t i = 0; i < hops.size(); i++) {
            if (i > 0) itinStr += "-";
            itinStr += hops[i];
        }
        r.itinerary = itinStr;

        bool allLegsOk = true;
        for (size_t i = 0; i + 1 < hops.size(); i++) {
            const std::string& origCode = hops[i];
            const std::string& destCode = hops[i + 1];

            LatLon origCoords, destCoords;
            std::string origName, destName;
            if (!findAirport(origCode.c_str(), origCoords, origName)) {
                Serial.printf("[SD] Warning: Line %d unknown airport '%s'\n", lineNum, origCode.c_str());
                allLegsOk = false;
                break;
            }
            if (!findAirport(destCode.c_str(), destCoords, destName)) {
                Serial.printf("[SD] Warning: Line %d unknown airport '%s'\n", lineNum, destCode.c_str());
                allLegsOk = false;
                break;
            }

            FlightLeg leg;
            strncpy(leg.origin, origCode.c_str(), 4);
            strncpy(leg.dest, destCode.c_str(), 4);
            leg.originCoords = origCoords;
            leg.destCoords = destCoords;
            leg.distanceNM = calcDistanceNM(origCoords.lat, origCoords.lon, destCoords.lat, destCoords.lon);
            leg.distanceKM = calcDistanceKM(origCoords.lat, origCoords.lon, destCoords.lat, destCoords.lon);
            leg.arcPoints = generateGreatCircleArc(origCoords, destCoords, 32, 0.06f);

            r.totalDistanceNM += leg.distanceNM;
            r.totalDistanceKM += leg.distanceKM;
            r.legs.push_back(leg);
        }

        r.distanceNM = r.totalDistanceNM;
        r.distanceKM = r.totalDistanceKM;

        if (allLegsOk && !r.legs.empty()) {
            _routes.push_back(r);
            loaded++;
            Serial.printf("[SD] Added route #%d: %s (%d legs, %.0f NM)\n",
                          loaded, r.itinerary.c_str(), (int)r.legs.size(), r.totalDistanceNM);
        }
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

    const char* demoItineraries[] = {
        "BMI-DFW-HKG-DFW-BMI",
        "ORD-SFO-HND-LHR-ORD",
        "ORD-MIA-LIM-CUZ-LIM-ORD",
        "ORD-ZRH-NCE-ZRH-ORD",
        "ORD-DOH-KTM-DOH-ORD",
        "ORD-PHX-HNL-KOA-HNL-ORD",
        "LAX-SYD-CNS-SYD-LAX"
    };

    for (size_t i = 0; i < sizeof(demoItineraries) / sizeof(demoItineraries[0]); i++) {
        std::vector<std::string> hops;
        if (!parseLineToHops(String(demoItineraries[i]), hops)) continue;

        FlightRoute r;
        r.stopCodes = hops;
        r.itinerary = demoItineraries[i];
        r.totalDistanceNM = 0.0f;
        r.totalDistanceKM = 0.0f;
        strncpy(r.origin, hops.front().c_str(), 4);
        strncpy(r.dest, hops.back().c_str(), 4);

        bool ok = true;
        for (size_t j = 0; j + 1 < hops.size(); j++) {
            LatLon origCoords, destCoords;
            std::string origName, destName;
            if (findAirport(hops[j].c_str(), origCoords, origName) &&
                findAirport(hops[j + 1].c_str(), destCoords, destName)) {
                FlightLeg leg;
                strncpy(leg.origin, hops[j].c_str(), 4);
                strncpy(leg.dest, hops[j + 1].c_str(), 4);
                leg.originCoords = origCoords;
                leg.destCoords = destCoords;
                leg.distanceNM = calcDistanceNM(origCoords.lat, origCoords.lon, destCoords.lat, destCoords.lon);
                leg.distanceKM = calcDistanceKM(origCoords.lat, origCoords.lon, destCoords.lat, destCoords.lon);
                leg.arcPoints = generateGreatCircleArc(origCoords, destCoords, 32, 0.06f);

                r.totalDistanceNM += leg.distanceNM;
                r.totalDistanceKM += leg.distanceKM;
                r.legs.push_back(leg);
            } else {
                ok = false;
                break;
            }
        }
        r.distanceNM = r.totalDistanceNM;
        r.distanceKM = r.totalDistanceKM;

        if (ok && !r.legs.empty()) {
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
        _totalDistanceNM += r.totalDistanceNM;
        _totalDistanceKM += r.totalDistanceKM;
        for (const auto& code : r.stopCodes) {
            uniqueSet.insert(code);
        }
    }

    _uniqueAirports.clear();
    for (const auto& code : uniqueSet) {
        _uniqueAirports.push_back(code);
    }
}
