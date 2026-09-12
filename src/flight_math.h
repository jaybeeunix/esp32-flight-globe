#pragma once
#include <Arduino.h>
#include <vector>
#include <cmath>

struct LatLon {
    float lat;
    float lon;
};

struct Vec3 {
    float x;
    float y;
    float z;

    Vec3() : x(0), y(0), z(0) {}
    Vec3(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}

    float length() const {
        return std::sqrt(x * x + y * y + z * z);
    }

    Vec3 normalized() const {
        float l = length();
        if (l < 1e-6f) return Vec3(0, 0, 1);
        return Vec3(x / l, y / l, z / l);
    }

    static float dot(const Vec3& a, const Vec3& b) {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }
};

struct FlightRoute {
    char origin[4];
    char dest[4];
    LatLon originCoords;
    LatLon destCoords;
    float distanceNM;
    float distanceKM;
    std::vector<Vec3> arcPoints;
};

// Coordinate conversions
Vec3 latLonToVec3(float latDeg, float lonDeg, float radius = 1.0f);
void vec3ToLatLon(const Vec3& v, float& outLatDeg, float& outLonDeg);

// Haversine distance
float calcDistanceNM(float lat1, float lon1, float lat2, float lon2);
float calcDistanceKM(float lat1, float lon1, float lat2, float lon2);

// Slerp great circle arc points with parabolic altitude bulge
std::vector<Vec3> generateGreatCircleArc(const LatLon& p1, const LatLon& p2, int segments = 32, float arcBulge = 0.055f);
