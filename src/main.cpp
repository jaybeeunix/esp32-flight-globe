#include <Arduino.h>
#include "I2C_Driver.h"
#include "TCA9554PWR.h"
#include "Display_ST7701.h"
#include "Touch_GT911.h"
#include "board_pins.h"
#include "display_lgfx.h"
#include "flight_math.h"
#include "sd_reader.h"
#include "globe_renderer.h"

#define APP_VERSION "v1.2"

struct TouchPoint {
    int16_t x;
    int16_t y;
};

LGFX_Sprite canvas;

static int activeRouteIdx = 0;
static uint32_t lastFrameMs = 0;
static uint32_t lastTouchMs = 0;
static uint32_t lastRouteSwitchMs = 0;
static bool isDragging = false;
static int16_t lastTouchX = 0;
static int16_t touchStartX = 0;
static int16_t touchStartY = 0;
static uint32_t touchStartTime = 0;

static void showSplashScreen() {
    // Deep midnight space background
    canvas.fillScreen(canvas.color565(8, 14, 26));

    // Outer decorative celestial rings matching the round 480x480 screen
    canvas.drawCircle(240, 240, 235, canvas.color565(25, 45, 80));
    canvas.drawCircle(240, 240, 230, canvas.color565(18, 32, 60));
    canvas.drawCircle(240, 240, 195, canvas.color565(15, 28, 50));

    // Title: "Flights Globe"
    canvas.setTextDatum(lgfx::textdatum::middle_center);
    canvas.setTextSize(3);
    // Subtle drop shadow
    canvas.setTextColor(canvas.color565(0, 0, 0));
    canvas.drawString("Flights Globe", 241, 161);
    // Vibrant gold/amber title
    canvas.setTextColor(canvas.color565(255, 215, 60));
    canvas.drawString("Flights Globe", 240, 160);

    // Decorative divider line
    canvas.drawLine(150, 195, 330, 195, canvas.color565(40, 75, 130));
    canvas.fillCircle(240, 195, 3, canvas.color565(255, 215, 60));

    // Subtitle / Attributions
    canvas.setTextSize(2);
    canvas.setTextColor(canvas.color565(220, 230, 245));
    canvas.drawString("Envisioned by", 240, 235);
    canvas.setTextColor(canvas.color565(255, 255, 255));
    canvas.drawString("Jason Burrell", 240, 265);

    // Link
    canvas.setTextSize(2);
    canvas.setTextColor(canvas.color565(80, 175, 255)); // Soft electric cyan/blue
    canvas.drawString("jaybeeunix.github.io", 240, 310);

    // Version number below
    canvas.setTextSize(1);
    canvas.setTextColor(canvas.color565(110, 130, 160));
    canvas.drawString(APP_VERSION, 240, 360);

    // Push splash frame to display
    LCD_addWindow(0, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1, (uint8_t*)canvas.getBuffer());
}

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println();
    Serial.println("==========================================");
    Serial.println(" Waveshare ESP32-S3 Flight Tracker Globe  ");
    Serial.println(" Target: ESP32-S3-Touch-LCD-2.8C (480x480)");
    Serial.printf (" Version: %s\n", APP_VERSION);
    Serial.println("==========================================");

    // 1. Initialize Waveshare 2.8C I2C & IO Expander (TCA9554)
    Serial.println("[MAIN] Initializing I2C & TCA9554 (SDA=15, SCL=7)...");
    I2C_Init();
    TCA9554PWR_Init(0x00);
    Set_EXIO(EXIO_PIN8, Low); // Power latch

    // 2. Initialize ST7701 RGB Display + Backlight + GT911 Touch
    Serial.println("[MAIN] Initializing ST7701 Display (480x480 Round)... ");
    LCD_Init();
    Serial.println("[MAIN] ST7701 Display initialized!");

    // 3. Allocate full 480x480 double-buffer sprite in PSRAM
    Serial.printf("[MAIN] Free PSRAM before canvas: %u bytes\n", (unsigned)ESP.getFreePsram());
    canvas.setColorDepth(lgfx::color_depth_t::rgb565_nonswapped);
    canvas.setPsram(true);
    void* ptr = canvas.createSprite(SCREEN_WIDTH, SCREEN_HEIGHT);
    if (!ptr) {
        Serial.println("[MAIN] Error: PSRAM 480x480 sprite allocation failed!");
    } else {
        Serial.printf("[MAIN] Full 480x480 canvas allocated in PSRAM (Free PSRAM: %u bytes)\n", (unsigned)ESP.getFreePsram());
        // Render splash screen immediately so user sees it right at power-on
        showSplashScreen();
    }

    uint32_t splashStartMs = millis();

    // 4. Mount SD Card and load flights + config
    if (flightData.initSD()) {
        flightData.loadFlights("/flights.txt");
    } else {
        Serial.println("[MAIN] SD card not mounted. Loading demo flights.");
        flightData.loadDemoFlights();
    }

    // 5. Initialize 3D Globe Raycasting Engine (LUT precomputation)
    globe.init();
    const auto& routes = flightData.getRoutes();
    if (!routes.empty()) {
        globe.centerOnRoute(routes[0]);
    }

    // Ensure splash screen remains visible for at least 2.5 seconds total
    while (millis() - splashStartMs < 2500) {
        delay(20);
    }

    lastFrameMs = millis();
    lastTouchMs = millis();
    lastRouteSwitchMs = millis();
    Serial.printf("[MAIN] Setup complete. Auto-cycling routes every %u seconds.\n",
                  (unsigned int)flightData.getCycleIntervalSec());
}

void loop() {
    uint32_t now = millis();
    float dt = (now - lastFrameMs) / 1000.0f;
    lastFrameMs = now;
    if (dt <= 0.0f || dt > 0.2f) dt = 0.02f;

    const auto& routes = flightData.getRoutes();

    // -------------------------------------------------------------
    // Touch Interaction Handling (GT911)
    // -------------------------------------------------------------
    TouchPoint pt = {0, 0};
    bool touched = false;
    uint16_t tx[1] = {0}, ty[1] = {0}, strength[1] = {0};
    uint8_t point_cnt = 0;

    Touch_Read_Data();
    if (Touch_Get_XY(tx, ty, strength, &point_cnt, 1) && point_cnt > 0) {
        touched = true;
        // Direct 1:1 touch on 480x480 circular panel
        pt.x = (int16_t)tx[0];
        pt.y = (int16_t)ty[0];
    }

    if (touched) {
        lastTouchMs = now;
        lastRouteSwitchMs = now; // Reset cycle timer on touch

        if (!isDragging) {
            isDragging = true;
            lastTouchX = pt.x;
            touchStartX = pt.x;
            touchStartY = pt.y;
            touchStartTime = now;
        } else {
            int dx = pt.x - lastTouchX;
            lastTouchX = pt.x;

            // Drag anywhere to rotate the globe
            if (std::abs(dx) > 0) {
                float deltaLon = (float)dx * 0.6f;
                globe.spinBy(-deltaLon);
            }
        }
    } else {
        if (isDragging) {
            isDragging = false;
            uint32_t holdTime = now - touchStartTime;
            int moveDist = std::abs(touchStartX - lastTouchX);

            // Tap anywhere to immediately advance to next flight route
            if (holdTime < 350 && moveDist < 15) {
                if (!routes.empty()) {
                    activeRouteIdx = (activeRouteIdx + 1) % routes.size();
                    globe.centerOnRoute(routes[activeRouteIdx]);
                    lastRouteSwitchMs = now;
                }
            }
        }

        // Resume auto-rotation after 6 seconds of touch inactivity
        if (!globe.isAutoSpin() && (now - lastTouchMs > 6000)) {
            globe.setAutoSpin(true);
        }

        // -------------------------------------------------------------
        // Automatic Route Cycle Timer
        // -------------------------------------------------------------
        uint32_t cycleIntervalMs = flightData.getCycleIntervalSec() * 1000;
        if (cycleIntervalMs > 0 && !routes.empty()) {
            // Only auto-cycle if user hasn't touched the screen in the last 4 seconds
            if ((now - lastTouchMs > 4000) && (now - lastRouteSwitchMs >= cycleIntervalMs)) {
                activeRouteIdx = (activeRouteIdx + 1) % routes.size();
                globe.centerOnRoute(routes[activeRouteIdx]);
                lastRouteSwitchMs = now;
                Serial.printf("[MAIN] Auto-cycling to route #%d: %s\n",
                              activeRouteIdx + 1, routes[activeRouteIdx].itinerary.c_str());
            }
        }
    }

    // -------------------------------------------------------------
    // Update Animation & Simulation
    // -------------------------------------------------------------
    globe.update(dt);

    // -------------------------------------------------------------
    // Render Scene to Double Buffer
    // -------------------------------------------------------------
    // Deep space background
    canvas.fillScreen(canvas.color565(8, 12, 22));

    // Render full 480x480 3D textured Earth, flight arcs, and airport markers
    globe.render(canvas, activeRouteIdx);

    // Push full 480x480 frame to ST7701 RGB panel
    if (canvas.getBuffer()) {
        LCD_addWindow(0, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1, (uint8_t*)canvas.getBuffer());
    }

    // FreeRTOS cooperative yield
    vTaskDelay(pdMS_TO_TICKS(1));
}
