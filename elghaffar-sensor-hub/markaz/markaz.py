import paho.mqtt.client as mqtt
import time
import threading
import json

# Configure relay details here
RELAY_LOCATION = "garage"
RELAY_MAC = "DC4F22112233"        # Replace with your relay ESP MAC (no :)
RELAY_TIMEOUT = 15                # seconds relay stays ON

BROKER = "localhost"              # or use your Pi's LAN IP, e.g., "192.168.1.100"

def on_motion(client, userdata, msg):
    try:
        payload = msg.payload.decode()
        print(f"[Motion] {msg.topic}: {payload}")

        # Optionally, parse JSON payload
        data = json.loads(payload)
        # Decide which relay to control (expand logic if needed)

        # Build relay cmd topic
        relay_cmd_topic = f"home/{RELAY_LOCATION}/{RELAY_MAC}/cmd"

        # Send REL_ON
        print(f"[Relay] Sending REL_ON to {relay_cmd_topic}")
        client.publish(relay_cmd_topic, "REL_ON")

        # After RELAY_TIMEOUT, send REL_OFF (in a separate thread)
        def delayed_off():
            time.sleep(RELAY_TIMEOUT)
            print(f"[Relay] Sending REL_OFF to {relay_cmd_topic}")
            client.publish(relay_cmd_topic, "REL_OFF")
        threading.Thread(target=delayed_off, daemon=True).start()

    except Exception as e:
        print(f"Error handling motion: {e}")

def main():
    client = mqtt.Client()
    client.on_message = on_motion

    print(f"Connecting to MQTT broker at {BROKER}...")
    client.connect(BROKER, 1883, 60)

    # Subscribe to all motion topics
    motion_topic = "home/+/+/motion"
    client.subscribe(motion_topic)
    print(f"Subscribed to: {motion_topic}")

    client.loop_forever()

if __name__ == "__main__":
    main()
