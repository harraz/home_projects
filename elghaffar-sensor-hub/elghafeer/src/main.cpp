/*
  NOTE: This code is customized for ESP01s modules prepackaged with a relay board.
  The relay is ACTIVE-LOW: setting RELAY_PIN LOW energizes (turns ON) the relay,
  and setting RELAY_PIN HIGH de-energizes (turns OFF) the relay.

  This is the reverse of the typical relay logic. 
  DO NOT MERGE THIS FILE INTO THE MASTER BRANCH unless all hardware uses active-low relays.

  If you need standard (active-high) relay logic, revert these changes.
*/

#define MQTT_MAX_PACKET_SIZE 512

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include "secrets.h"   // #define WIFI_SSID, WIFI_PASSWORD

String GHAFEER_NAME = "MAHROOS";

const int PIR_PIN    = 2;  // D4
const int RELAY_PIN  = 0;  // D3

unsigned long PIR_INTERVAL = 60000; // ms (default, can change via MQTT)
const unsigned long RELAY_MAX_ON_DURATION = 120000; // ms
bool SKIP_LOCAL_RELAY = true; // true to use local relay control, false to control other devices via MQTT
// Note: SKIP_LOCAL_RELAY is used to skip local relay activation when motion is detected
// and the device is configured to control the relay via MQTT only.

bool SKIP_LOCAL_PIR = false; // true to skip local PIR relay activation, false to allow local PIR relay activation

bool DEBUG = false; // Set to true for debug messages, false for normal operation

unsigned long lastMillis = 0;
unsigned long relayActivatedMillis = 0;

WiFiClient espClient;
PubSubClient client(espClient);

String mac;           // No colons, uppercase
String statusTopic;
String motionTopic;
String cmdTopic;

void debugPrint(const String &msg) {
#if DEBUG
  Serial.println(msg);
#endif
}

void setup_wifi() {
  delay(10);
  Serial.begin(115200);
  debugPrint("Connecting to Wi-Fi…");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    debugPrint("…still connecting");
  }
  debugPrint("Wi-Fi connected. IP: " + WiFi.localIP().toString());
}

void buildTopics() {
  mac = WiFi.macAddress();
  mac.replace(":", "");
  mac.toUpperCase();
  statusTopic = "home/" + String(GHAFEER_NAME) + "/" + mac + "/status";
  motionTopic = "home/" + String(GHAFEER_NAME) + "/" + mac + "/motion";
  cmdTopic    = "home/" + String(GHAFEER_NAME) + "/" + mac + "/cmd";
}

void callback(char* topic, byte* payload, unsigned int length) {
  String cmd = "";
  for (unsigned int i = 0; i < length; i++) cmd += (char)payload[i];
  cmd.trim();
  debugPrint("MQTT cmd: " + cmd);

  if (cmd == "REL_ON") {
    digitalWrite(RELAY_PIN, LOW); // Relay ON (active-low)
    relayActivatedMillis = millis();
    client.publish(statusTopic.c_str(), "Relay_ON");
  }
  else if (cmd == "REL_OFF") {
    digitalWrite(RELAY_PIN, HIGH); // Relay OFF (active-low)
    relayActivatedMillis = 0;
    client.publish(statusTopic.c_str(), "Relay_OFF");
  }
  else if (cmd == "REL_STATUS") {
    String st = digitalRead(RELAY_PIN) == LOW ? "ON" : "OFF"; // Relay ON if pin LOW
    client.publish(statusTopic.c_str(), ("Relay_Status:" + st).c_str());
  }
  else if (cmd.startsWith("PIR_INTERVAL:")) {
    PIR_INTERVAL = cmd.substring(13).toInt();
    String info = "PIR_INTERVAL_set:" + String(PIR_INTERVAL);
    client.publish(statusTopic.c_str(), info.c_str());
} 
  else if (cmd.startsWith("SKIP_LOCAL_RELAY:")) {
    String value = cmd.substring(17);
    if (value == "true" || value == "1") {
      SKIP_LOCAL_RELAY = true;
      client.publish(statusTopic.c_str(), "SKIP_LOCAL_RELAY set to true");
    } else if (value == "false" || value == "0") {
      SKIP_LOCAL_RELAY = false;
      client.publish(statusTopic.c_str(), "SKIP_LOCAL_RELAY set to false");
    } else {
      client.publish(statusTopic.c_str(), "Invalid SKIP_LOCAL_RELAY value");
    }

  } else if(cmd.startsWith("SKIP_LOCAL_PIR:")) {
    String value = cmd.substring(15);
    if (value == "true" || value == "1") {
      SKIP_LOCAL_PIR = true;
      client.publish(statusTopic.c_str(), "SKIP_LOCAL_PIR set to true");
    } else if (value == "false" || value == "0") {
      SKIP_LOCAL_PIR = false;
      client.publish(statusTopic.c_str(), "SKIP_LOCAL_PIR set to false");
    } else {
      client.publish(statusTopic.c_str(), "Invalid SKIP_LOCAL_PIR value");
    }

  }

  else if (cmd == "RESTART") {
    client.publish(statusTopic.c_str(), "Restarting...");
    delay(1000);
    ESP.restart();
  }
  
  else if (cmd.startsWith("DEBUG:")) {
    String debugValue = cmd.substring(6);
    if (debugValue == "true" || debugValue == "1") {
      DEBUG = true;
      client.publish(statusTopic.c_str(), "DEBUG mode enabled");
    } else if (debugValue == "false" || debugValue == "0") {
      DEBUG = false;
      client.publish(statusTopic.c_str(), "DEBUG mode disabled");
    } else {
      client.publish(statusTopic.c_str(), "Invalid DEBUG value");
    }
  }
  
  else if (cmd.startsWith("GHAFEER_NAME:")) {
    String newName = cmd.substring(13);
    if (newName.length() > 0) {
      GHAFEER_NAME = newName;
      buildTopics(); // Rebuild topics with the new name
      String msg = "GHAFEER_NAME set to: " + GHAFEER_NAME;
      client.publish(statusTopic.c_str(), msg.c_str());
    } else {
      client.publish(statusTopic.c_str(), "Invalid GHAFEER_NAME value");
    }
  }
  else if (cmd == "INFO" || cmd == "HELP") {
    String helpMsg = 
      "REL_ON:Relay ON | "
      "REL_OFF:Relay OFF | "
      "REL_STATUS:Get relay status | "
      "PIR_INTERVAL:<ms>:Set PIR interval | "
      "SKIP_LOCAL_RELAY:<true/false>:Local relay control | "
      "SKIP_LOCAL_PIR:<true/false>:Local PIR relay | "
      "RESTART:Restart | "
      "REBOOT:Reboot | "
      "STATUS:Device status | "
      "DEBUG:<true/false>:Debug mode | "
      "GHAFEER_NAME:<name>:Set name | "
      "HELP:Show help";
    client.publish(statusTopic.c_str(), helpMsg.c_str());
  }

  else if (cmd == "REBOOT") {
    client.publish(statusTopic.c_str(), "Rebooting...");
    delay(1000);
    ESP.restart();
  }
  else if (cmd == "STATUS") { 
    String relStatus = digitalRead(RELAY_PIN) == LOW ? "ON" : "OFF"; // Relay ON if pin LOW
    String info = "{";
    info += "\"GHAFEER_NAME\":\"" + String(GHAFEER_NAME) + "\",";
    info += "\"Device_Status\":\"Online\",";
    info += "\"MAC\":\"" + mac + "\",";
    info += "\"IP\":\"" + WiFi.localIP().toString() + "\",";
    info += "\"SKIP_LOCAL_RELAY\":" + String(SKIP_LOCAL_RELAY ? "true" : "false") + ",";
    info += "\"REL_Status\":\"" + relStatus + "\",";
    info += "\"PIR_INTERVAL\":" + String(PIR_INTERVAL) + ",";
    info += "\"SKIP_LOCAL_PIR\":" + String(SKIP_LOCAL_PIR ? "true" : "false") + ",";
    info += "\"DEBUG\":" + String(DEBUG ? "true" : "false");
    info += "}";
    client.publish(statusTopic.c_str(), info.c_str());
  }
  else {
    String info = "Unknown_CMD:" + cmd;
    client.publish(statusTopic.c_str(), info.c_str());
  }
}

void reconnect() {
  while (!client.connected()) {
    debugPrint("Connecting to MQTT…");
    if (client.connect(mac.c_str())) {
      client.subscribe(cmdTopic.c_str());
      debugPrint("MQTT connected, subscribed to: " + cmdTopic);
      client.publish(statusTopic.c_str(), "Device_Online", true); // retained
    } else {
      debugPrint("MQTT connect failed, rc=" + String(client.state()));
      delay(2000);
    }
  }
}

void handlePIR() {
  if (relayActivatedMillis == 0) {
    relayActivatedMillis = millis(); // Reset the timer when motion is detected

    if (!SKIP_LOCAL_RELAY) {
      digitalWrite(RELAY_PIN, LOW); // Relay ON (active-low)
      debugPrint("Motion ON, relay ON (local control)");
    } else {
      debugPrint("Motion detected, but SKIP_LOCAL_RELAY is enabled, not activating relay locally");
    }
    // digitalWrite(RELAY_PIN, HIGH);
    // debugPrint("Motion ON, relay ON");

    // Publish motion detection as JSON
    String payload = "{\"motion\":true,\"mac\":\"" + mac +
      "\",\"location\":\"" + String(GHAFEER_NAME) +
      "\",\"ip\":\"" + WiFi.localIP().toString() +
      "\",\"time\":" + String(millis()) + "}";
    client.publish(motionTopic.c_str(), payload.c_str());

    if (SKIP_LOCAL_RELAY) {
      client.publish(statusTopic.c_str(), "Motion detected, but SKIP_LOCAL_RELAY is enabled, not activating relay locally");
    } else {
      client.publish(statusTopic.c_str(), "Motion detected, relay activated");
    }
    // client.publish(statusTopic.c_str(), "Relay_ON");
  }
}

void checkRelayTimeout() {
  if (relayActivatedMillis > 0 && digitalRead(RELAY_PIN) == LOW) { // Relay ON if pin LOW
    if (millis() - relayActivatedMillis >= RELAY_MAX_ON_DURATION) {
      digitalWrite(RELAY_PIN, HIGH); // Relay OFF (active-low)
      relayActivatedMillis = 0;
      client.publish(statusTopic.c_str(), "Relay_OFF (timer expired)");
      debugPrint("Relay OFF (timer expired)");
    }
  }
}

void setup() {
  // pinMode(PIR_PIN, INPUT_PULLUP);
  pinMode(PIR_PIN,   INPUT);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH); // Relay OFF at startup (active-low)

  // set baud rate for serial communication
  Serial.begin(115200);
  

  debugPrint("Starting setup...");

  setup_wifi();
  buildTopics();

  client.setServer("192.168.1.246", 1883); // RPi broker IP
  client.setCallback(callback);
  debugPrint("Setup complete");
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  unsigned long now = millis();

  checkRelayTimeout();

  if (!SKIP_LOCAL_PIR) {

    if (now - lastMillis >= PIR_INTERVAL) {
      bool motion = (digitalRead(PIR_PIN) == HIGH);
      if (motion) {
        relayActivatedMillis = 0;
        lastMillis = now;
        handlePIR();
      }
    }
  }

}
