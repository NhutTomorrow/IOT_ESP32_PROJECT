import socket, time
from zeroconf import ServiceInfo, Zeroconf

def get_local_ip():
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.connect(("8.8.8.8", 80))
    ip = s.getsockname()[0]
    s.close()
    return ip

ip = get_local_ip()
print(f"[mDNS] Broadcasting gateway at {ip}:1883")

info = ServiceInfo(
    "_mqtt._tcp.local.",
    "IoTGateway._mqtt._tcp.local.",
    addresses=[socket.inet_aton(ip)],
    port=1883,
    properties={"role": "iot-gateway"},
)

zc = Zeroconf()
zc.register_service(info)
print("[mDNS] Running... Ctrl+C to stop")
try:
    while True:
        time.sleep(1)
except KeyboardInterrupt:
    zc.unregister_service(info)
    zc.close()