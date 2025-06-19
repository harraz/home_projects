import paho.mqtt.client as mqtt
import time
import threading
import json
from datetime import datetime

# Mapping: source ESP MAC -> list of (target location, target MAC, relay ON time)
TRIGGERS = {
    "BCDDC257015B": [("ASHRAF", "CC50E35334BA", 10)], # /BASYOUNEE>ASHRAF
    "CC50E35334BA": [("ABBAS", "483FDA5CC67A", 30)], # /ASHRAF>ABBAS
    "84F3EB4F23C2": [("ASHRAF", "CC50E35334BA", 30)], # /HAGRRAS>ASHRAF
    
    # add more mappings as needed
}

BROKER = "localhost"  # or Pi IP

def on_motion(client, userdata, msg):
    
    current_timestamp=(datetime.now().strftime('%Y-%m-%d %H:%M:%S'))

    try:
        payload = msg.payload.decode()
        print(f"{current_timestamp} - [Motion] {msg.topic}: {payload}")
        data = json.loads(payload)
        source_mac = data.get("mac", "")

        if source_mac not in TRIGGERS:
            print(f"No relay trigger configured for source MAC: {source_mac}")
            return

        for target_location, target_mac, delay in TRIGGERS[source_mac]:
            relay_cmd_topic = f"home/{target_location}/{target_mac}/cmd"
            print(f"{current_timestamp} - [Relay] Sending REL_ON to {relay_cmd_topic}")
            client.publish(relay_cmd_topic, "REL_ON")

            def delayed_off(topic=relay_cmd_topic, delay=delay):
                time.sleep(delay)
                print(f"{current_timestamp} - [Relay] Sending REL_OFF to {topic}")
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
