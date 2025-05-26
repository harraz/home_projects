import socket
import threading
import logging

# ------------------------------------------------------------------------
LISTEN_IP     = "0.0.0.0"
LISTEN_PORT   = 8080
ACTUATOR_PORT = 12345

# PER-STATION CONFIG
STATION_CONFIG = {
    "BASYOUNEE": {
        "ip": "192.168.1.197",
        "on_cmd": "OPEN_VALVE",
        "off_cmd": "CLOSE_VALVE",
        "off_delay": 15,
    },
    "ASHRAF": {
        "ip": "192.168.1.197",
        "on_cmd": "OPEN_VALVE",
        "off_cmd": "CLOSE_VALVE",
        "off_delay": 15,
    },
    # …
}
# ------------------------------------------------------------------------

logging.basicConfig(level=logging.INFO,
                    format="%(asctime)s %(levelname)s %(message)s")


def parse_fields(text: str) -> dict:
    data = {}
    for chunk in text.strip().split():
        if ':' not in chunk:
            continue
        key, val = chunk.split(':', 1)
        data[key] = val
    return data


def send_cmd(ip: str, cmd: str):
    """Send a single UDP command to the given actuator IP."""
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        sock.sendto(cmd.encode(), (ip, ACTUATOR_PORT))
        logging.info("Sent '%s' to %s:%d", cmd, ip, ACTUATOR_PORT)
    except Exception as e:
        logging.error("Failed to send '%s' to %s: %s", cmd, ip, e)
    finally:
        sock.close()


def handle_packet(raw: bytes, addr):
    text   = raw.decode(errors='ignore')
    fields = parse_fields(text)

    # Determine message type and station name
    if "MD" in fields:
        msg_type, station = "MD", fields["MD"]
    elif "ACK" in fields:
        msg_type, station = "ACK", fields["ACK"]
    else:
        logging.warning("Unknown message type from %s: %r", addr, text)
        return

    recv_ip = fields.get("IP")
    tstamp  = fields.get("Time")

    logging.info("Received %s from '%s' (IP=%s, Time=%s)",
                 msg_type, station, recv_ip, tstamp)

    config = STATION_CONFIG.get(station)
    if not config:
        logging.error("No config for station '%s'", station)
        return

    ip = config["ip"]

    if msg_type == "MD":
        on_cmd    = "CMD:" + config["on_cmd"]
        off_cmd   = "CMD:" + config["off_cmd"]
        off_delay = config["off_delay"]

        # Log motion event
        try:
            if tstamp is not None:
                ms = int(tstamp)
                logging.info("Motion at '%s' (+%.2f s) dispatch ON", station, ms/1000.0)
            else:
                logging.info("Motion at '%s' (Time=%s) dispatch ON", station, tstamp)
        except Exception:
            logging.info("Motion at '%s' (Time=%s) dispatch ON", station, tstamp)

        # send ON
        send_cmd(ip, on_cmd)

        # schedule OFF
        timer = threading.Timer(off_delay, send_cmd, args=(ip, off_cmd))
        timer.daemon = True
        timer.start()

    else:  # ACK
        logging.info("ACK received from '%s'", station)


def listener():
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind((LISTEN_IP, LISTEN_PORT))
    logging.info("Dispatcher listening on UDP %s:%d", LISTEN_IP, LISTEN_PORT)
    while True:
        data, addr = sock.recvfrom(1024)
        threading.Thread(target=handle_packet,
                         args=(data, addr),
                         daemon=True).start()


if __name__ == "__main__":
    try:
        listener()
    except KeyboardInterrupt:
        logging.info("Shutting down.")


