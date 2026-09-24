# ✈️ ESP32-S3 Flight Globe

An interactive 3D textured flight tracking globe and itinerary visualizer for the **Waveshare ESP32-S3 2.8" Round Touch LCD** (`ESP32-S3-Touch-LCD-2.8C`, 480×480 ST7701 RGB Display with GT911 capacitive touch).

Visualizes multi-hop flight routes with realistic 3D great circle arcs, smooth globe rotation, animated flight pulses, airport markers, and distance calculations—all rendered in real time on an ESP32-S3 using an optimized raycasting look-up table (LUT) and double-buffered PSRAM sprites.

---

## 🌟 Features

- **🌍 Real-Time 3D Raycasted Earth**:
  - Precomputed sphere raycaster Look-Up Table (LUT) with a realistic 20° axial tilt.
  - Spherical limb shading (edge darkening) and atmospheric rim glow (celestial cyan halo).
  - 720×360 high-resolution RGB565 Earth texture map derived from Natural Earth 50m vector datasets.
  - Smooth double-buffered rendering using a 480×480 16-bit PSRAM canvas (powered by LovyanGFX).

- **✈️ Multi-Hop Flight Routes & Great Circle Arcs**:
  - Full support for multi-leg itineraries (e.g. `BMI-DFW-HKG-DFW-BMI`, `ORD-SFO-HND-LHR-ORD`).
  - Spherical linear interpolation (Slerp) with parabolic altitude bulge to render realistic curved flight arcs floating above the planet.
  - Animated glowing beacon pulses traversing each leg of the active route in sequence.
  - Automatic 3D centroid calculation to smoothly glide and center the camera on the active itinerary.
  - Total distance calculation in both Nautical Miles (NM) and Kilometers (KM).

- **📍 Global Airport Database & Custom Code Support**:
  - Built-in PROGMEM database with binary search lookup for thousands of commercial IATA airport codes (`src/airports_data.h`).
  - Support for custom airfields, regional strips, and user-defined coordinates via `airports.txt` on the MicroSD card.
  - Dynamic collision detection and smart offset positioning for airport code labels on the round display.

- **👆 Interactive Touch Controls**:
  - **Drag / Swipe**: Manually spin and explore the globe along its longitude axis.
  - **Tap**: Instantly jump to the next itinerary in the playlist.
  - **Auto-Spin**: Automatically resumes smooth planetary rotation after a brief period of inactivity.
  - **Auto-Cycle Timer**: Periodically rotates through loaded flight itineraries (configurable via SD card).

- **💾 MicroSD & Offline Operation**:
  - Reads routes, configuration, and custom airports directly from a FAT32 MicroSD card via 1-bit `SD_MMC`.
  - Seamless fallback to built-in demo flight routes if no SD card is detected or files are missing.

---

## 🖥️ Target Hardware

This project is tailored specifically for the **[Waveshare ESP32-S3-Touch-LCD-2.8C](https://www.waveshare.com/wiki/ESP32-S3-Touch-LCD-2.8C)** round display module:

| Component | Specification |
| :--- | :--- |
| **MCU** | ESP32-S3 Dual-Core Xtensa LX7 @ 240 MHz |
| **Flash / PSRAM** | 16 MB Flash (Quad/Octal), 8 MB Octal PSRAM (OPI) |
| **Display** | 2.8" Round IPS LCD, 480×480 pixels, 16-bit RGB565 |
| **Display Driver** | Sitronix ST7701 (16-bit parallel RGB interface) |
| **Touch Controller**| Goodix GT911 Capacitive Touch (I2C) |
| **IO Expander** | TCA9554PWR (I2C Address `0x00`) |
| **Storage** | MicroSD slot connected via 1-bit `SD_MMC` |

### Pin Assignments Summary

| Bus / Function | Pins / Signals |
| :--- | :--- |
| **I2C Bus** | `SDA = GPIO 15`, `SCL = GPIO 7` (TCA9554, GT911) |
| **RGB LCD Timing** | `HSYNC = 38`, `VSYNC = 39`, `DE = 40`, `PCLK = 41` |
| **RGB LCD Data (16-bit)** | `D0..D15 = [5, 45, 48, 47, 21, 14, 13, 12, 11, 10, 9, 46, 3, 8, 18, 17]` |
| **LCD SPI 3-wire Config** | `CLK = GPIO 2`, `MOSI = GPIO 1` |
| **Backlight PWM** | `GPIO 6` (20 kHz PWM) |
| **MicroSD (SD_MMC 1-bit)** | `CLK = GPIO 2`, `CMD = GPIO 1`, `D0 = GPIO 42` (Power switch on `EXIO4`) |
| **TCA9554 IO Expander** | `EXIO1 = TP_RST`, `EXIO2 = LCD_RST`, `EXIO4 = SD_D3_EN`, `EXIO8 = Power Latch` |

---

## 📁 MicroSD Card Configuration

Format a MicroSD card as **FAT32** (MBR partition scheme) and place the configuration files in the root directory. Ready-to-use sample files are available in the [`sample_sd/`](file:///home/jason/google-drive/work/workspace/ai/dot-gemini/antigravity/scratch/esp32-flight-globe/sample_sd) folder:

### 1. `flights.txt` (or `flights.csv`)
Define one flight itinerary per line using 3-letter IATA airport codes. Multi-hop connections and round trips are fully supported:

```text
# Sample flight itineraries
BMI-DFW-HKG-DFW-BMI
ORD-SFO-HND-LHR-ORD
ORD-MIA-LIM-CUZ-LIM-ORD
ORD-ZRH-NCE-ZRH-ORD
ORD-DOH-KTM-DOH-ORD
ORD-PHX-HNL-KOA-HNL-ORD
LAX-SYD-CNS-SYD-LAX
```

*Note: Lines can use hyphens (`-`), arrows (`->`), commas (`,`), tabs, or spaces as delimiters.*

### 2. `config.txt` (or `config.ini`)
Configure runtime settings such as the automatic route cycle timer:

```ini
# Auto-cycle interval in seconds between routes (default: 10)
cycle_time=10
```

### 3. `airports.txt` (Optional)
Define custom airstrips, regional airports, or points of interest not found in standard commercial IATA databases:

```text
# Format: CODE, LATITUDE, LONGITUDE, NAME
1LL2, 40.1234, -88.1234, Private Strip
MYA, 42.0000, -87.8000, My Airstrip
```

---

## 🏗️ Project Structure

```
esp32-flight-globe/
├── platformio.ini         # PlatformIO environment and build configuration
├── sample_sd/             # Sample configuration and flight data for MicroSD
│   ├── airports.txt       # Example custom airport coordinates
│   ├── config.txt         # Auto-cycle interval configuration
│   └── flights.txt        # Multi-leg flight itineraries
├── src/
│   ├── main.cpp           # Main application loop, splash screen, touch & cycle logic
│   ├── globe_renderer.cpp # 3D sphere raycaster, limb shading, atmosphere & arc renderers
│   ├── globe_renderer.h   # GlobeRenderer class and LUT definitions
│   ├── flight_math.cpp    # Great circle trigonometry, Slerp arcs & distance formulas
│   ├── flight_math.h      # LatLon, Vec3, FlightRoute, and FlightLeg structures
│   ├── sd_reader.cpp      # SDMMC reader, config parser, custom airports loader
│   ├── sd_reader.h        # FlightDataManager class interface
│   ├── airports_data.h    # Compact PROGMEM IATA airport coordinates database
│   ├── earth_texture.h    # 720x360 RGB565 Earth surface texture map in PROGMEM
│   ├── Display_ST7701.cpp # Waveshare ST7701 RGB panel & backlight driver
│   ├── Touch_GT911.cpp    # Goodix GT911 capacitive touch driver
│   ├── TCA9554PWR.cpp     # TCA9554 I2C IO expander driver
│   └── I2C_Driver.cpp     # Hardware I2C initialization
└── tools/
    ├── dedup_routes.py    # Python utility to deduplicate multi-hop routes into unique legs
    ├── generate_airports.py       # Scrapes/compiles IATA airport DB to C++ PROGMEM header
    └── generate_earth_texture.py  # Generates 720x360 RGB565 texture from Natural Earth GeoJSON
```

---

## 🛠️ Building & Flashing

1. Install [Visual Studio Code](https://code.visualstudio.com/) and the [PlatformIO IDE extension](https://platformio.org/).
2. Clone this repository and open the project directory in VS Code:
   ```bash
   git clone https://github.com/jaybeeunix/esp32-flight-globe.git
   cd esp32-flight-globe
   ```
3. Connect your **Waveshare ESP32-S3-Touch-LCD-2.8C** board via USB-C.
4. Build and flash the project:
   ```bash
   # Build the project
   pio run

   # Upload firmware to board
   pio run -t upload

   # Open serial monitor (115200 baud)
   pio device monitor
   ```

---

## 🧰 Python Tools

The `tools/` directory includes helper scripts for processing flight data and updating graphics:

### Route Deduplicator (`tools/dedup_routes.py`)
Splits multi-hop routes (e.g. `CMI-ORD-LHR-ORD-CMI`) into individual unique flight legs and strips bidirectional duplicates (`ORD-CMI` matching `CMI-ORD`):
```bash
python3 tools/dedup_routes.py sample_sd/flights.txt -v
```

### Airport Database Generator (`tools/generate_airports.py`)
Downloads global airport coordinate data and compiles an alphabetized, binary-searchable PROGMEM struct table in `src/airports_data.h`:
```bash
python3 tools/generate_airports.py
```

### Earth Texture Generator (`tools/generate_earth_texture.py`)
Renders land polygons and coastlines from Natural Earth GeoJSON vector files into the 720×360 RGB565 header `src/earth_texture.h`:
```bash
python3 tools/generate_earth_texture.py
```

---

## 🎮 Interaction & Gestures

| Gesture / Event | Action |
| :--- | :--- |
| **Touch & Drag (Horizontal)** | Spins the globe to view any hemisphere. Auto-spin pauses while interacting. |
| **Single Tap** | Immediately advances to the next flight route in the list. |
| **Touch Inactivity (6s)** | Smoothly resumes automatic rotation at the default spin rate. |
| **Auto-Cycle Timer** | Smoothly rotates and centers the view on the next flight route every *N* seconds (default: 10s). |

---

## 📜 License & Credits

- **Project Author**: Jason Burrell ([jaybeeunix.github.io](https://jaybeeunix.github.io))
- **AI Development Assistant**: [Antigravity](https://antigravity.google/)
- **Display Driver & Graphics**: [LovyanGFX](https://github.com/lovyan03/LovyanGFX)
- **Map Data**: [Natural Earth](https://www.naturalearthdata.com/) (Public Domain vector map data)
- **Airport Database**: [mwgg/Airports](https://github.com/mwgg/Airports) (MIT License)

Distributed under the GNU General Public License v3.0 (GPL-3.0). See [LICENSE](LICENSE) for full details.
