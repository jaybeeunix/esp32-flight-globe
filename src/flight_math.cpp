#include "flight_math.h"

static constexpr float DEG_TO_RAD_F = 0.017453292519943295f;
static constexpr float RAD_TO_DEG_F = 57.29577951308232f;
static constexpr float EARTH_RADIUS_NM = 3440.065f;
static constexpr float EARTH_RADIUS_KM = 6371.0f;

Vec3 latLonToVec3(float latDeg, float lonDeg, float radius) {
    float phi = latDeg * DEG_TO_RAD_F;
    float lam = lonDeg * DEG_TO_RAD_F;
    float cosPhi = std::cos(phi);
    return Vec3(
        radius * cosPhi * std::sin(lam),
        radius * std::sin(phi),
        radius * cosPhi * std::cos(lam)
    );
}

void vec3ToLatLon(const Vec3& v, float& outLatDeg, float& outLonDeg) {
    Vec3 n = v.normalized();
    outLatDeg = std::asin(std::max(-1.0f, std::min(1.0f, n.y))) * RAD_TO_DEG_F;
    outLonDeg = std::atan2(n.x, n.z) * RAD_TO_DEG_F;
}

float calcDistanceNM(float lat1, float lon1, float lat2, float lon2) {
    float dLat = (lat2 - lat1) * DEG_TO_RAD_F;
    float dLon = (lon2 - lon1) * DEG_TO_RAD_F;
    float lat1R = lat1 * DEG_TO_RAD_F;
    float lat2R = lat2 * DEG_TO_RAD_F;

    float a = std::sin(dLat * 0.5f) * std::sin(dLat * 0.5f) +
              std::cos(lat1R) * std::cos(lat2R) *
              std::sin(dLon * 0.5f) * std::sin(dLon * 0.5f);
    float c = 2.0f * std::atan2(std::sqrt(a), std::sqrt(1.0f - a));
    return EARTH_RADIUS_NM * c;
}

float calcDistanceKM(float lat1, float lon1, float lat2, float lon2) {
    return calcDistanceNM(lat1, lon1, lat2, lon2) * 1.852f;
}

std::vector<Vec3> generateGreatCircleArc(const LatLon& p1, const LatLon& p2, int segments, float arcBulge) {
    std::vector<Vec3> points;
    if (segments < 2) segments = 2;
    points.reserve(segments + 1);

    Vec3 v1 = latLonToVec3(p1.lat, p1.lon, 1.0f);
    Vec3 v2 = latLonToVec3(p2.lat, p2.lon, 1.0f);

    float dot = std::max(-1.0f, std::min(1.0f, Vec3::dot(v1, v2)));
    float omega = std::acos(dot);
    float sinOmega = std::sin(omega);

    if (sinOmega < 1e-4f) {
        // Coincident or antipodal points
        points.push_back(v1);
        points.push_back(v2);
        return points;
    }

    for (int i = 0; i <= segments; i++) {
        float t = (float)i / (float)segments;
        float s1 = std::sin((1.0f - t) * omega) / sinOmega;
        float s2 = std::sin(t * omega) / sinOmega;

        Vec3 interp(
            s1 * v1.x + s2 * v2.x,
            s1 * v1.y + s2 * v2.y,
            s1 * v1.z + s2 * v2.z
        );

        // Elevate points along trajectory (sinusoidal altitude bulge)
        float altMultiplier = 1.0f + arcBulge * std::sin((float)M_PI * t);
        Vec3 elevated = interp.normalized();
        elevated.x *= altMultiplier;
        elevated.y *= altMultiplier;
        elevated.z *= altMultiplier;

        points.push_back(elevated);
    }

    return points;
}
