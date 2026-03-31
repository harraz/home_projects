#include <ESP8266WiFi.h>
#include <EEPROM.h>
#include <PubSubClient.h>
#include "secrets.h"   // WIFI_SSID, WIFI_PASSWORD

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <ArduinoJson.h>

JsonDocument doc;

const int RELAY_PIN  = 12;  // D6

String GHAFEER_NAME = "ABBAS";  // change this to your ghafeer name
bool relayOn = false;
unsigned long lastRelayOnMs = 0;
bool DEBUG = false;

// The actual ON time used for this wake cycle after randomization.
unsigned int RELAY_ON_DURATION_MS = 10000;   // actual randomized duration, capped at 10s
// Lowest random relay ON time we allow.
const unsigned int RELAY_ON_MIN_DURATION_MS = 7000;
// Highest random relay ON time we allow.
const unsigned int RELAY_ON_MAX_DURATION_MS = 10000;
const unsigned long AWAKE_WINDOW_MS      = 160000;   // total time awake before sleep
const unsigned long WIFI_CONNECT_TIMEOUT_MS = 18000;
const unsigned long MQTT_CONNECT_TIMEOUT_MS = 8000;
const unsigned long TIME_SYNC_TIMEOUT_MS = 15000;
// Ignore repeated wake triggers that happen too soon after the last accepted one.
const unsigned long TRIGGER_COOLDOWN_MS = 5000;
const char* MQTT_SERVER = "192.168.1.246";
const int   MQTT_PORT   = 1883;
// Marker written to EEPROM so we can tell whether the stored data is ours.
const uint32_t EEPROM_STATE_MAGIC = 0x47524652;
const int EEPROM_SIZE_BYTES = 64;
const int EEPROM_STATE_ADDR = 0;

WiFiClient espClient;
PubSubClient client(espClient);

String mac;
String statusTopic;
String motionTopic;
String cmdTopic;

// Small EEPROM-persisted struct that survives resets and power loss.
struct PersistedThrottleState {
  // Fixed marker used to validate that EEPROM contains our struct.
  uint32_t magic;
  // Simple integrity check so corrupted persisted content gets ignored.
  uint32_t checksum;
  // Unix epoch seconds recorded for the last accepted wake event.
  uint32_t lastAcceptedEpoch;
  // Number of wake events suppressed since the last accepted wake.
  uint32_t suppressedWakeCount;
};

uint32_t calculateChecksum(const PersistedThrottleState &state) {
  // Cheap checksum good enough to reject obviously invalid persisted data.
  return state.magic ^ state.lastAcceptedEpoch ^ state.suppressedWakeCount ^ 0xA5A5A5A5;
}

bool readPersistedThrottleState(PersistedThrottleState &state) {
  // Read the stored struct from EEPROM.
  EEPROM.get(EEPROM_STATE_ADDR, state);

  // Reject persisted content that does not match our expected marker.
  if (state.magic != EEPROM_STATE_MAGIC) {
    return false;
  }

  // Reject persisted content if the checksum does not match.
  return state.checksum == calculateChecksum(state);
}

void writePersistedThrottleState(const PersistedThrottleState &sourceState) {
  // Copy the caller's state so we can fill the checksum before writing.
  PersistedThrottleState state = sourceState;
  // Fill in the checksum after the rest of the struct is ready.
  state.checksum = calculateChecksum(state);
  // Persist the record in EEPROM so it survives resets and power loss.
  EEPROM.put(EEPROM_STATE_ADDR, state);
  EEPROM.commit();
}

bool syncTime() {
  // Fetch wall-clock time after Wi-Fi connect so cooldown uses real elapsed seconds.
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  unsigned long start = millis();

  while (millis() - start < TIME_SYNC_TIMEOUT_MS) {
    time_t now = time(nullptr);
    if (now > 1700000000) {
      return true;
    }
    delay(250);
  }

  return false;
}

bool isTriggerThrottled(time_t nowEpoch, PersistedThrottleState &state) {
  // If there is no valid stored state, this wake cannot be throttled yet.
  if (!readPersistedThrottleState(state)) {
    return false;
  }

  // Throttle this wake if it happened inside the cooldown window.
  return nowEpoch >= state.lastAcceptedEpoch &&
         static_cast<uint32_t>(nowEpoch - state.lastAcceptedEpoch) < (TRIGGER_COOLDOWN_MS / 1000UL);
}

void recordSuppressedWake() {
  PersistedThrottleState state;

  // If persisted state is missing, create a valid empty record first.
  if (!readPersistedThrottleState(state)) {
    state = {EEPROM_STATE_MAGIC, 0, 0, 0};
  }

  // Count this blocked wake so we can report it on the next accepted one.
  state.suppressedWakeCount++;
  writePersistedThrottleState(state);
}

void debugPrint(const String &msg) {
  if (DEBUG) Serial.println(msg);
}

void publishStatusStep(const String &msg) {
  if (client.connected()) {
    client.publish(statusTopic.c_str(), msg.c_str());
  }
}

bool setup_wifi() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - t0 < WIFI_CONNECT_TIMEOUT_MS)) {
    delay(500);
    debugPrint("Connecting...");
  }
  if (WiFi.status() != WL_CONNECTED) {
    debugPrint("Wi-Fi connect timeout");
    return false;
  }
  debugPrint("Wi-Fi connected: " + WiFi.localIP().toString());
  return true;
}

void buildTopics() {
  mac = WiFi.macAddress();
  mac.replace(":", "");
  mac.toUpperCase();
  statusTopic = "home/" + GHAFEER_NAME + "/" + mac + "/status";
  motionTopic = "home/" + GHAFEER_NAME + "/" + mac + "/motion";
  cmdTopic    = "home/" + GHAFEER_NAME + "/" + mac + "/cmd";
}

void callback(char* topic, byte* payload, unsigned int length) {
  String cmd; cmd.reserve(length+1);
  for (unsigned int i=0; i<length; i++) cmd += (char)payload[i];
  cmd.trim();

  if (cmd == "REL_ON") {
    digitalWrite(RELAY_PIN, HIGH);
    relayOn = true;
    lastRelayOnMs = millis();
    client.publish(statusTopic.c_str(), "Relay forced ON (MQTT)");
  } 
  else if (cmd == "REL_OFF") {
    digitalWrite(RELAY_PIN, LOW);
    relayOn = false;
    client.publish(statusTopic.c_str(), "Relay forced OFF (MQTT)");
  }
  else if (cmd == "PING") {
    client.publish(statusTopic.c_str(), "Awake and responding");
  }
}

void goToSleep(bool publishStatus = true) {
  // Optionally publish a final status before sleeping if MQTT is available.
  if (publishStatus && client.connected()) {
    client.publish(statusTopic.c_str(), "Going to deep sleep...");
    delay(150);
    client.disconnect();
  }
  // Shut Wi-Fi down cleanly to reduce power and avoid stale connections.
  if (WiFi.isConnected()) {
    WiFi.disconnect(true);
  }
  debugPrint("Sleeping...");
  delay(2000);  // wait 2s to let PIR output go LOW
  ESP.deepSleep(0);   // forever, until RST triggered (PIR)
}

void setup() {
  digitalWrite(RELAY_PIN, LOW);  // preset output level before enabling pin to avoid boot pulse
  pinMode(RELAY_PIN, OUTPUT);
  Serial.begin(115200);
  EEPROM.begin(EEPROM_SIZE_BYTES);
  debugPrint("Booting after motion...");

  if (!setup_wifi()) {
    goToSleep(false);
  }
  buildTopics();

  client.setServer(MQTT_SERVER, MQTT_PORT);
  client.setCallback(callback);

  unsigned long mqttStart = millis();
  while (!client.connected() && (millis() - mqttStart < MQTT_CONNECT_TIMEOUT_MS)) {
    client.connect(mac.c_str());
    delay(500);
  }
  if (!client.connected()) {
    debugPrint("MQTT connect timeout, sleeping");
    goToSleep(false);
  }

  publishStatusStep("Boot complete: WiFi and MQTT connected");

  PersistedThrottleState persistedState;
  uint32_t suppressedWakeCount = 0;
  // Recover the number of suppressed wakes so we can publish one summary now.
  if (readPersistedThrottleState(persistedState)) {
    suppressedWakeCount = persistedState.suppressedWakeCount;
    publishStatusStep("Persisted state loaded");
  } else {
    persistedState = {EEPROM_STATE_MAGIC, 0, 0, 0};
    publishStatusStep("Persisted state missing; starting fresh");
  }

  if (!syncTime()) {
    client.publish(statusTopic.c_str(), "Time sync failed; skipping throttle");
  } else {
    time_t nowEpoch = time(nullptr);
    publishStatusStep("Time sync OK; epoch:" + String(static_cast<unsigned long>(nowEpoch)));

    // Reject repeated wakes based on real wall-clock time from NTP.
    if (isTriggerThrottled(nowEpoch, persistedState)) {
      // Remember that this wake was suppressed so the next accepted wake can report it.
      recordSuppressedWake();
      client.publish(statusTopic.c_str(), "Wake suppressed: cooldown active");
      goToSleep(false);
    }

    if (persistedState.lastAcceptedEpoch > 0) {
      publishStatusStep("Wake accepted; last accepted epoch:" + String(persistedState.lastAcceptedEpoch));
    } else {
      publishStatusStep("Wake accepted; no previous accepted epoch");
    }
  }


  // Stay awake for both relay duration and MQTT commands
  unsigned long start = millis();
// Remove or comment out:
// srand(time(NULL));
// RELAY_ON_DURATION_MS = rand() % RELAY_ON_DURATION_MS + 7000;

  // Size of the inclusive random range, e.g. 7000..10000 ms.
  uint32_t randomRange = RELAY_ON_MAX_DURATION_MS - RELAY_ON_MIN_DURATION_MS + 1;
  // Pick one random relay ON duration inside the allowed range for this wake.
  RELAY_ON_DURATION_MS = RELAY_ON_MIN_DURATION_MS + (ESP.random() % randomRange);

  doc["motion"] = true;
  doc["mac"] = mac;
  doc["location"] = GHAFEER_NAME;
  doc["ip"] = WiFi.localIP().toString();
  doc["relay_duration_ms"] = RELAY_ON_DURATION_MS;
  doc["awake_window_ms"] = AWAKE_WINDOW_MS;

  // Publish motion message
  String payload;
  serializeJson(doc, payload);

  time_t acceptedEpoch = time(nullptr);
  // Reset the persisted throttle state for this newly accepted wake.
  if (acceptedEpoch > 1700000000) {
    persistedState.magic = EEPROM_STATE_MAGIC;
    persistedState.lastAcceptedEpoch = static_cast<uint32_t>(acceptedEpoch);
    persistedState.suppressedWakeCount = 0;
    writePersistedThrottleState(persistedState);
  }

  // Publish a deferred summary of wakes suppressed during the previous cooldown window.
  if (suppressedWakeCount > 0) {
    String suppressedMsg = "Suppressed_wakes:" + String(suppressedWakeCount);
    client.publish(statusTopic.c_str(), suppressedMsg.c_str());
  }
  publishStatusStep("Relay duration ms:" + String(RELAY_ON_DURATION_MS));
  client.publish(motionTopic.c_str(), payload.c_str());
  publishStatusStep("Motion event published");

  // Relay ON from local motion trigger
  digitalWrite(RELAY_PIN, HIGH);
  relayOn = true;
  lastRelayOnMs = millis();
  client.publish(statusTopic.c_str(), "Relay ON (local motion trigger)");

  while (millis() - start < AWAKE_WINDOW_MS) {
    client.loop();

    // Turn relay OFF when duration elapsed
    if (relayOn && millis() - lastRelayOnMs >= RELAY_ON_DURATION_MS) {
      digitalWrite(RELAY_PIN, LOW);
      relayOn = false;
      client.publish(statusTopic.c_str(), "Relay OFF (timer expired)");
      publishStatusStep("Relay timer expired");
    }
    delay(10);
  }

  goToSleep();
}

void loop() {}
