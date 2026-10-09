import requests

class ESP32Controller:
    def __init__(self, esp_ip, timeout=None):
        self.esp_ip = esp_ip
        self.timeout = timeout
        self.base_url = f"http://{esp_ip}"
    
    def send(self, endpoint):
        url = f"{self.base_url}/{endpoint}"
        try:
            r = requests.get(url, timeout=self.timeout)
            print(f"✅ ESP32: {endpoint} -> {r.status_code}")
            return True
        except Exception as e:
            print(f"❌ ESP32: {endpoint} failed - {e}")
            return False
    
    def idle(self): return self.send("idle")
    def listening(self): return self.send("listening")
    def positive(self): return self.send("positive")
    def neutral(self): return self.send("neutral")
    def negative(self): return self.send("negative")