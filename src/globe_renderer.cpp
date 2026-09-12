#include "globe_renderer.h"
#include "board_pins.h"
#include "earth_texture.h"
#include "sd_reader.h"
#include <algorithm>
#include <cmath>
#include <esp_heap_caps.h>

GlobeRenderer globe(240, 240, 230);

static constexpr float DEG_TO_RAD_F = 0.017453292519943295f;
static constexpr float RAD_TO_DEG_F = 57.29577951308232f;

GlobeRenderer::GlobeRenderer(int cx, int cy, int radius)
    : _cx(cx), _cy(cy), _radius(radius), _tiltDeg(20.0f),
      _currentLonDeg(-88.0f), _targetLonDeg(-88.0f), _autoSpin(true),
      _spinSpeed(8.5f), _pulsePhase(0.0f), _lut(nullptr), _lutCount(0) {}

GlobeRenderer::~GlobeRenderer() {
  if (_lut) {
    free(_lut);
    _lut = nullptr;
  }
}

bool GlobeRenderer::init() {
  Serial.printf("[GLOBE] Initializing 3D sphere raycaster (R=%d)...\n",
                _radius);
  precomputeLUT();
  return (_lut != nullptr);
}

void GlobeRenderer::precomputeLUT() {
  int R = _radius;
  int maxPixels = (2 * R + 1) * (2 * R + 1);

  // Try allocating in PSRAM first, fallback to internal RAM
  _lut = (SpherePixel *)heap_caps_malloc(maxPixels * sizeof(SpherePixel),
                                         MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!_lut) {
    _lut = (SpherePixel *)malloc(maxPixels * sizeof(SpherePixel));
  }

  if (!_lut) {
    Serial.println("[GLOBE] Error: Failed to allocate memory for sphere LUT!");
    return;
  }

  float tiltRad = _tiltDeg * DEG_TO_RAD_F;
  float cosT = std::cos(tiltRad);
  float sinT = std::sin(tiltRad);

  _lutCount = 0;
  int R2 = R * R;

  for (int dy = -R; dy <= R; dy++) {
    for (int dx = -R; dx <= R; dx++) {
      int r2 = dx * dx + dy * dy;
      if (r2 <= R2) {
        float z = std::sqrt((float)(R2 - r2));
        float nx = (float)dx / (float)R;
        float ny = -(float)dy / (float)R;
        float nz = z / (float)R;

        // Rotate by inverse fixed axial tilt
        float yRot = ny * cosT + nz * sinT;
        float zRot = -ny * sinT + nz * cosT;
        float xRot = nx;

        // Spherical coordinates
        float lat = std::asin(std::max(-1.0f, std::min(1.0f, yRot)));
        float lonRel = std::atan2(xRot, zRot);

        // Texture coordinates
        int v = (int)(((float)M_PI * 0.5f - lat) / (float)M_PI *
                      (float)EARTH_TEX_HEIGHT);
        v = std::max(0, std::min(EARTH_TEX_HEIGHT - 1, v));

        int uBase = (int)((lonRel + (float)M_PI) / (2.0f * (float)M_PI) *
                          (float)EARTH_TEX_WIDTH);
        uBase = (uBase % EARTH_TEX_WIDTH + EARTH_TEX_WIDTH) % EARTH_TEX_WIDTH;

        // Limb shading (darkens towards edges)
        float shadeFactor = 0.55f + 0.45f * nz;
        uint8_t shade = (uint8_t)(shadeFactor * 255.0f);

        // Atmosphere rim glow
        float rim = 1.0f - nz;
        rim = rim * rim * rim; // cubic falloff
        uint8_t rimGlow = (uint8_t)(rim * 255.0f);

        SpherePixel &sp = _lut[_lutCount++];
        sp.sx = _cx + dx;
        sp.sy = _cy + dy;
        sp.u_base = (uint16_t)uBase;
        sp.v = (uint8_t)v;
        sp.shade = shade;
        sp.rim_glow = rimGlow;
      }
    }
  }

  Serial.printf("[GLOBE] LUT generated: %u pixels (%u KB)\n",
                (unsigned int)_lutCount,
                (unsigned int)((_lutCount * sizeof(SpherePixel)) / 1024));
}

void GlobeRenderer::update(float dt) {
  _pulsePhase += dt * 1.8f;
  if (_pulsePhase > 1.0f)
    _pulsePhase -= 1.0f;

  if (_autoSpin) {
    _currentLonDeg -= _spinSpeed * dt;
    while (_currentLonDeg < -180.0f)
      _currentLonDeg += 360.0f;
    while (_currentLonDeg > 180.0f)
      _currentLonDeg -= 360.0f;
    _targetLonDeg = _currentLonDeg;
  } else {
    // Smooth interpolation towards target longitude
    float diff = _targetLonDeg - _currentLonDeg;
    while (diff > 180.0f)
      diff -= 360.0f;
    while (diff < -180.0f)
      diff += 360.0f;

    if (std::abs(diff) > 0.1f) {
      _currentLonDeg += diff * std::min(1.0f, dt * 6.0f);
      while (_currentLonDeg < -180.0f)
        _currentLonDeg += 360.0f;
      while (_currentLonDeg > 180.0f)
        _currentLonDeg -= 360.0f;
    } else {
      _currentLonDeg = _targetLonDeg;
    }
  }
}

void GlobeRenderer::setRotation(float lonDeg) {
  while (lonDeg > 180.0f)
    lonDeg -= 360.0f;
  while (lonDeg < -180.0f)
    lonDeg += 360.0f;
  _currentLonDeg = lonDeg;
  _targetLonDeg = lonDeg;
}

void GlobeRenderer::spinBy(float deltaDeg) {
  _targetLonDeg += deltaDeg;
  while (_targetLonDeg > 180.0f)
    _targetLonDeg -= 360.0f;
  while (_targetLonDeg < -180.0f)
    _targetLonDeg += 360.0f;
  _currentLonDeg = _targetLonDeg;
  _autoSpin = false;
}

void GlobeRenderer::centerOnRoute(const FlightRoute &route) {
  if (route.legs.empty())
    return;
  // Calculate average longitude and 3D centroid of all stops
  float sumX = 0, sumZ = 0;
  for (const auto &leg : route.legs) {
    Vec3 v1 = latLonToVec3(leg.originCoords.lat, leg.originCoords.lon);
    Vec3 v2 = latLonToVec3(leg.destCoords.lat, leg.destCoords.lon);
    sumX += v1.x + v2.x;
    sumZ += v1.z + v2.z;
  }
  if (std::abs(sumX) > 1e-4f || std::abs(sumZ) > 1e-4f) {
    float avgLon = std::atan2(sumX, sumZ) * RAD_TO_DEG_F;
    _targetLonDeg = avgLon;
  } else {
    _targetLonDeg = route.legs[0].originCoords.lon;
  }
  _autoSpin = false;
}

Vec3 GlobeRenderer::rotateToView(const Vec3 &pt, float lonDeg, float tiltDeg) {
  float rotLonRad = lonDeg * DEG_TO_RAD_F;
  float tiltRad = tiltDeg * DEG_TO_RAD_F;

  float cosL = std::cos(rotLonRad);
  float sinL = std::sin(rotLonRad);
  float cosT = std::cos(tiltRad);
  float sinT = std::sin(tiltRad);

  // Rotate around Y axis (longitude)
  float x1 = pt.x * cosL - pt.z * sinL;
  float y1 = pt.y;
  float z1 = pt.x * sinL + pt.z * cosL;

  // Rotate around X axis (tilt)
  float x2 = x1;
  float y2 = y1 * cosT - z1 * sinT;
  float z2 = y1 * sinT + z1 * cosT;

  return Vec3(x2, y2, z2);
}

bool GlobeRenderer::projectPoint(const Vec3 &pt, float lonDeg, float tiltDeg,
                                 int16_t &outX, int16_t &outY, float &outZ) {
  Vec3 v = rotateToView(pt, lonDeg, tiltDeg);
  outZ = v.z;
  outX = _cx + (int16_t)std::round(v.x * (float)_radius);
  outY = _cy - (int16_t)std::round(v.y * (float)_radius);
  return (v.z > -0.05f); // Visible hemisphere
}

void GlobeRenderer::render(LGFX_Sprite &target, int activeRouteIndex) {
  if (!_lut)
    return;
  uint16_t *fb = (uint16_t *)target.getBuffer();

  // 1. Render Textured Earth Sphere
  // Compute current texture U offset from longitude
  int uOffset =
      (int)std::round(_currentLonDeg * ((float)EARTH_TEX_WIDTH / 360.0f));
  uOffset = (uOffset % EARTH_TEX_WIDTH + EARTH_TEX_WIDTH) % EARTH_TEX_WIDTH;

  for (size_t i = 0; i < _lutCount; i++) {
    const SpherePixel &sp = _lut[i];
    int u = (sp.u_base + uOffset) % EARTH_TEX_WIDTH;
    uint16_t texColor =
        pgm_read_word(&EARTH_TEXTURE[sp.v * EARTH_TEX_WIDTH + u]);

    // Unpack RGB565
    uint32_t r = (texColor >> 11) & 0x1F;
    uint32_t g = (texColor >> 5) & 0x3F;
    uint32_t b = texColor & 0x1F;

    // Apply limb shading
    uint32_t shade = sp.shade;
    r = (r * shade) >> 8;
    g = (g * shade) >> 8;
    b = (b * shade) >> 8;

    // Add atmosphere rim glow (cyan/blue)
    if (sp.rim_glow > 10) {
      uint32_t glow = sp.rim_glow;
      b = std::min((uint32_t)31, (uint32_t)(b + (glow >> 4)));
      g = std::min((uint32_t)63, (uint32_t)(g + (glow >> 5)));
    }

    uint16_t finalColor = (uint16_t)((r << 11) | (g << 5) | b);
    if (fb) {
      fb[sp.sy * 480 + sp.sx] = finalColor;
    } else {
      target.writePixel(sp.sx, sp.sy, finalColor);
    }
  }

  // 2. Atmosphere Outer Halo Ring
  target.drawCircle(_cx, _cy, _radius + 1, target.color565(30, 80, 160));
  target.drawCircle(_cx, _cy, _radius + 2, target.color565(20, 50, 115));
  target.drawCircle(_cx, _cy, _radius + 3, target.color565(14, 32, 75));
  target.drawCircle(_cx, _cy, _radius + 4, target.color565(8, 18, 45));

  const auto &routes = flightData.getRoutes();
  if (routes.empty())
    return;

  // 3. Render Great Circle Flight Arcs
  for (size_t rIdx = 0; rIdx < routes.size(); rIdx++) {
    const auto &r = routes[rIdx];
    bool isActive = ((int)rIdx == activeRouteIndex);
    uint16_t arcColor = isActive ? target.color565(255, 220, 60)
                                 : target.color565(255, 140, 20);

    for (size_t lIdx = 0; lIdx < r.legs.size(); lIdx++) {
      const auto &leg = r.legs[lIdx];
      int16_t prevX = 0, prevY = 0;
      float prevZ = -1.0f;
      bool hasPrev = false;

      for (const auto &pt : leg.arcPoints) {
        int16_t sx, sy;
        float sz;
        bool vis = projectPoint(pt, _currentLonDeg, _tiltDeg, sx, sy, sz);

        if (vis && sz > 0.0f) {
          if (hasPrev && prevZ > 0.0f) {
            target.drawLine(prevX, prevY, sx, sy, arcColor);
            if (isActive) {
              // Double-stroke for active route
              target.drawLine(prevX + 1, prevY, sx + 1, sy, arcColor);
            }
          }
          prevX = sx;
          prevY = sy;
          prevZ = sz;
          hasPrev = true;
        } else {
          hasPrev = false;
        }
      }

      // Animated beacon pulse along active route
      if (isActive && !leg.arcPoints.empty()) {
        // Leg-staggered or sequenced pulse along active itinerary
        float numLegs = (float)r.legs.size();
        float totalT = std::fmod(_pulsePhase * numLegs, numLegs);
        int currentPulseLeg = (int)totalT;
        if (currentPulseLeg == (int)lIdx) {
          float legT = totalT - (float)currentPulseLeg;
          size_t segCount = leg.arcPoints.size() - 1;
          float segF = legT * (float)segCount;
          size_t idx0 = (size_t)segF;
          size_t idx1 = std::min(idx0 + 1, segCount);
          float frac = segF - (float)idx0;

          const Vec3 &p0 = leg.arcPoints[idx0];
          const Vec3 &p1 = leg.arcPoints[idx1];
          Vec3 pulsePt(p0.x + (p1.x - p0.x) * frac, p0.y + (p1.y - p0.y) * frac,
                       p0.z + (p1.z - p0.z) * frac);

          int16_t px, py;
          float pz;
          if (projectPoint(pulsePt, _currentLonDeg, _tiltDeg, px, py, pz) &&
              pz > 0.05f) {
            target.fillCircle(px, py, 3, target.color565(255, 255, 255));
            target.drawCircle(px, py, 4, target.color565(255, 220, 60));
          }
        }
      }
    }
  }

  // 4. Render Airport Markers & IATA Labels
  struct DrawnAirport {
    char code[4];
    int16_t x, y;
    bool active;
  };
  std::vector<DrawnAirport> drawnAirports;

  const auto &uniqueAirports = flightData.getUniqueAirports();
  for (const auto &codeStr : uniqueAirports) {
    LatLon coords;
    std::string name;
    if (!flightData.findAirport(codeStr.c_str(), coords, name))
      continue;

    Vec3 unitPt = latLonToVec3(coords.lat, coords.lon, 1.0f);
    int16_t sx, sy;
    float sz;
    if (projectPoint(unitPt, _currentLonDeg, _tiltDeg, sx, sy, sz) &&
        sz > 0.08f) {
      bool isActiveAirport = false;
      if (activeRouteIndex >= 0 && activeRouteIndex < (int)routes.size()) {
        const auto &cur = routes[activeRouteIndex];
        for (const auto &stop : cur.stopCodes) {
          if (stop == codeStr) {
            isActiveAirport = true;
            break;
          }
        }
      }

      DrawnAirport da;
      strncpy(da.code, codeStr.c_str(), 4);
      da.x = sx;
      da.y = sy;
      da.active = isActiveAirport;
      drawnAirports.push_back(da);
    }
  }

  // Draw markers
  for (const auto &a : drawnAirports) {
    if (a.active) {
      target.fillCircle(a.x, a.y, 4, target.color565(255, 220, 40));
      target.fillCircle(a.x, a.y, 2, target.color565(255, 255, 255));
      target.drawCircle(a.x, a.y, 5, target.color565(255, 60, 40));
    } else {
      target.fillCircle(a.x, a.y, 3, target.color565(255, 50, 50));
      target.fillCircle(a.x, a.y, 1, target.color565(255, 255, 255));
    }
  }

  // Draw labels with smart collision offset
  target.setTextColor(target.color565(255, 255, 255));
  target.setTextSize(1);

  for (size_t i = 0; i < drawnAirports.size(); i++) {
    const auto &a = drawnAirports[i];
    int offX = 5;
    int offY = -4;

    // Check proximity to other airports to avoid label overlap
    for (size_t j = 0; j < drawnAirports.size(); j++) {
      if (i == j)
        continue;
      int dx = a.x - drawnAirports[j].x;
      int dy = a.y - drawnAirports[j].y;
      if (std::abs(dx) < 18 && std::abs(dy) < 16) {
        if (a.y > drawnAirports[j].y) {
          offY = 5;
        } else {
          offY = -12;
        }
        break;
      }
    }

    int labelX = a.x + offX;
    int labelY = a.y + offY;

    // Keep labels on screen
    if (labelX > SCREEN_WIDTH - 25)
      labelX = a.x - 24;
    if (labelY < 4)
      labelY = 4;
    if (labelY > SCREEN_HEIGHT - 12)
      labelY = SCREEN_HEIGHT - 12;

    // Drop shadow for crisp contrast over land/water
    target.setTextColor(target.color565(0, 0, 0));
    target.drawString(a.code, labelX + 1, labelY + 1);
    target.setTextColor(a.active ? target.color565(255, 230, 80)
                                 : target.color565(240, 245, 255));
    target.drawString(a.code, labelX, labelY);
  }

  // 5. Active Itinerary Info Banner (bottom of 480x480 round screen)
  if (activeRouteIndex >= 0 && activeRouteIndex < (int)routes.size()) {
    const auto &cur = routes[activeRouteIndex];
    char banner[128];
    snprintf(banner, sizeof(banner), "%s   %.0f NM", cur.itinerary.c_str(),
             cur.totalDistanceNM);
    target.setTextDatum(lgfx::textdatum::middle_center);

    // Use textSize 2 for shorter itineraries, textSize 1 if longer than 22
    // chars
    if (strlen(banner) > 22) {
      target.setTextSize(1);
    } else {
      target.setTextSize(2);
    }

    // Semi-transparent pill drop shadow
    target.setTextColor(target.color565(0, 0, 0));
    target.drawString(banner, _cx + 1, 443);
    target.setTextColor(target.color565(255, 225, 70));
    target.drawString(banner, _cx, 442);
    target.setTextDatum(lgfx::textdatum::top_left);
    target.setTextSize(1);
  }
}
