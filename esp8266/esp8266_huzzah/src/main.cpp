#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <secrets.h>

const char* ghafeerName = "ROVER1";  // Device name

// WiFi credentials are now defined in secrets.h
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;

int PIRSensorOutputPin = 2; // GPIO2 (D4 on most ESP8266 modules)
bool isTriggered = false;  // To track PIR state

// UDP setup
WiFiUDP udp;
const unsigned int localPort = 12345; // Port for listening for incoming UDP packets
const unsigned int targetPort = 8080; // Port to send outgoing UDP messages

// Store the IP and port of the last UDP sender
IPAddress lastClientIP;
bool lastClientKnown = false; // Flag to check if we have a client IP

// Define DEBUG flag
#define DEBUG 0  // Set to 1 to enable debug prints, 0 to disable them

void setup() {
  pinMode(PIRSensorOutputPin, INPUT); // PIR sensor input
  Serial.begin(9600);              // Serial communication for debugging

  #if DEBUG
  Serial.println("Starting up...");
  #endif

  WiFi.begin(ssid, password);

  // Connect to WiFi with a timeout to prevent indefinite blocking
  unsigned long startAttemptTime = millis();
  unsigned long timeout = 30000;  // 30 seconds timeout

  #if DEBUG
  Serial.println("Attempting to connect to WiFi...");
  #endif

  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < timeout) {
    delay(1000);
    #if DEBUG
    Serial.println("Connecting to WiFi...");
    #endif
  }

  if (WiFi.status() == WL_CONNECTED) {
    #if DEBUG
    Serial.println("Connected to Wi-Fi");
    Serial.println(WiFi.localIP());
    #endif
  } else {
    #if DEBUG
      Serial.println("Failed to connect to Wi-Fi.");
      Serial.print("Error code: ");
      Serial.println(WiFi.status());
    #endif
  }

  // Start UDP server
  udp.begin(localPort);
  #if DEBUG
  Serial.printf("Listening for UDP packets on IP %s, port %d\n", WiFi.localIP().toString().c_str(), localPort);
  #endif
}

void loop() {
  // Check for incoming UDP messages
  int packetSize = udp.parsePacket();
  if (packetSize) {
    char incomingPacket[255];
    int len = udp.read(incomingPacket, sizeof(incomingPacket) - 1);
    if (len > 0) {
      incomingPacket[len] = '\0'; // Null-terminate the string
    }

    lastClientIP = udp.remoteIP();
    lastClientKnown = true; // Mark that we have a valid client IP

    #if DEBUG
    Serial.printf("Received message: <%s> from %s:%d\t", incomingPacket, lastClientIP.toString().c_str(), udp.remotePort());
    #endif

    udp.beginPacket(lastClientIP, targetPort);
    udp.printf("ack:%s\n", ghafeerName);
    udp.endPacket();

    #if DEBUG
    Serial.printf("Acknowledgment sent from: %s\n", ghafeerName);
    #endif

    // send command to Arduino via serial TX/RX
    Serial.printf(incomingPacket);

  }


// Check for Serial input from Arduino
  if (Serial.available()) {
    String arduinoMessage = Serial.readStringUntil('\n');
    arduinoMessage.trim(); // Remove any trailing newline or whitespace

    // Send Arduino's message back to the last UDP sender
    if (lastClientIP != IPAddress(0, 0, 0, 0)) {
      if (udp.beginPacket(lastClientIP, targetPort)) {
        udp.println(arduinoMessage);
        if (!udp.endPacket()) {
          Serial.println("Failed to send UDP packet.");
        }
      } else {
        Serial.println("Failed to start UDP packet.");
      }
    } else {
      Serial.println("No valid client IP found. Cannot send message.");
    }
  }

  // PIR sensor logic
  if (digitalRead(PIRSensorOutputPin) == HIGH) { // Motion detected
    if (!isTriggered) { // Avoid repeated triggers
      isTriggered = true;
      #if DEBUG
      Serial.printf("Motion detected from %s\n", ghafeerName);
      #endif

      // Send motion-detected message via UDP
      if (lastClientKnown) {
        udp.beginPacket(lastClientIP, targetPort);
        udp.printf("MD:%s\n", ghafeerName);
        udp.endPacket();
        #if DEBUG
        Serial.printf("Motion detected message sent by: %s\n", ghafeerName);
        #endif
      }
    }
  } else { // No motion
    if (isTriggered) { // Reset the trigger state
      isTriggered = false;
      #if DEBUG
      Serial.println("Motion stopped.");
      #endif
    }
  }

  delay(100); // Small delay for stability
}

