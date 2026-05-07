# Embedded Computing 2 Lab
# Task 3: MQTT Communication with JSON

Implementation of bidirectional MQTT communication between a simulated Raspberry Pi Pico W (Wokwi) and a real Arduino Nano 33 IoT using JSON-formatted messages over a public MQTT broker.

---

# Overview

The project consists of two communicating devices:

| Device | Role |
|---|---|
| Raspberry Pi Pico W (Wokwi Simulation) | MQTT receiver + sensor publisher |
| Arduino Nano 33 IoT | Serial command gateway + Remote Statistics|

The Nano receives commands through the Serial Monitor, converts them into JSON messages, and publishes them via MQTT. The Pico subscribes to these commands, executes them, and continuously publishes its sensor and LED status back to the Nano.

Communication is performed through the public MQTT broker:

```text
141.69.95.10 : 1883
```

---

# System Architecture

```text
Serial Monitor
      │
      ▼
Arduino Nano 33 IoT
(Serial → JSON → MQTT)
      │
      ▼
141.69.95.10
      │
      ▼
Raspberry Pi Pico W (Wokwi)
(MQTT → JSON → Hardware Control)
```

---

# Features

## MQTT Communication
- Bidirectional MQTT pub/sub communication
- JSON-formatted messages
- Separate command and data topics

## Pico W Features
- LED control
- Variable blink interval
- Potentiometer-controlled blinking
- Button-controlled local blink toggle
- Sensor data publishing
- Safe-state fallback mode

## Nano 33 IoT Features
- Serial command parser
- Remote Pico status display
- MQTT command publishing
- Mirror LED feedback
- Statistics and debugging output

## Robustness Features
- Shared token authentication
- Source validation
- Sequence number tracking
- Replay detection
- Gap detection
- Watchdog timeout handling
- Range checking for intervals

---

# MQTT Topics

| Topic | Publisher | Subscriber | Purpose |
|---|---|---|---|
| `iem/task3/pico/cmd` | Nano | Pico | Remote control commands |
| `iem/task3/pico/data` | Pico | Nano | Sensor and status data |

---

# JSON Message Format

## Nano → Pico (Commands)

### Blink Command

```json
{
  "token":"iem2026",
  "source":"nano",
  "seq":1,
  "blinkEnabled":true
}
```

### Static LED ON

```json
{
  "token":"iem2026",
  "source":"nano",
  "seq":2,
  "ledOn":true
}
```

### Interval Override

```json
{
  "token":"iem2026",
  "source":"nano",
  "seq":3,
  "interval":250
}
```

---

## Pico → Nano (Status Data)

```json
{
  "token":"iem2026",
  "source":"pico",
  "seq":42,
  "potValue":512,
  "blinkEnabled":true,
  "interval":250,
  "ledState":false,
  "safeState":false,
  "uptime":120
}
```

---

# Hardware Setup

## Pico W (Wokwi)
- LED → GP28
- Pushbutton → GP2
- Potentiometer → GP26

## Nano 33 IoT
- Uses onboard LED to mirror remote LED status
- Connected to PC through USB

---

# Required Libraries

## Pico W
- `WiFi.h`
- `PubSubClient`
- `SimpleJson.h`

## Nano 33 IoT
- `WiFiNINA.h`
- `PubSubClient`
- `SimpleJson.h`

---

# Installation

## 1. Clone Repository

```bash
git clone https://github.com/neelkaria/ec2_task3.git
```

---

## 2. Wokwi Setup

1. Create a new Raspberry Pi Pico W project in Wokwi
2. Copy:
    - `pico_w_mqtt.ino`
    - `SimpleJson.h`
3. Add the `PubSubClient` library.
4. Open `pico_w_mqtt.ino`
5. Update WiFi credentials (`WIFI_SSID` and `WIFI_PASS`)
6. Install and start the latest release of **[Wokwi IoT Gateway](https://github.com/wokwi/wokwigw/releases)** application on your desktop before simulation and **keep it running**.
7. Start simulation.

**Note/Observation:** Switch to Private Gateway on Wokwi (click the WiFi symbol) to use 141.69.95.10 broker on RWU WiFi.

---

## 3. Nano 33 IoT Setup

1. Install Arduino IDE:

2. Install Board Package:
    - Arduino IDE → Tools → Boards → Boards Manager
    - Search: Arduino SAMD Boards
    - Install latest version

3. Install Libraries:
    - Sketch → Include Library → Manage Libraries
    - Search and install:
      - WiFiNINA (by Arduino)
      - PubSubClient (by Nick O'Leary)



Then:
1. Open `mqtt_nano.ino`
2. Update WiFi credentials (`cWIFI_SSID` and `WIFI_PASS`)
3. Upload code to Nano
4. Open Serial Monitor at `115200 baud`

---

# Serial Commands (Nano 33 IoT Serial Monitor)

| Command | Function |
|---|---|
| `ON` | LED permanently ON |
| `OFF` | LED permanently OFF |
| `BLINK` | Enable blinking |
| `NOBLINK` | Disable blinking |
| `INTERVAL <50-2000>` | Set blink interval (in ms)|
| `POT` | Re-enable potentiometer control |
| `STATUS` | Display remote Pico status |
| `STATS` | Display communication statistics |
| `HELP` | Show command menu |

---

# Example Usage

## Enable Blinking

```text
BLINK
```

## Set Interval

```text
INTERVAL 500
```

## Return to Potentiometer Control

```text
POT
```

## Display Remote Status

```text
STATUS
```

---

# Robustness Mechanisms

## Shared Token Validation

Every MQTT message contains:

```json
"token":"iem2026"
```

Messages with invalid tokens are rejected.

---

## Source Validation

The Pico only accepts:

```json
"source":"nano"
```

The Nano only accepts:

```json
"source":"pico"
```

---

## Sequence Number Checking

Each message contains:

```json
"seq": <number>
```

Used for:
- replay detection
- duplicate detection
- gap warnings

---

## Watchdog / Safe State

### Pico
If no valid MQTT command is received within 10 seconds:
- system enters safe state
- potentiometer control is restored

### Nano
If no Pico data is received within 10 seconds:
- timeout warning is displayed

---

# Testing Invalid MQTT Messages

Example using Mosquitto MQTT Tools:

## Invalid Token

```bash
mosquitto_pub -h 141.69.95.10 -t "iem/task3/pico/cmd" \
-m '{"token":"wrong","source":"nano","seq":0,"blinkEnabled":false}'
```

## Missing Token

```bash
mosquitto_pub -h 141.69.95.10 -t "iem/task3/pico/cmd" \
-m '{"blinkEnabled":false}'
```

## Valid Command

```bash
mosquitto_pub -h 141.69.95.10 -t "iem/task3/pico/cmd" \
-m '{"token":"iem2026","source":"nano","seq":0,"blinkEnabled":true}'
```

---

# Known Issues / Notes

- MQTT brokers may occasionally deliver duplicate or out-of-order messages
- Wokwi IoT Gateway must remain running during simulation
- Public MQTT brokers may introduce latency or packet reordering
- Unique topic paths and client IDs are recommended for multiple users/groups

---

# Project Files

| File | Description |
|---|---|
| `pico_w_mqtt.ino` | Pico W MQTT receiver and sensor publisher |
| `mqtt_nano.ino` | Nano 33 IoT serial command gateway |
| `SimpleJson.h` | Lightweight JSON parser/builder |
| `README.md` | Project documentation |

---

# Technologies Used

- C++
- MQTT
- JSON
- Arduino Framework
- Wokwi Simulation
- WiFiNINA
- PubSubClient

---

# References

- Task Deliverables: `embedded_lab_task3.pdf`
- MQTT Brokers: [EMQX](https://www.emqx.com/en/mqtt/public-mqtt5-broker) (Local) **OR** 141.69.95.10 (Public)
- [PubSubClient Library](https://github.com/knolleary/pubsubclient)
- [Wokwi Simulator](https://wokwi.com/)

---

## Author
**Neel Karia (16346352)**

Embedded Lab & Project – Task 3  
MQTT Communication with JSON  
Summer Semester 2026

