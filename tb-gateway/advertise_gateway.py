import socket, time
from zeroconf import ServiceInfo, Zeroconf

def get_local_ip():
    try:
        # Bước 1: Lấy danh sách TẤT CẢ các IP đang có trên Laptop
        hostname = socket.gethostname()
        _, _, all_ips = socket.gethostbyname_ex(hostname)
        # Lọc bỏ IP ảo localhost của hệ thống
        valid_ips = [ip for ip in all_ips if not ip.startswith("127.")]

        # Bước 2: Tìm IP của Subnet A (IP đang dùng để ra Internet)
        # Bằng cách giả vờ kết nối đến DNS của Google
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        try:
            s.connect(("8.8.8.8", 80))
            internet_ip = s.getsockname()[0]
        except Exception:
            internet_ip = None
        finally:
            s.close()

        # Bước 3: Thuật toán loại trừ để tìm Subnet B
        hotspot_ip = None
        for ip in valid_ips:
            # Nếu IP này KHÁC với IP đang ra Internet, đó chính là Hotspot
            if ip != internet_ip:
                hotspot_ip = ip
                break
        
        # Đặc cách cho Windows: Đôi khi nó ưu tiên dải 192.168.137.x
        for ip in valid_ips:
            if ip.startswith("192.168.137."):
                hotspot_ip = ip

        # Kết quả cuối cùng
        if hotspot_ip:
            return hotspot_ip
        elif internet_ip:
            return internet_ip # Fallback an toàn
        else:
            return "127.0.0.1"

    except Exception as e:
        print(f"[Error] Không thể lấy IP: {e}")
        return "127.0.0.1"

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