# ESP8266 MQTT Motion Relay Controller

This project turns an ESP8266 module (e.g., ESP-01S) into a Wi-Fi motion controller that:
- Detects motion via a PIR sensor
- Controls a relay either locally or via MQTT
- Publishes status and motion data in JSON
- Supports remote configuration through MQTT commands

## 📦 Features

- PIR motion detection with configurable polling interval
- MQTT-based command interface (ON/OFF/STATUS/REBOOT/etc.)
- Optional local relay control bypass (SKIP_LOCAL_RELAY)
- Sends motion events as JSON to MQTT broker
- Self-resets relay after a defined timeout
- Dynamic topic construction based on MAC address

## 🔌 Hardware Setup

| Component   | ESP8266 Pin |
|-------------|-------------|
| PIR Sensor  | D4 (GPIO2)  |
| Relay       | D3 (GPIO0)  |
| VCC/GND     | Common power & ground lines |

Power Notes:  
Ensure your ESP8266 gets sufficient current (~250mA or more). Use a regulated 3.3V power supply or buck converter if powering from 5V.

## 📡 MQTT Setup

The device connects to an MQTT broker to publish status, send motion events, and receive commands.

Broker setup is hardcoded as:
client.setServer("192.168.1.246", 1883);

### 🔖 Topic Structure

Topics are dynamically generated using this format:
home/{GHAFEER_NAME}/{MAC}/status  
home/{GHAFEER_NAME}/{MAC}/motion  
home/{GHAFEER_NAME}/{MAC}/cmd

Example:
home/HAGRRAS/C0A80123/status  
home/HAGRRAS/C0A80123/motion  
home/HAGRRAS/C0A80123/cmd

## 🧪 MQTT Command Reference

Use the command topic to send these instructions:

| Command                    | Effect                                            |
|---------------------------|---------------------------------------------------|
| REL_ON                    | Turn relay ON                                     |
| REL_OFF                   | Turn relay OFF                                    |
| REL_STATUS                | Query relay status (ON or OFF)                    |
| PIR_INTERVAL:<ms>         | Set motion polling interval (e.g. PIR_INTERVAL:5000) |
| SKIP_LOCAL_RELAY:true     | Prevent local relay activation on motion          |
| SKIP_LOCAL_RELAY:false    | Allow motion to trigger relay directly            |
| REBOOT                    | Restart the device                                |
| STATUS                    | Respond with IP, MAC, and mode information        |

Responses are published to the status topic.

## 📤 Motion JSON Format

When motion is detected, the following JSON payload is published:

{
  "motion": true,
  "mac": "C0A80123",
  "location": "HAGRRAS",
  "ip": "192.168.1.99",
  "time": 123456789
}

## 🧠 Behavior Flow

PIR sensor is polled every PIR_INTERVAL milliseconds. If motion is detected:
- If SKIP_LOCAL_RELAY is false, the relay is activated.
- A JSON message is published to the motion topic.
- A status update is sent.

Relay is turned off automatically if left on for more than RELAY_MAX_ON_DURATION (2 minutes by default), or via the REL_OFF command.

## 📶 Wi-Fi Setup

Wi-Fi credentials must be stored in a separate file named secrets.h as:

#define WIFI_SSID     "your-ssid"  
#define WIFI_PASSWORD "your-password"

## ⚙️ Constants in Code

| Variable               | Description                                          |
|------------------------|------------------------------------------------------|
| PIR_PIN                | GPIO2 (D4) – PIR sensor input                        |
| RELAY_PIN              | GPIO0 (D3) – Relay control output                    |
| PIR_INTERVAL           | Delay between motion checks (default 60000ms)       |
| RELAY_MAX_ON_DURATION  | Max duration relay stays ON (default 120000ms)      |
| SKIP_LOCAL_RELAY       | If true, bypass relay control locally               |

## 🛠 Example Status Message

Published to the status topic after startup or on STATUS command:

GHAFEER_NAME:HAGRRAS,  
Device_Status:Online,  
MAC:C0A80123,  
IP:192.168.1.99,  
SKIP_LOCAL_RELAY:true

## 🖼️ Suggested System Diagram

PIR Sensor → ESP8266 →  
  ↳ Relay Control  
  ↳ MQTT Broker  
    ↳ Receives commands  
    ↳ Publishes motion and status  
    → Home Dashboard or Logger

## 📁 Project Structure

main.ino               — Main Arduino sketch  
secrets.h              — Wi-Fi credentials (not shared)  
README.md              — This documentation  

## 🧯 Reliability Notes

- Use INPUT_PULLUP for PIR input to reduce false triggers.
- Add a small capacitor on relay VCC line to prevent brownout resets.
- Consider filtering motion messages in consumer applications.

## 🧰 Requirements

- Arduino IDE or PlatformIO
- Libraries:
  - ESP8266WiFi
  - PubSubClient

## 📬 Author

Device Name: HAGRRAS  
Contact: harraz@gmail.com  
Platform: ESP8266 (tested on ESP-01S)

## 📄 License

MIT License – free to use, share, and modify.
