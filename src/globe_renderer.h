#pragma once
#include <Arduino.h>
#include <vector>
#include "display_lgfx.h"
#include "flight_math.h"

struct SpherePixel {
    int16_t sx;
    int16_t sy;
    uint16_t u_base;
    uint8_t v;
    uint8_t shade;      // 0..255 illumination multiplier
    uint8_t rim_glow;   // 0..255 atmospheric edge glow
};

class GlobeRenderer {
public:
    GlobeRenderer(int cx = 240, int cy = 240, int radius = 230);
    ~GlobeRenderer();

    bool init();
    void update(float dt);
    void render(LGFX_Sprite& target, int activeRouteIndex = 0);

    // Rotation controls
    void setRotation(float lonDeg);
    void spinBy(float deltaDeg);
    void setAutoSpin(bool enable) { _autoSpin = enable; }
    bool isAutoSpin() const { return _autoSpin; }
    void centerOnRoute(const FlightRoute& route);
    float getRotationDeg() const { return _currentLonDeg; }

    int getGlobeCenterX() const { return _cx; }
    int getGlobeCenterY() const { return _cy; }
    int getGlobeRadius() const { return _radius; }

private:
    int _cx;
    int _cy;
    int _radius;
    float _tiltDeg;
    float _currentLonDeg;
    float _targetLonDeg;
    bool _autoSpin;
    float _spinSpeed; // deg/sec
    float _pulsePhase;

    SpherePixel* _lut;
    size_t _lutCount;

    void precomputeLUT();
    Vec3 rotateToView(const Vec3& pt, float lonDeg, float tiltDeg);
    bool projectPoint(const Vec3& pt, float lonDeg, float tiltDeg, int16_t& outX, int16_t& outY, float& outZ);
};

extern GlobeRenderer globe;
