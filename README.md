# Smart Hen Farm 🐔

An automated poultry shed that feeds, waters, and ventilates itself using IoT sensors and actuators.

Three sensors watch the shed, three actuators act on it, and nothing needs a person standing there. Grain runs low and the hopper gate opens until the tray is full again. The drinker empties and it refills. The shed gets too warm and the exhaust fan runs until it cools.

A phone app shows the live readings and raises an alert when something needs attention.

```
temperature ─┐                              ┌─→ exhaust fan   (relay)
humidity   ──┼─→  NodeMCU ESP8266  ─────────┼─→ hopper gate   (servo)
feed level ──┤     read → decide → act      ├─→ water valve   (relay)
water level ─┘         └─ verify ─┘         └─→ Firebase → phone app
```

---

## Project Images

<details>
<summary><b>Click to view project photos</b></summary>

### System Overview
![Hen Farm System](docs/images/system-overview.jpg)
*Complete IoT-based hen farm setup with all components*

### Hardware Components
![Hardware Components](docs/images/components.jpg)
*Sensors and actuators used in the system*

### Control Box
![Control Box](docs/images/control-box.jpg)
*NodeMCU ESP8266 with relay module and power supply*

### Gate Mechanism
![Gate Mechanism](docs/images/gate-mechanism.jpg)
*Servo-controlled hopper gate in action*

</details>

> **📸 How to add your images:**
> 1. Create a `docs/images/` folder if it doesn't exist
> 2. Add your project photos (JPG/PNG format)
> 3. Update the image links above with your filenames
> 4. Commit and push to make images visible

---

## The Three Control Loops

Each loop is closed. It acts, then keeps reading until the sensor confirms the target state, then stops. None of them run on a timer.

| Loop | Sensor | Actuator | Opens when | Closes when |
|---|---|---|---|---|
| **Feed** | Ultrasonic (HC-SR04) above the feed tray | Servo on the hopper gate | Tray reads empty | Tray reads full |
| **Water** | Moisture / water-level probe in the drinker | Relay on the water valve | Drinker reads empty | Drinker reads full |
| **Climate** | Temperature and humidity (DHT22) | Relay on the exhaust fan | Temp > threshold | Temp back in range |

### Why Closed-Loop Rather Than Timed?

A timed feeder dispenses whether or not the birds ate, so it either overflows the tray or starves them when they eat faster than expected. Measuring the tray and stopping when it is full means the same code works regardless of flock size or appetite.

---

## Hardware

| Part | Purpose | Notes |
|---|---|---|
| NodeMCU ESP8266 | Controller and WiFi | Powered separately from sensors |
| DHT22 | Temperature & humidity sensor | More accurate than DHT11 |
| HC-SR04 ultrasonic | Feed tray level measurement | Measures distance to grain surface |
| Capacitive moisture probe | Water level detection | Powers only during readings to extend life |
| SG90 servo | Hopper gate control | Alternative: MG996R for higher torque |
| 2-channel relay module | Exhaust fan & water valve | Must be optically isolated |
| 5V power supply | Powers servo and relays | **Separate from ESP8266 supply** |

**⚠️ Important:** Power the servo and relays from their own 5V rail, not from the NodeMCU's regulator. The servo draws a current spike when it starts moving, which can brown out the ESP mid-write. Use common ground between the two supplies.

---

## Control Logic

```
every loop:
    read temperature, humidity, feed distance, water state
    average readings to reduce sensor noise
    push readings to Firebase
    
    if temperature > TEMP_ON        → fan ON
    if temperature < TEMP_OFF       → fan OFF (hysteresis)
    
    if feed_distance > TRAY_EMPTY   → servo to GATE_OPEN
    if feed_distance < TRAY_FULL    → servo to GATE_CLOSED
    
    if drinker reads empty          → valve OPEN
    if drinker reads full           → valve CLOSED
```

### Configuration Thresholds

| Constant | Value | Purpose |
|---|---|---|
| `TEMP_ON` | ~28°C | Temperature at which fan activates |
| `TEMP_OFF` | ~25°C | Temperature at which fan stops (below `TEMP_ON` for hysteresis) |
| `TRAY_EMPTY` | ~20cm | Distance reading when feed tray needs refilling |
| `TRAY_FULL` | ~10cm | Distance reading when tray is full |
| `GATE_OPEN` | ~90° | Servo angle to open the hopper gate |
| `GATE_CLOSED` | ~0° | Servo angle to seal the hopper |

### Implementation Details

- **Hysteresis:** Fan switches on at `TEMP_ON` and off at `TEMP_OFF` (2-3°C gap) to prevent chattering. This extends relay life significantly.
- **Reading Averaging:** Ultrasonic readings are averaged over 5 samples to handle grain settling and dust scattering. This prevents false gate openings.
- **Fail-Safe:** Gate closes and valve shuts if ESP resets or WiFi disconnects.

---

## Setup Instructions

### 1. Arduino IDE Configuration

```bash
# Add the ESP8266 board URL under Preferences:
# http://arduino.esp8266.com/stable/package_esp8266com_index.json

# Install required libraries:
# - DHT sensor library by Adafruit
# - Servo (built-in)
# - FirebaseESP8266
```

### 2. Configure Credentials

```bash
cp src/config.example.h src/config.h
```

Edit `src/config.h`:

```cpp
#define WIFI_SSID       "your-network-name"
#define WIFI_PASSWORD   "your-wifi-password"
#define FIREBASE_HOST   "your-project.firebaseio.com"
#define FIREBASE_AUTH   "your-database-secret"

// Hardware calibration
#define TEMP_ON         28    // °C
#define TEMP_OFF        25    // °C
#define TRAY_EMPTY      20    // cm
#define TRAY_FULL       10    // cm
#define GATE_OPEN       90    // degrees
#define GATE_CLOSED     0     // degrees
```

**⚠️ Security:** `config.h` is gitignored. Never commit real credentials.

### 3. Flash and Test

```bash
# 1. Connect NodeMCU via USB
# 2. Select Tools → Board → NodeMCU 1.0 (ESP-12E Module)
# 3. Upload sketch
# 4. Open Serial Monitor at 115200 baud
# 5. Verify sensor readings look correct
# 6. Test gate and valve movements with no poultry present
# 7. Only then connect anything to mains power
```

---

## Mobile App

**Platform:** Flutter / Firebase Realtime Database

**Features:**
- Live temperature and humidity display
- Feed and water level monitoring
- Real-time alerts when thresholds are exceeded
- Manual override controls (emergency stop, force feed/water)
- Historical data logs and trends

**Authentication:** Firebase email/password or anonymous mode

> Add a screenshot of the app here for visual reference.

---

## Wiring Diagram

See `docs/wiring-diagram.pdf` or `docs/schematic.png`

---

## Safety ⚠️

The fan and water valve control mains-adjacent loads. Follow these rules:

1. **Low-voltage testing first:** Wire and test the NodeMCU, sensors, and servo completely before anything touches mains.
2. **Isolation required:** Use an optically isolated relay module (not a simple transistor).
3. **Proper enclosure:** All mains wiring must be enclosed, away from birds and water.
4. **Grounding:** Ensure proper earth connection on all equipment.

This is a working model and educational project, not a certified installation. Do your own safety review before deployment.

---

## Known Limitations

| Issue | Impact | Mitigation |
|---|---|---|
| **Fail-safe on connection loss** | Open gate on ESP crash empties hopper | Firmware closes gate and shuts valve on reset |
| **Moisture probe corrosion** | Probe life: 2-3 weeks continuous | Power probe only during readings |
| **Ultrasonic with sloped grain** | Measures nearest point, not average | Averaging 5 samples reduces false readings |
| **No local fallback** | WiFi outage = no visibility | Loops continue locally; logs stored on ESP |
| **Single flock, single shed** | Cannot scale to multiple sheds | Would require reworked data model |

---

## Project Structure

```
IOT-Based-Hen-Farm/
├── src/
│   ├── main.ino              Control loops and main logic
│   ├── config.example.h      Credentials and threshold template
│   ├── sensors.h/.cpp        DHT, ultrasonic, water probe drivers
│   ├── actuators.h/.cpp      Servo gate, relay control functions
│   └── firebase.h/.cpp       Firebase connectivity
├── app/
│   ├── lib/                  Flutter application source
│   ├── pubspec.yaml          Dependencies
│   └── README.md             App-specific setup
├── docs/
│   ├── images/               Project photos
│   ├── wiring-diagram.pdf    Hardware connections
│   └── TROUBLESHOOTING.md    Common issues and fixes
├── README.md                 This file
└── LICENSE                   MIT or your preferred license
```

---

## Getting Started

1. **Hardware:** Gather components and assemble following the wiring diagram
2. **Firmware:** Clone repo, set up Arduino IDE, configure `config.h`, flash ESP8266
3. **Testing:** Verify all sensors read correctly via serial monitor
4. **Firebase:** Set up a Realtime Database and update credentials
5. **App:** Build and deploy the Flutter app, connect to your Firebase project
6. **Deployment:** Mount in shed, perform safety checks, power on

---

## Troubleshooting

**ESP8266 won't connect to WiFi**
- Verify SSID and password in config.h
- Check antenna connection
- Restart ESP with serial monitor open to see debug messages

**Ultrasonic readings jumping**
- Check wiring, especially trigger and echo pins
- Increase averaging window in code
- Verify HC-SR04 isn't faulty

**Servo not moving**
- Check 5V power supply (should be separate from ESP)
- Verify servo PWM pin configuration
- Test servo with a standalone servo example

**Firebase connection fails**
- Verify WiFi is working first
- Check Firebase host and auth token
- Ensure database rules allow read/write

See `docs/TROUBLESHOOTING.md` for more solutions.

---

## Contributing

Improvements welcome! Please:
1. Test thoroughly before submitting PRs
2. Document any hardware changes
3. Update configuration thresholds for your setup
4. Share photos of your build

---

## License

MIT License - See LICENSE file for details

---

## Author

Burhan Ahmed
Automated poultry monitoring system | IoT | Arduino
