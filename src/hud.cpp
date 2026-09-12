#include "hud.h"
#include "sd_reader.h"
#include "board_pins.h"

HudManager hud;

HudManager::HudManager()
    : _useKm(false),
      _boxX(224), _boxY(6), _boxW(90), _boxH(228),
      _routeBtnY(138), _routeBtnH(50),
      _spinBtnY(194), _spinBtnH(28) {}

void HudManager::render(LGFX_Sprite& target, int activeRouteIndex, bool autoSpin) {
    // 1. HUD Card Background
    uint16_t cardBg = target.color565(14, 20, 32);
    uint16_t borderCol = target.color565(36, 68, 110);
    target.fillRoundRect(_boxX, _boxY, _boxW, _boxH, 5, cardBg);
    target.drawRoundRect(_boxX, _boxY, _boxW, _boxH, 5, borderCol);

    // 2. Title & Status Badge
    target.setTextColor(target.color565(80, 180, 255));
    target.setTextSize(1);
    target.drawString("FLIGHT LOG", _boxX + 8, _boxY + 8);

    bool demo = flightData.isDemoMode();
    uint16_t badgeBg = demo ? target.color565(180, 90, 20) : target.color565(20, 140, 60);
    target.fillRoundRect(_boxX + 8, _boxY + 22, _boxW - 16, 12, 3, badgeBg);
    target.setTextColor(target.color565(255, 255, 255));
    target.drawString(demo ? "DEMO MODE" : "SD CARD OK", _boxX + 12, _boxY + 24);

    target.drawFastHLine(_boxX + 8, _boxY + 38, _boxW - 16, target.color565(28, 50, 80));

    // 3. Stats: Routes & Airports
    target.setTextColor(target.color565(130, 150, 180));
    target.drawString("ROUTES", _boxX + 8, _boxY + 44);
    target.setTextColor(target.color565(255, 255, 255));
    target.drawString(String((int)flightData.getRouteCount()), _boxX + 8, _boxY + 54);

    target.setTextColor(target.color565(130, 150, 180));
    target.drawString("AIRPORTS", _boxX + 8, _boxY + 70);
    target.setTextColor(target.color565(255, 255, 255));
    target.drawString(String((int)flightData.getUniqueAirports().size()), _boxX + 8, _boxY + 80);

    // 4. Distance
    target.setTextColor(target.color565(130, 150, 180));
    target.drawString("DISTANCE", _boxX + 8, _boxY + 96);
    float dist = _useKm ? flightData.getTotalDistanceKM() : flightData.getTotalDistanceNM();
    const char* unitStr = _useKm ? "KM" : "NM";
    char distBuf[24];
    if (dist >= 1000.0f) {
        snprintf(distBuf, sizeof(distBuf), "%.0f %s", dist, unitStr);
    } else {
        snprintf(distBuf, sizeof(distBuf), "%.1f %s", dist, unitStr);
    }
    target.setTextColor(target.color565(255, 205, 50));
    target.drawString(distBuf, _boxX + 8, _boxY + 106);

    target.drawFastHLine(_boxX + 8, _boxY + 124, _boxW - 16, target.color565(28, 50, 80));

    // 5. Active Route Card (Interactive touch target)
    const auto& routes = flightData.getRoutes();
    uint16_t subCardBg = target.color565(20, 30, 48);
    target.fillRoundRect(_boxX + 6, _routeBtnY, _boxW - 12, _routeBtnH, 4, subCardBg);
    target.drawRoundRect(_boxX + 6, _routeBtnY, _boxW - 12, _routeBtnH, 4, target.color565(50, 90, 150));

    target.setTextColor(target.color565(100, 180, 255));
    target.drawString("ACTIVE ROUTE", _boxX + 10, _routeBtnY + 4);

    if (!routes.empty() && activeRouteIndex >= 0 && activeRouteIndex < (int)routes.size()) {
        const auto& cur = routes[activeRouteIndex];
        char pairBuf[16];
        snprintf(pairBuf, sizeof(pairBuf), "%s > %s", cur.origin, cur.dest);
        target.setTextColor(target.color565(255, 225, 60));
        target.drawString(pairBuf, _boxX + 10, _routeBtnY + 18);

        float legDist = _useKm ? cur.distanceKM : cur.distanceNM;
        char legBuf[16];
        snprintf(legBuf, sizeof(legBuf), "%.0f %s", legDist, unitStr);
        target.setTextColor(target.color565(180, 210, 240));
        target.drawString(legBuf, _boxX + 10, _routeBtnY + 32);
    } else {
        target.setTextColor(target.color565(160, 160, 160));
        target.drawString("NO FLIGHTS", _boxX + 10, _routeBtnY + 20);
    }

    // 6. Auto-spin Toggle Button
    uint16_t spinBg = autoSpin ? target.color565(24, 52, 90) : target.color565(40, 44, 52);
    target.fillRoundRect(_boxX + 6, _spinBtnY, _boxW - 12, _spinBtnH, 4, spinBg);
    target.drawRoundRect(_boxX + 6, _spinBtnY, _boxW - 12, _spinBtnH, 4, target.color565(60, 110, 180));
    target.setTextColor(autoSpin ? target.color565(120, 220, 255) : target.color565(180, 180, 180));
    target.drawString(autoSpin ? "AUTO: ON" : "AUTO: OFF", _boxX + 12, _spinBtnY + 5);
    target.setTextColor(target.color565(100, 120, 150));
    target.drawString("TAP TO SPIN", _boxX + 12, _spinBtnY + 16);
}

bool HudManager::handleTouch(int16_t x, int16_t y, int& inOutActiveRoute, bool& inOutAutoSpin) {
    if (x < _boxX || x > _boxX + _boxW || y < _boxY || y > _boxY + _boxH) {
        return false;
    }

    // 1. Tap on Active Route card -> cycle to next route
    if (y >= _routeBtnY && y <= _routeBtnY + _routeBtnH) {
        size_t total = flightData.getRouteCount();
        if (total > 0) {
            inOutActiveRoute = (inOutActiveRoute + 1) % total;
            Serial.printf("[HUD] Switched to route #%d\n", inOutActiveRoute);
        }
        return true;
    }

    // 2. Tap on Auto-spin button -> toggle auto-rotation
    if (y >= _spinBtnY && y <= _spinBtnY + _spinBtnH) {
        inOutAutoSpin = !inOutAutoSpin;
        Serial.printf("[HUD] Auto-spin set to %s\n", inOutAutoSpin ? "ON" : "OFF");
        return true;
    }

    // 3. Tap on Distance area -> toggle units (NM <-> KM)
    if (y >= _boxY + 96 && y <= _boxY + 124) {
        toggleUnits();
        Serial.printf("[HUD] Distance units toggled: %s\n", _useKm ? "KM" : "NM");
        return true;
    }

    return true;
}
