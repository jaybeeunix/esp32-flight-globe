#pragma once
#include <Arduino.h>
#include "display_lgfx.h"
#include "flight_math.h"

class HudManager {
public:
    HudManager();
    void render(LGFX_Sprite& target, int activeRouteIndex, bool autoSpin);
    bool handleTouch(int16_t x, int16_t y, int& inOutActiveRoute, bool& inOutAutoSpin);
    void toggleUnits() { _useKm = !_useKm; }

private:
    bool _useKm;
    // Bounding boxes for touch targets
    int _boxX, _boxY, _boxW, _boxH;
    int _routeBtnY, _routeBtnH;
    int _spinBtnY, _spinBtnH;
};

extern HudManager hud;
