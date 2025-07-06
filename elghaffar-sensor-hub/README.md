# ESP8266 MQTT Motion Relay Controller — Deer Deterrent System

This project transforms an ESP8266 module (e.g., ESP-01S) into a Wi-Fi-enabled motion controller that:
- Detects motion via a PIR sensor
- Controls a relay either locally or remotely via MQTT
- Publishes motion events and status as JSON messages
- Accepts real-time configuration commands through MQTT
- Seamlessly integrates into a multi-device, multi-zone deterrent network

---

## 🌿 Real-World Use Case: Deer Deterrent System

This system is deployed outdoors to protect gardens and flower beds from deer intrusion. It works as follows:

- **PIR Sensor Nodes** (numbered 0, 1, 2, ..., n) are placed strategically around the perimeter.
- When motion is detected, the sensor sends a **motion message via MQTT** to a central dispatcher.
- The **Dispatcher Hub** processes the signal and triggers the appropriate **Action Device**.
- **Action Devices** (each with a relay and solenoid valve) activate water sprays to scare off deer.

**Components:**
- **Sensor Nodes:** ESP8266 + PIR + MQTT client
- **Action Devices:** ESP8266 + Relay + Solenoid Valve (connected to water hose)
- **Dispatcher Hub:** Raspberry Pi running MQTT broker + Python dispatcher using Paho MQTT

**Flow:**
1. Motion detected → MQTT `motion` topic
2. Dispatcher routes the alert to correct action zone
3. `REL_ON` sent to action device via MQTT
4. After delay (~15 sec), `REL_OFF` command stops the spray

This network is reliable, modular, and scalable — suitable for outdoor automation and wildlife deterrence.

---

## 📦 Features

- Motion-triggered relay control with debounce and timeout
- MQTT messaging using structured topics with dynamic MAC & device names
- Fully supports multiple device groups (e.g., ELGHAFFAR, BASYOUNEE)
- JSON-formatted motion payloads with metadata
- Configurable via MQTT commands (e.g., polling interval, reboot)

---

## 🔌 Hardware Setup

| Component   | ESP8266 Pin |
|-------------|-------------|
| PIR Sensor  | D4 (GPIO2)  |
| Relay       | D3 (GPIO0)  |
| VCC/GND     | Common power & ground lines |

> **Power Notes:** Ensure each device has a reliable 3.3V supply (>250mA). Use external regulators for solenoids/relays.

---

## 📡 MQTT Setup

Each ESP8266 publishes to MQTT topics structured as:

home/{GHAFEER_NAME}/{MAC}/status  
home/{GHAFEER_NAME}/{MAC}/motion  
home/{GHAFEER_NAME}/{MAC}/cmd

Where:
- `GHAFEER_NAME` = logical group or sensor location (e.g., ELGHAFFAR, HAGRRAS)
- `MAC` = device MAC address in uppercase, no colons

**Example:**

home/BACKYARD/C0A80123/status  
home/BACKYARD/C0A80123/motion  
home/BACKYARD/C0A80123/cmd

---

### Broker Configuration (Python Dispatcher)

The dispatcher is implemented using the **Paho MQTT Python client**, and lives in the [`markaz/`](https://github.com/harraz/home_projects/tree/main/elghaffar-sensor-hub/markaz) directory of this project.

Example dispatcher behavior:
- Subscribes to: `home/+/+/motion`
- Extracts: `GHAFEER_NAME` and `MAC` from the topic
- Determines which action device(s) to activate
- Sends `REL_ON` to `home/{ActionName}/{MAC}/cmd`
- Waits (e.g., 15 seconds), then sends `REL_OFF`

This enables centralized control, zoning logic, and flexible future integrations such as AI filtering or event scheduling.

> ⚙️ MQTT broker and dispatcher both run on a local Raspberry Pi, allowing full control of local automation without the cloud.

---

## 🧪 MQTT Command Reference

Commands are published to:  
home/{GHAFEER_NAME}/{MAC}/cmd

| Command                    | Description                                      |
|---------------------------|--------------------------------------------------|
| `REL_ON`                  | Turn relay ON                                    |
| `REL_OFF`                 | Turn relay OFF                                   |
| `REL_STATUS`              | Query relay state                                |
| `PIR_INTERVAL:<ms>`       | Set PIR polling interval                         |
| `SKIP_LOCAL_RELAY:true`   | Disable local relay control                      |
| `SKIP_LOCAL_RELAY:false`  | Enable local relay control                       |
| `REBOOT`                  | Reboot the ESP8266 device                        |
| `STATUS`                  | Report MAC, IP, and relay mode                   |

---

## 📤 Motion JSON Format

When motion is detected, a JSON message is published:

```json
{
  "motion": true,
  "mac": "C0A80123",
  "location": "BACKYARD",
  "ip": "192.168.1.99",
  "time": 123456789
}
```

---

## 🧠 Behavior Flow

1. Device boots and connects to Wi-Fi + MQTT
2. PIR checked every `PIR_INTERVAL` ms
3. If motion is detected:
   - Relay activated (if not skipped)
   - MQTT motion JSON published
   - Status update sent
4. After 2 minutes (or via `REL_OFF`), relay is disabled

---

## 🧩 Multi-Device, Multi-Zone Support

- Each device operates independently using its own topics
- The dispatcher maps `GHAFEER_NAME + MAC` → one or more action zones
- Allows localized responses without overlap
- Fully asynchronous — no need for devices to know each other

---

## 📶 Wi-Fi Setup

Create a `secrets.h` file:

```cpp
#define WIFI_SSID     "your-ssid"
#define WIFI_PASSWORD "your-password"
```

---

## ⚙️ Constants in Code

| Constant                | Description                                          |
|-------------------------|------------------------------------------------------|
| `PIR_PIN`               | GPIO2 – PIR input                                    |
| `RELAY_PIN`             | GPIO0 – Relay output                                 |
| `PIR_INTERVAL`          | PIR polling interval in ms (default 60000)          |
| `RELAY_MAX_ON_DURATION` | Max ON duration before auto-off (default 120000 ms) |
| `SKIP_LOCAL_RELAY`      | If true, relay not triggered locally on motion       |

---

## 🛠 Example STATUS Message

Published on boot or `STATUS` command:

```
GHAFEER_NAME:BASYOUNEE
Device_Status:Online
MAC:C0A80123
IP:192.168.1.99
SKIP_LOCAL_RELAY:false
```

---

## 🖼️ System Architecture

```
[PIR Sensor Node 1]       [PIR Sensor Node 2]         ...       [PIR Sensor Node N]
       │                          │                                       │
       └── MQTT: motion event     └── MQTT: motion event                  └── MQTT: motion event
                    │
              [Dispatcher / Hub]
                    │
       ┌────────────┼────────────┐
       │            │            │
[Action Device 1] [Action 2] [Action 3]
   (Relay + solenoid valve)
```

---

## 🔭 Future Plans

- ☀️ Add solar panels + battery packs for off-grid deployment
- 📡 Use LoRa modules for wide-area low-power communication
- 🤖 Add AI camera (e.g., ESP32-CAM + YOLO or Edge Impulse) to differentiate animals
- 📈 Dashboard integration for event logging and visualization
- 🕸️ Mesh-network fallback in case of Wi-Fi failure

---

## 📁 Project Structure

The project follows standard **PlatformIO** structure:

```
elghaffar-sensor-hub/
├── markaz/              # Python dispatcher code using Paho MQTT
├── src/                 # C++ source code for ESP8266 devices
│   ├── main.cpp         # Main firmware logic
│   └── other files...  
├── include/             # Header files
├── platformio.ini       # PlatformIO project configuration
└── README.md            # Project documentation
```
---

## 🧯 Reliability Tips

- Add capacitor near relay power line to prevent brownouts
- Secure waterproof enclosures for outdoor use
- Add watchdog timer for unresponsive devices

---

## 🧰 Requirements

- **PlatformIO** (Recommended)
- Libraries managed through PlatformIO `platformio.ini`:
  - ESP8266WiFi
  - PubSubClient

> ℹ️ This project is intended for use with **PlatformIO**, not the Arduino IDE. The source code is organized in `.cpp` and `.h` files following PlatformIO conventions.

---

## 📬 Author

Maintainer: Mohamed Farouk Harraz  
Contact: harraz@gmail.com  
Platform: ESP8266 (tested on ESP-01S)

---

## 📄 License

MIT License – free to use, share, and modify.
