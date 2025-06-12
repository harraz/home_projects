import paho.mqtt.client as mqtt
import time
import threading
import json

# Mapping: source ESP MAC -> list of (target location, target MAC, relay ON time)
TRIGGERS = {
    "112233AABBCC": [("garage", "AABBCCDDEEFF", 10)],  # example: hallway ESP triggers garage relay
    "334455667788": [("livingroom", "112211221122", 15), ("garage", "AABBCCDDEEFF", 8)],
    # add more mappings as needed
}

BROKER = "localhost"  # or Pi IP

def on_motion(client, userdata, msg):
    try:
        payload = msg.payload.decode()
        print(f"[Motion] {msg.topic}: {payload}")
        data = json.loads(payload)
        source_mac = data.get("mac", "")

        if source_mac not in TRIGGERS:
            print(f"No relay trigger configured for source MAC: {source_mac}")
            return

        for target_location, target_mac, delay in TRIGGERS[source_mac]:
            relay_cmd_topic = f"home/{target_location}/{target_mac}/cmd"
            print(f"[Relay] Sending REL_ON to {relay_cmd_topic}")
            client.publish(relay_cmd_topic, "REL_ON")

            def delayed_off(topic=relay_cmd_topic, delay=delay):
                time.sleep(delay)
                print(f"[Relay] Sending REL_OFF to {topic}")
                client.publish(topic, "REL_OFF")
            threading.Thread(target=delayed_off, daemon=True).start()

    except Exception as e:
        print(f"Error handling motion: {e}")

def main():
    client = mqtt.Client()
    client.on_message = on_motion

    print(f"Connecting to MQTT broker at {BROKER}...")
    client.connect(BROKER, 1883, 60)

    motion_topic = "home/+/+/motion"
    client.subscribe(motion_topic)
    print(f"Subscribed to: {motion_topic}")

    client.loop_forever()

if __name__ == "__main__":
    main()
