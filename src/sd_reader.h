#pragma once
#include <Arduino.h>
#include <vector>
#include <map>
#include <string>
#include "flight_math.h"

struct CustomAirport {
    std::string code;
    float lat;
    float lon;
    std::string name;
};

class FlightDataManager {
public:
    FlightDataManager();
    bool initSD();
    bool loadFlights(const char* filename = "/flights.txt");
    void loadDemoFlights();

    const std::vector<FlightRoute>& getRoutes() const { return _routes; }
    const std::vector<std::string>& getUniqueAirports() const { return _uniqueAirports; }
    float getTotalDistanceNM() const { return _totalDistanceNM; }
    float getTotalDistanceKM() const { return _totalDistanceKM; }
    bool isDemoMode() const { return _isDemoMode; }
    bool isSdMounted() const { return _sdMounted; }
    size_t getRouteCount() const { return _routes.size(); }

    uint32_t getCycleIntervalSec() const { return _cycleIntervalSec; }
    void setCycleIntervalSec(uint32_t sec) { _cycleIntervalSec = sec; }

    bool findAirport(const char* code, LatLon& outCoords, std::string& outName);

private:
    bool _sdMounted;
    bool _isDemoMode;
    std::vector<FlightRoute> _routes;
    std::vector<std::string> _uniqueAirports;
    std::map<std::string, CustomAirport> _customAirports;
    float _totalDistanceNM;
    float _totalDistanceKM;
    uint32_t _cycleIntervalSec;

    void loadConfig(const char* filename = "/config.txt");
    void loadCustomAirports(const char* filename = "/airports.txt");
    void updateStats();
    bool parseLineToHops(const String& rawLine, std::vector<std::string>& outHops);
};

extern FlightDataManager flightData;
