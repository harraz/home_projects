# ESP8266 MQTT Motion Relay Controller

This project transforms an ESP8266 module (e.g., ESP-01S) into a Wi-Fi-enabled motion controller that:
- Detects motion via a PIR sensor
- Controls a relay either locally or remotely via MQTT
- Publishes motion events and status as JSON messages
- Accepts real-time configuration commands through MQTT
- Seamlessly integrates into a multi-device, multi-group MQTT sensor hub

## 📦 Features

- PIR motion detection with configurable polling interval
- MQTT-based command interface (ON/OFF/STATUS/REBOOT/etc.)
- Optional local relay control bypass (`SKIP_LOCAL_RELAY`)
- Sends motion events as JSON to MQTT
- Self-resets relay after a defined timeout
- Dynamic topic generation using `GHAFEER_NAME` and device MAC
- Supports multiple device groups (e.g., ELGHAFFAR, BASYOUNEE)

## 🔌 Hardware Setup

| Component   | ESP8266 Pin |
|-------------|-------------|
| PIR Sensor  | D4 (GPIO2)  |
| Relay       | D3 (GPIO0)  |
| VCC/GND     | Common power & ground lines |

> **Power Notes:** Ensure sufficient current (~250mA or more). Use a regulated 3.3V power supply or buck converter if powering from 5V.

## 📡 MQTT Setup

Each ESP8266 publishes to MQTT topics structured as:

home/{GHAFEER_NAME}/{MAC}/status  
home/{GHAFEER_NAME}/{MAC}/motion  
home/{GHAFEER_NAME}/{MAC}/cmd

Where:
- `GHAFEER_NAME` is a logical device group (e.g., ELGHAFFAR, HAGRRAS)
- `MAC` is the device’s MAC address (uppercase, no colons)

**Example:**

home/ELGHAFFAR/C0A80123/status  
home/ELGHAFFAR/C0A80123/motion  
home/ELGHAFFAR/C0A80123/cmd

### MQTT Broker Setup

The MQTT broker (e.g., running on a Raspberry Pi) should subscribe using wildcards to capture all device groups:

```js
mqttClient.subscribe('home/+/+/status');
mqttClient.subscribe('home/+/+/motion');
mqttClient.subscribe('home/+/+/cmd');

mqttClient.on('message', (topic, payload) => {
  const [_, ghafeer, mac, type] = topic.split('/');
  // Use ghafeer (device group) and mac to route/control devices
});
```

This allows for dynamic coordination across multiple independent ESP devices grouped by function or location.

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

All responses are published to the `status` topic.

## 📤 Motion JSON Format

When motion is detected, a JSON payload is published to the motion topic:

{
  "motion": true,
  "mac": "C0A80123",
  "location": "ELGHAFFAR",
  "ip": "192.168.1.99",
  "time": 123456789
}

## 🧠 Behavior Flow

1. Device boots and connects to Wi-Fi and MQTT broker
2. Topics are dynamically constructed using `GHAFEER_NAME` and MAC
3. PIR sensor is checked every `PIR_INTERVAL` milliseconds
4. If motion is detected:
   - If `SKIP_LOCAL_RELAY == false`, the relay is activated
   - JSON motion payload is published
   - A status update is sent
5. Relay automatically turns off after 2 minutes unless overridden

## 🧩 Multi‑Device & Group Support

This system supports multiple devices and logical groups.

Each device:
- Belongs to a unique `GHAFEER_NAME` group (e.g., "BASYOUNEE", "ELGHAFFAR")
- Publishes its motion and status independently
- Can be commanded individually through its topic

The MQTT hub (like the Node.js broker in your [sensor-hub repo](https://github.com/harraz/home_projects/tree/main/elghaffar-sensor-hub)) can:
- Subscribe to all devices using `home/+/+/+`
- Parse topic structure to identify source device/group
- Route commands or status updates dynamically

## 📶 Wi-Fi Setup

Wi-Fi credentials should be stored in a `secrets.h` file:

#define WIFI_SSID     "your-ssid"  
#define WIFI_PASSWORD "your-password"

## ⚙️ Configuration Constants

| Constant                | Description                                          |
|-------------------------|------------------------------------------------------|
| `PIR_PIN`               | GPIO2 (D4) – PIR input                               |
| `RELAY_PIN`             | GPIO0 (D3) – Relay control output                    |
| `PIR_INTERVAL`          | Delay between motion checks (default 60000ms)       |
| `RELAY_MAX_ON_DURATION` | Max duration relay stays ON (default 120000ms)      |
| `SKIP_LOCAL_RELAY`      | Bypass relay when true, MQTT control only           |

## 🛠 Example STATUS Message

Published to `status` topic on startup or via `STATUS` command:

GHAFEER_NAME:ELGHAFFAR,  
Device_Status:Online,  
MAC:C0A80123,  
IP:192.168.1.99,  
SKIP_LOCAL_RELAY:false

## 🖼️ System Overview Diagram

PIR Sensor → ESP8266 →  
  ↳ Relay Control  
  ↳ MQTT Broker (home/{GHAFEER_NAME}/{MAC}/...)  
    ↳ Receives Commands  
    ↳ Publishes Motion Events  
    ↳ Publishes Status  
    → Node.js MQTT Hub / Web Dashboard

## 📁 Project Structure

main.ino               — ESP8266 Arduino code  
secrets.h              — Wi-Fi credentials (excluded from version control)  
README.md              — This documentation  

## 🧯 Reliability Tips

- Use `INPUT_PULLUP` for PIR pin to reduce noise
- Ensure solid GND between relay and ESP
- Add capacitor across relay VCC/GND to prevent brownouts
- Watch for ghost triggers; use debounce logic if needed

## 🧰 Requirements

- Arduino IDE or PlatformIO
- Libraries:
  - ESP8266WiFi
  - PubSubClient

## 📬 Author

Device Name: HAGRRAS  
Project Maintainer: Mohamed Farouk Harraz  
Contact: harraz@gmail.com  
Platform: ESP8266 (tested on ESP-01S)

## 📄 License

MIT License – free to use, share, and modify.
