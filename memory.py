class ConversationMemory:
    def __init__(self, max_pairs=5):
        self.max_pairs = max_pairs
        self.history = []
    
    def add_interaction(self, user_message, bot_response):
        self.history.append((user_message, bot_response))
        if len(self.history) > self.max_pairs:
            self.history.pop(0)
    
    def get_context(self):
        if not self.history:
            return ""
        
        context = "\nPrevious conversation:\n"
        for user, bot in self.history:
            context += f"User: {user}\nBot: {bot}\n"
        return context
    
    def clear(self):
        self.history = []
    
    def set_max_pairs(self, new_max):
        self.max_pairs = new_max
        while len(self.history) > self.max_pairs:
            self.history.pop(0)