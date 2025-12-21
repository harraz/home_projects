import paho.mqtt.client as mqtt
import time
import threading
import json
from datetime import datetime
import pygame


# Mapping: source ESP MAC -> list of (target location, target MAC, relay ON time)
TRIGGERS = {
    # "BCDDC257015B": [("ASHRAF", "CC50E35334BA", 40)], # /BASYOUNEE>ASHRAF
    "40F520F27E93": [ ("MAHROOS", "E868E7C95A50", 2)], # /SHANKAL>ASHRAF
    # "8C4F00428981": [("ASHRAF", "CC50E35334BA", 30)], # /ABBAS>ASHRAF
    "BCDDC23DD032": [("ASHRAF", "CC50E35334BA", 15)], # /MARZOOQ>ASHRAF
    # "8C4F00428BBE": [("ASHRAF", "CC50E35334BA", 30), ("MARZOOQ", "BCDDC23DD032", 25)], # /MAGHAWOREE>MARZOOQ
    # "8C4F00428BBE": [("ASHRAF", "CC50E35334BA", 30)], # /MAGHAWOREE>ASHRAF
    # "483FDA62A988": [("MAHROOS", "E868E7C95A50", 2)], # /DAHROOG/MAHROOS
    #"483FDA62A988": [("MARZOOQ", "BCDDC23DD032", 10), ("MAHROOS", "E868E7C95A50", 2)], # /DAHROOG>MARZOOQ/MAHROOS
    # "F008D101FFF0": [("MAHROOS", "E868E7C95A50", 2)], # /RAGAB>MAHROOS
    "84F3EB865A55" : [("MAHROOS", "E868E7C95A50", 2)], # /3ANTTAR>MAHROOS
    # "40F520F27E93" : [("MAHROOS", "E868E7C95A50", 2)], # /SHANKAL>MAHROOS

    # add more mappings as needed
}

BROKER = "localhost"

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
            
            if ('483FDA62A988' in source_mac): #DAHROOG
                play_sound('alarm-no3-14864.mp3')
            elif ('84F3EB865A55' in source_mac): #3ANTTAR home/3ANTTAR/84F3EB865A55/motion:
                play_sound('notification-bell-sound-1-376885.mp3')
            else:
                #play_sound('warning-alarm-loop-1-279206.mp3')
                play_sound('snd_fragment_retrievewav-14728.mp3')
                
    except Exception as e:
        print(f"Error handling motion: {e}")

def play_sound(file_name: str):
    fixed_path = '/home/harraz/Downloads/'
    full_path = fixed_path + file_name

    # Stop the music if already playing
    if pygame.mixer.music.get_busy():
        #time.sleep(10)
        pygame.mixer.music.stop()

    pygame.mixer.music.load(full_path)
    pygame.mixer.music.play()



def main():
    client = mqtt.Client()
    client.on_message = on_motion

    print(f"Connecting to MQTT broker at {BROKER}...")
    client.connect(BROKER, 1883, 60)

    motion_topic = "home/+/+/motion"
    client.subscribe(motion_topic)
    print(f"Subscribed to: {motion_topic}")
    
    pygame.mixer.init()

    client.loop_forever()

if __name__ == "__main__":
    main()
