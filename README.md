# Smart Hen Farm

An automated poultry shed that feeds, waters and ventilates itself.

![The build](build-exterior.jpg)

Three sensors watch the shed, three actuators act on it, and nothing needs a person standing
there. Grain runs low and the hopper gate opens until the tray is full again. The drinker
empties and it refills. The shed gets too warm and the exhaust fan runs until it cools.

A phone app shows the live readings and raises an alert when something needs attention.

```
  temperature  --+                            +--> exhaust fan  (relay)
  humidity     --+                            |
                 +-->  NodeMCU ESP8266  ------+--> hopper gate  (servo)
  feed level   --+   read -> decide -> act    |
  water level  --+        \__ verify __/      +--> water pump   (relay)
                                              |
                                              +--> Firebase --> phone app
```

---

## The three control loops

Each loop is closed. It acts, then keeps reading until the sensor confirms the target state,
then stops. None of them run on a timer.

| Loop | Sensor | Actuator | Starts when | Stops when |
|---|---|---|---|---|
| **Feed** | HC-SR04 ultrasonic above the feed tray | SG90 servo on the hopper gate | Tray reads empty | Tray reads full |
| **Water** | FC-28 probe in the drinker | Submersible pump via relay | Drinker reads empty | Drinker reads full |
| **Climate** | DHT11 temperature and humidity | DC exhaust fan via relay | Temperature above the limit | Temperature back in range |

![Interior](build-interior.jpg)

*Drinker with the level probe on the left, feed area and sensor modules on the right, exhaust
fan at the back.*

### Why closed-loop rather than timed

A timed feeder dispenses whether or not the birds ate, so it either overflows the tray or
starves them when they eat faster than expected. Measuring the tray and stopping when it is
full means the same code works regardless of flock size or appetite.

---

## Hardware

| Part | Purpose |
|---|---|
| NodeMCU V3 (ESP8266) | Controller and WiFi |
| DHT11 | Shed temperature and humidity |
| HC-SR04 ultrasonic | Feed tray level, measured as distance to the grain surface |
| FC-28 probe with LM393 comparator | Drinker water level |
| SG90 servo | Hopper gate |
| 4-channel relay module | Exhaust fan and water pump |
| Submersible pump | Refills the drinker from the reservoir |
| DC fan | Exhaust |

The wiring diagram below is the pin reference. Every sensor and actuator connection is on it.

![Wiring](wiring-diagram.jpg)

### Two wiring details that matter

**The HC-SR04 Echo pin outputs 5V and the ESP8266 is a 3.3V part.** Echo goes through a
divider, 1k from Echo to the input and 2k from the input to ground, before it reaches a NodeMCU
pin. Driven directly it works at first and degrades the pin over time, which makes it an
unpleasant fault to track down later.

**The servo and relays run from their own 5V rail, not the NodeMCU's regulator.** The servo
pulls a current spike the moment it starts moving, and sharing the rail browns out the ESP
mid-write. The two grounds are tied together.

---

## Control logic

```
every loop:
    read temperature, humidity, feed distance, water state
    push readings to Firebase

    if temperature > TEMP_ON        -> fan on
    if temperature < TEMP_OFF       -> fan off

    if feed_distance > TRAY_EMPTY   -> servo to GATE_OPEN
    if feed_distance < TRAY_FULL    -> servo to GATE_CLOSED

    if drinker reads empty          -> pump on
    if drinker reads full           -> pump off
```

### Temperature setpoints

| Constant | Value | Note |
|---|---|---|
| `TEMP_ON` | 30 C | Fan starts. Heat stress in grown birds begins around 26 to 28 C |
| `TEMP_OFF` | 27 C | Fan stops. A 3 C gap, see below |

These suit grown birds. Day-old chicks need 32 to 35 C and step down about 3 C a week, so a
brooding shed needs an age-dependent setpoint rather than the fixed one here.

### Two things worth knowing if you rebuild this

**Keep a gap between the on and off thresholds.** If the fan switches on and off at the same
temperature it chatters at that point: the relay clicks continuously and the fan starts and
stops every few seconds, wearing out both. The 3 C gap means the fan runs a useful while before
it stops. The same idea applies to the feed tray, which is why calibration puts a 1 cm margin
either side of the measured readings.

**Take the median of several ultrasonic readings before acting.** Grain settles unevenly and
dust scatters the pulse, so single readings jump around. A median discards the outliers that a
mean would fold into the result. `readDistanceCm()` in `calibrate.ino` is the implementation.

---

## Calibration

The remaining four constants depend on the physical build, because they change with how high
the ultrasonic sensor sits above the tray and where the servo horn is mounted. They are
measured rather than chosen.

Flash `calibrate.ino`, set the three pins at the top to match your wiring, and open the serial
monitor at 115200. It walks through an empty tray, a full tray and a servo sweep, then prints:

| Constant | What it is |
|---|---|
| `TRAY_EMPTY` | Distance to the grain surface when the tray needs filling |
| `TRAY_FULL` | Distance when full. Stays above 2 cm, the HC-SR04 minimum range |
| `GATE_OPEN` | Servo angle that clears the hopper mouth |
| `GATE_CLOSED` | Servo angle that seals it |

Copy the four values into `src/main.ino`.

The sketch also reports the spread on each reading. If a static tray varies by more than about
2 cm, the sensor is not aimed cleanly and no choice of threshold will fix that.

---

## Setup

```bash
# 1. Arduino IDE, add the ESP8266 board URL under Preferences:
#    http://arduino.esp8266.com/stable/package_esp8266com_index.json
# 2. Install libraries: DHT sensor library, Servo, FirebaseESP8266
# 3. Copy the config template and fill it in
cp src/config.example.h src/config.h
```

`config.h`:

```cpp
#define WIFI_SSID       "your-network"
#define WIFI_PASSWORD   "your-password"
#define FIREBASE_HOST   "your-project.firebaseio.com"
#define FIREBASE_AUTH   "your-database-secret"
```

`config.h` is gitignored. Do not commit real credentials.

Flash the board, open the serial monitor at 115200, and confirm the readings look sane before
connecting anything to mains.

---

## The app

The controller pushes every reading to the Firebase Realtime Database, and the companion mobile
app subscribes to that same tree. Because Firebase pushes changes rather than being polled, the
app shows the shed's current state within a second or so of the sensor reading it, and raises
an alert when a value sits outside its range.

Keeping all the decision-making on the device rather than in the app is deliberate. The app
observes and notifies; it never holds a control loop. That means the shed keeps feeding,
watering and venting itself whether or not a phone is connected, or a network is even present.

---

## Safety

The fan and the pump switch mains-adjacent loads through relays. Two rules:

- Wire and test the low-voltage side completely before anything is connected to mains.
- The relay module must be optically isolated, and mains wiring should be enclosed and out of
  reach of the birds and of water.

This is a working model, not a certified installation.

---

## Known limits

- **A reset mid-cycle leaves the gate where the servo last held it.** Nothing in the hardware
  returns it to a safe position on power loss, so the firmware has to drive it to `GATE_CLOSED`
  at the top of `setup()`, before anything else runs. An open gate on a hung controller empties
  the hopper into the tray.
- **The FC-28 is a soil probe used as a water-level sensor.** It works, but its exposed
  electrodes corrode in standing water within weeks, because continuous DC through them
  electrolyses the metal. Powering the probe only during a reading extends its life a long way.
  A float switch or a capacitive probe is the proper fix.
- **Ultrasonic sensing struggles with a sloped grain surface.** It measures the nearest point,
  not the average, so a mound under the hopper mouth reads full while the edges are empty.
- **No remote visibility without a network.** The control loops run on the device and keep
  working through an outage, but readings and alerts stop until it reconnects.
- **Fixed temperature setpoint.** Suits grown birds, not chicks. See the setpoints above.
- **Single shed, single flock.** Nothing here scales to multiple sheds without reworking the
  data model.

---

## Gallery

| | |
|---|---|
| ![Exterior](build-exterior.jpg) | ![Side](build-side.jpg) |
| The finished model | Reservoir, tubing, servo and fan through the acrylic wall |

---

## Project layout

```
src/
  main.ino            control loops
  config.example.h    credentials template
calibrate.ino         one-off sketch for the four measured constants
app/                  mobile monitoring app
```
