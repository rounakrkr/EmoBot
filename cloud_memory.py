import os
import requests
from datetime import datetime
from dotenv import load_dotenv

# Load variables from .env file
load_dotenv()

class CloudMemory:
    def __init__(self, bin_id=None, api_key=None):
        self.bin_id = bin_id or os.getenv("JSONBIN_BIN_ID")
        self.api_key = api_key or os.getenv("JSONBIN_API_KEY")
        
        if not self.bin_id or not self.api_key:
            print("⚠️ JSONBin credentials missing. Cloud memory disabled.")
            self.enabled = False
            return
        
        self.enabled = True
        self.base_url = f"https://api.jsonbin.io/v3/b/{self.bin_id}"
        self.headers = {
            'Content-Type': 'application/json',
            'X-Master-Key': self.api_key,
            'X-Bin-Meta': 'false'
        }
        print("✅ Cloud memory initialized")
    
    def _get_data(self):
        if not self.enabled:
            return {"user_memory": {}, "conversation_log": []}
        
        try:
            response = requests.get(self.base_url, headers=self.headers)
            return response.json() if response.status_code == 200 else {"user_memory": {}, "conversation_log": []}
        except Exception as e:
            print(f"⚠️ Cloud read error: {e}")
            return {"user_memory": {}, "conversation_log": []}
    
    def _save_data(self, data):
        if not self.enabled:
            return False
        
        try:
            response = requests.put(self.base_url, json=data, headers=self.headers)
            return response.status_code == 200
        except Exception as e:
            print(f"⚠️ Cloud write error: {e}")
            return False
    
    def remember(self, key, value):
        data = self._get_data()
        data["user_memory"][key] = {
            "value": value,
            "timestamp": datetime.now().isoformat()
        }
        if self._save_data(data):
            print(f"☁️ Cloud: Remembered '{key}' = '{value}'")
            return True
        return False
    
    def recall(self, key):
        data = self._get_data()
        return data["user_memory"].get(key, {}).get("value")
    
    def forget(self, key):
        data = self._get_data()
        if key in data["user_memory"]:
            del data["user_memory"][key]
            self._save_data(data)
            print(f"☁️ Cloud: Forgotten '{key}'")
            return True
        return False
    
    def get_all(self):
        return self._get_data().get("user_memory", {})
    
    def log_conversation(self, user_msg, bot_msg):
        data = self._get_data()
        data.setdefault("conversation_log", [])
        
        data["conversation_log"].append({
            "timestamp": datetime.now().isoformat(),
            "user": user_msg,
            "bot": bot_msg
        })
        
        if len(data["conversation_log"]) > 100:
            data["conversation_log"] = data["conversation_log"][-100:]
        
        self._save_data(data)

# Global instance pointer
cloud_mem = None