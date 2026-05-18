import scapy.all as scapy
import serial
import socket
import tkinter as tk
import time
import threading
import requests
import os
import cv2
import numpy as np
from PIL import Image, ImageTk
from collections import defaultdict
from scapy.all import sniff, TCP, IP

# ==========================================
# CONFIGURATION
# ==========================================
# UPDATED with your new Webhook URL
DISCORD_WEBHOOK_URL = "https://discord.com/api/webhooks/1506071607586590790/fnotJTHhO9mHiV-_-o0HoOkakBfDmlZCJKx9pvBzxBIa_UeeSs8mZnKeX-AWXaTwXFF5"
UNIQUE_PORT_THRESHOLD = 15
SCAN_TIME_WINDOW = 5
PI_IP = '0.0.0.0'
LISTEN_PORT = 5555

try:
    ser = serial.Serial('/dev/ttyACM0', 9600, timeout=0.1)
    time.sleep(2) 
    print("🚀 Connected to Car")
except:
    ser = None
    print("⚠️ Running without Arduino connected.")

# ==========================================
# INFOTAINMENT GUI
# ==========================================
class InfotainmentApp:
    def __init__(self, root):
        self.root = root
        self.root.title("Vehicle OS")
        self.root.attributes('-fullscreen', True)
        
        # Colors
        self.bg_page = "#f5f5f7"
        self.bg_card = "#ffffff"
        self.accent_blue = "#007aff"
        self.danger_red = "#ff3b30"
        self.success_green = "#34c759"
        self.text_dim = "#86868b"

        self.root.configure(bg=self.bg_page)
        self.root.bind("<Escape>", lambda e: self.root.destroy())
        
        self.popup_active = False
        self.is_hacked = False 
        
        # Camera & State variables
        self.show_pedestrian = False
        self.current_frame_image = None
        
        self.setup_ui()
        
        # Start the camera stream thread
        threading.Thread(target=self.camera_stream_loop, daemon=True).start()

    def setup_ui(self):
        for widget in self.root.winfo_children():
            widget.destroy()

        # Top Bar
        top = tk.Frame(self.root, bg=self.bg_page, height=40)
        top.pack(fill='x', side='top', padx=20, pady=5)
        tk.Label(top, text="🚗 VSOC System v2.0", font=("Helvetica", 12, "bold"), bg=self.bg_page).pack(side='left')
        tk.Label(top, text="5G 📶  84% 🔋", font=("Helvetica", 11), bg=self.bg_page, fg=self.text_dim).pack(side='right')

        # Main Content
        self.content = tk.Frame(self.root, bg=self.bg_page)
        self.content.pack(expand=True, fill='both', padx=15)

        # LEFT: Vision & Nav
        left_f = tk.Frame(self.content, bg=self.bg_page)
        left_f.pack(side='left', fill='both', expand=True)

        # Tesla Road (Vision Pane) 
        self.vision = tk.Frame(left_f, bg="#1c1c1e", height=220, highlightthickness=2, highlightbackground="#3a3a3c")
        self.vision.pack(fill='x', padx=10, pady=10)
        self.vision.pack_propagate(False)
        
        # UI Container for Camera Display
        self.camera_label = tk.Label(self.vision, bg="#1c1c1e")
        self.camera_label.pack(fill='both', expand=True)

        # Nav Card
        nav_card = tk.Frame(left_f, bg=self.bg_card, highlightthickness=1, highlightbackground="#d2d2d7")
        nav_card.pack(fill='both', expand=True, padx=10, pady=10)
        tk.Label(nav_card, text="🗺️ Current Route: Security HQ", font=("Helvetica", 14, "bold"), bg=self.bg_card).pack(pady=(15, 5))
        tk.Label(nav_card, text="Select Driving Mode", font=("Helvetica", 11), bg=self.bg_card, fg=self.text_dim).pack()
        
        btn_frame = tk.Frame(nav_card, bg=self.bg_card)
        btn_frame.pack(pady=15)
        
        self.stop_btn = tk.Button(btn_frame, text="STANDBY", bg="#e5e5ea", font=("Arial", 11, "bold"), padx=15, pady=10, borderwidth=0, command=self.manual_stop)
        self.stop_btn.pack(side="left", padx=5)

        self.start_btn = tk.Button(btn_frame, text="SELF DRIVING", bg=self.accent_blue, fg="white", font=("Arial", 11, "bold"), padx=15, pady=10, borderwidth=0, command=self.start_self_driving)
        self.start_btn.pack(side="left", padx=5)

        self.auto_btn = tk.Button(btn_frame, text="AUTOPILOT", bg="#34c759", fg="white", font=("Arial", 11, "bold"), padx=15, pady=10, borderwidth=0, command=self.start_autopilot)
        self.auto_btn.pack(side="left", padx=5)

        # RIGHT: Status & Music
        right_f = tk.Frame(self.content, bg=self.bg_page)
        right_f.pack(side='right', fill='both', expand=True, padx=10, pady=10)
        
        # Status Card
        self.car_card = tk.Frame(right_f, bg=self.bg_card, highlightthickness=1, highlightbackground="#d2d2d7")
        self.car_card.pack(fill='both', expand=True, pady=(0, 10))
        self.car_icon = tk.Label(self.car_card, text="🚗", font=("Helvetica", 60), bg=self.bg_card, fg="#d2d2d7")
        self.car_icon.pack(pady=10)
        self.status_lbl = tk.Label(self.car_card, text="SYSTEM READY", font=("Helvetica", 12, "bold"), bg=self.bg_card, fg=self.text_dim)
        self.status_lbl.pack()

        # Music Card
        music_card = tk.Frame(right_f, bg=self.bg_card, highlightthickness=1, highlightbackground="#d2d2d7")
        music_card.pack(fill='both', expand=True)
        tk.Label(music_card, text="NOW PLAYING", font=("Helvetica", 9, "bold"), bg=self.bg_card, fg=self.accent_blue).pack(pady=(10, 0))
        tk.Label(music_card, text="Cyber Sentinel", font=("Helvetica", 14, "bold"), bg=self.bg_card).pack()
        tk.Label(music_card, text="⏮  ⏸  ⏭", font=("Helvetica", 24), bg=self.bg_card).pack(expand=True)

        # Bottom Dock
        self.dock = tk.Frame(self.root, bg=self.bg_card, height=80, highlightthickness=1, highlightbackground="#d2d2d7")
        self.dock.pack(fill='x', side='bottom', padx=20, pady=(0, 20))
        
        tk.Label(self.dock, text="LO", font=("Helvetica", 18), bg=self.bg_card).pack(side='left', padx=30)
        
        menu_frame = tk.Frame(self.dock, bg=self.bg_card)
        menu_frame.pack(side='left', expand=True)
        
        icons = [("🗺️", "Maps"), ("🎶", "Music"), ("🏠", "Exit"), ("📞", "Phone"), ("❄️", "AC")]
        for icon, name in icons:
            cmd = self.root.destroy if name == "Exit" else None
            tk.Button(menu_frame, text=icon, font=("Helvetica", 24), bg=self.bg_card, borderwidth=0, activebackground=self.bg_page, command=cmd).pack(side='left', padx=15)
        
        tk.Label(self.dock, text="21°", font=("Helvetica", 18), bg=self.bg_card).pack(side='right', padx=30)

    # ==========================================
    # CAMERA LOGIC (WITH ROAD MARKINGS)
    # ==========================================
    def camera_stream_loop(self):
        cap = cv2.VideoCapture(0)
        
        while True:
            ret, frame = cap.read()
            if not ret:
                frame = np.zeros((220, 400, 3), dtype=np.uint8) + 28
            else:
                frame = cv2.resize(frame, (400, 220))

            cv2.line(frame, (120, 220), (180, 20), (255, 255, 255), 2) 
            cv2.line(frame, (280, 220), (220, 20), (255, 255, 255), 2) 

            if self.show_pedestrian:
                cv2.rectangle(frame, (150, 140), (250, 150), (48, 59, 255), -1)
                cv2.putText(frame, "(!)", (185, 120), cv2.FONT_HERSHEY_SIMPLEX, 0.8, (0, 204, 255), 2)

            rgb_frame = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
            img = Image.fromarray(rgb_frame)
            img_tk = ImageTk.PhotoImage(image=img)

            def update_ui(i=img_tk):
                self.camera_label.config(image=i)
                self.current_frame_image = i 

            self.root.after(0, update_ui)
            time.sleep(0.03) 
        cap.release()

    def show_pedestrian_on_road(self, show=True):
        self.show_pedestrian = show

    def show_obstacle_warning(self):
        if self.popup_active or self.is_hacked: return
        self.popup_active = True
        self.popup = tk.Toplevel(self.root)
        self.popup.overrideredirect(True)
        self.popup.attributes("-topmost", True)
        self.popup.configure(bg="#1c1c1e")
        w, h = 400, 220
        sw, sh = self.root.winfo_screenwidth(), self.root.winfo_screenheight()
        x = (sw/2) - (w/2)
        y = (sh/2) - (h/2)
        self.popup.geometry(f'{w}x{h}+{int(x)}+{int(y)}')
        tk.Label(self.popup, text="🚧", font=("Arial", 40), bg="#1c1c1e").pack(pady=10)
        tk.Label(self.popup, text="CRITICAL PROXIMITY", font=("Arial", 16, "bold"), bg="#1c1c1e", fg="white").pack()
        tk.Label(self.popup, text="HOLD STEERING STEADY", font=("Arial", 12), bg="#1c1c1e", fg=self.danger_red).pack(pady=10)
        tk.Button(self.popup, text="DISMISS", bg=self.accent_blue, fg="white", font=("Arial", 10, "bold"), padx=20, command=self.close_popup).pack(pady=10)
        self.root.after(3000, self.close_popup)

    def close_popup(self):
        if hasattr(self, 'popup'): 
            try: self.popup.destroy()
            except: pass
        self.popup_active = False

    def start_self_driving(self):
        if ser:
            ser.write(b'G')
            ser.flush()
            self.car_icon.config(fg=self.success_green)
            self.status_lbl.config(text="SELF DRIVING LIVE", fg=self.success_green)

    def start_autopilot(self):
        if ser:
            ser.write(b'A')
            ser.flush()
            self.car_icon.config(fg="#ff9500") 
            self.status_lbl.config(text="AUTOPILOT (LINE) LIVE", fg="#ff9500")

    def manual_stop(self):
        if ser:
            ser.write(b'S')
            ser.flush()
            self.car_icon.config(fg="#d2d2d7")
            self.status_lbl.config(text="SYSTEM READY", fg=self.text_dim)

    def trigger_hacked_ui(self, ip):
        if self.is_hacked: return
        self.is_hacked = True
        if ser:
            ser.write(b'S')
            ser.flush()
        self.root.after(0, lambda: self._hacked_logic(ip))

    def _hacked_logic(self, ip):
        for w in self.root.winfo_children(): w.pack_forget()
        self.root.configure(bg=self.danger_red)
        f = tk.Frame(self.root, bg=self.danger_red)
        f.pack(expand=True)
        tk.Label(f, text="⚠️ SYSTEM COMPROMISED", font=("Arial", 30, "bold"), bg=self.danger_red, fg="white").pack()
        tk.Label(f, text=f"Attacker: {ip}", font=("Arial", 18), bg=self.danger_red, fg="white").pack(pady=10)
        tk.Button(f, text="Acknowledge & exit", bg="black", fg="white", font=("Arial", 14, "bold"), padx=30, pady=15, borderwidth=0, command=self.root.destroy).pack(pady=20)
        self.flash(f)

    def flash(self, f):
        try:
            c = f.cget('bg')
            n = "black" if c == self.danger_red else self.danger_red
            self.root.configure(bg=n); f.configure(bg=n)
            self.root.after(400, lambda: self.flash(f))
        except: pass

# ==========================================
# THREAD FUNCTIONS
# ==========================================

def serial_listener(app):
    while True:
        try:
            if ser and ser.in_waiting > 0:
                line = ser.readline().decode('utf-8', errors='ignore').strip()
                if line.isdigit():
                    distance = int(line)
                    if 0 < distance <= 40:
                        app.root.after(0, lambda: app.show_pedestrian_on_road(True))
                        if distance <= 20 and app.status_lbl.cget("text") != "SENSOR BYPASS ACTIVE":
                            app.root.after(0, app.show_obstacle_warning)
                    else:
                        app.root.after(0, lambda: app.show_pedestrian_on_road(False))
                elif "W" in line:
                    app.root.after(0, lambda: app.show_pedestrian_on_road(True))
                    app.root.after(0, app.show_obstacle_warning)
        except: pass
        time.sleep(0.05)

def network_attack_listener(app):
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        s.bind((PI_IP, LISTEN_PORT))
        s.listen()
        while True:
            conn, addr = s.accept()
            with conn:
                try:
                    data = conn.recv(1024).decode().strip()
                    
                    if data == "ATTACK_BYPASS":
                        if ser:
                            ser.write(b'B')
                            ser.flush()
                        app.root.after(0, lambda: app.status_lbl.config(text="SENSOR BYPASS ACTIVE", fg="#ff9500"))
                        app.root.after(0, lambda: app.car_icon.config(fg="#ff9500"))
                        conn.sendall(b"BYPASS_CONFIRMED")
                        
                    elif data == "ULTRA_JAM":
                        if ser:
                            ser.write(b'S')
                            ser.flush()
                        app.root.after(0, lambda: app.status_lbl.config(text="ULTRASONIC JAMMED", fg="#00d4ff"))
                        app.root.after(0, lambda: app.car_icon.config(fg="#00d4ff"))
                        conn.sendall(b"JAMMING_ACTIVE")

                    elif data == "BRAKE_LOCK":
                        if ser:
                            ser.write(b'S') 
                            ser.flush()
                        app.root.after(0, lambda: app.status_lbl.config(text="BRAKES LOCKED", fg="#ff3b30"))
                        app.root.after(0, lambda: app.car_icon.config(fg="#ff3b30"))
                        conn.sendall(b"BRAKES_ENGAGED")

                    elif data == "SYS_KILL":
                        if ser:
                            ser.write(b'S')
                            ser.flush()
                        app.root.after(0, lambda: app.status_lbl.config(text="ENGINE CORE TERMINATED", fg="#ff3b30"))
                        app.root.after(0, lambda: app.car_icon.config(fg="#ff3b30"))
                        conn.sendall(b"ENGINE_OFFLINE")

                    elif data in ["ARP_SPOOF", "DOS_SYN", "CAN_FUZZ"]:
                        app.trigger_hacked_ui(addr[0]) 
                        
                        def send_discord_alert():
                            payload = {
                                "embeds": [{
                                    "title": "🚨 VSOC CRITICAL ALERT",
                                    "description": f"**Exploit Payload Detected:** `{data}`\n**Source IP:** `{addr[0]}`",
                                    "color": 16711680 
                                }]
                            }
                            try:
                                # Capturing response to help debug Discord side
                                response = requests.post(DISCORD_WEBHOOK_URL, json=payload, timeout=5)
                                print(f"[*] Discord Webhook Status: {response.status_code}")
                            except Exception as e:
                                print(f"Discord Webhook Failed: {e}")
                                
                        threading.Thread(target=send_discord_alert, daemon=True).start()
                        conn.sendall(b"NETWORK_EXPLOIT_DEPLOYED")

                    elif data == "PURGE_LOGS":
                        if app.is_hacked:
                            app.is_hacked = False
                            app.root.after(0, app.setup_ui) 
                        else:
                            app.root.after(0, lambda: app.status_lbl.config(text="SYSTEM READY", fg=app.text_dim))
                            app.root.after(0, lambda: app.car_icon.config(fg="#d2d2d7"))
                        conn.sendall(b"LOGS_PURGED")
                        
                    else:
                        conn.sendall(b"ERR_UNKNOWN_COMMAND")
                        
                except Exception as e:
                    print(f"Network error: {e}")

def sniffing_worker(app):
    port_scan_tracker = defaultdict(list)
    def cb(pkt):
        if app.is_hacked: return 
        if pkt.haslayer(TCP) and pkt[TCP].flags == 0x02:
            src, dst_port, now = pkt[IP].src, pkt[TCP].dport, time.time()
            port_scan_tracker[src].append((now, dst_port))
            port_scan_tracker[src] = [(t, p) for t, p in port_scan_tracker[src] if now - t <= SCAN_TIME_WINDOW]
            if len(set(p for t, p in port_scan_tracker[src])) >= UNIQUE_PORT_THRESHOLD:
                app.trigger_hacked_ui(src)
                threading.Thread(target=lambda: requests.post(DISCORD_WEBHOOK_URL, json={"embeds": [{"title": "🚨 VEHICLE CYBER-ATTACK", "description": f"Source: `{src}`", "color": 16711680}]}, timeout=5), daemon=True).start()
    sniff(filter="tcp", prn=cb, store=0)

if __name__ == "__main__":
    os.system("sudo chmod 666 /dev/ttyACM0 > /dev/null 2>&1")
    root = tk.Tk()
    app = InfotainmentApp(root)
    threading.Thread(target=serial_listener, args=(app,), daemon=True).start()
    threading.Thread(target=sniffing_worker, args=(app,), daemon=True).start()
    threading.Thread(target=network_attack_listener, args=(app,), daemon=True).start()
    root.mainloop()
