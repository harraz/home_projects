#include <ESP8266WiFi.h>
#include <WiFiUdp.h>

// WiFi credentials
const char* ssid = "Portsaid";
const char* password = "****";

// UDP setup
WiFiUDP udp;
const unsigned int localPort = 12345; // Port for listening for incoming UDP packets

// Store the IP and port of the last UDP sender
IPAddress lastClientIP;
unsigned int lastClientPort;

void setup() {
  Serial.begin(115200); // Serial communication with Arduino
  WiFi.begin(ssid, password);

  // Connect to WiFi
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }

  Serial.println("Connected to Wi-Fi");
  Serial.println(WiFi.localIP());

  // Start UDP server
  udp.begin(localPort);
  Serial.printf("Listening for UDP packets on IP %s, port %d\n", WiFi.localIP().toString().c_str(), localPort);

  // Set Serial timeout
  Serial.setTimeout(100);
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

    // Store the sender's IP and port
    lastClientIP = udp.remoteIP();
    lastClientPort = udp.remotePort();

    // Forward the received message to the Arduino over Serial
    // Serial.println(incomingPacket);
    Serial.printf("%s:%s\n", incomingPacket, lastClientIP.toString().c_str());

    // Serial.printf("Received message: <%s> from %s:%d\n", incomingPacket, lastClientIP.toString().c_str(), lastClientPort);
  }


  // Check for Serial input from Arduino
  if (Serial.available()) {
    String arduinoMessage = Serial.readStringUntil('\n');
    arduinoMessage.trim(); // Remove any trailing newline or whitespace
  
    // Parse the received message into IP and message parts
    int separatorIndex = arduinoMessage.indexOf(':');
    if (separatorIndex != -1) {
      String message = arduinoMessage.substring(0, separatorIndex);
      String senderIP = arduinoMessage.substring(separatorIndex + 1);
    }

    // Send Arduino's message back to the last UDP sender
    if (lastClientIP != IPAddress(0, 0, 0, 0)) {

      if (udp.beginPacket(lastClientIP, 8080)) {
        udp.print(arduinoMessage + "\n");
        udp.endPacket();
        }

    }

  
  }

  // Check for Serial input from Arduino
  // if (Serial.available()) {
  //   String arduinoMessage = Serial.readStringUntil('\n');
  //   arduinoMessage.trim(); // Remove any trailing newline or whitespace

  //   // Send Arduino's message back to the last UDP sender
  //   if (lastClientIP != IPAddress(0, 0, 0, 0)) {
  //     if (udp.beginPacket(lastClientIP, 8080)) {
  //       udp.print(arduinoMessage);
  //       if (udp.endPacket()) {
  //         Serial.printf("Sent message to %s:%d: %s\n", lastClientIP.toString().c_str(), lastClientPort, arduinoMessage.c_str());
  //       } else {
  //         Serial.println("Failed to send UDP packet.");
  //       }
  //     } else {
  //       Serial.println("Failed to start UDP packet.");
  //     }
  //   } else {
  //     Serial.println("No valid client IP found. Cannot send message.");
  //   }
  // }
}
