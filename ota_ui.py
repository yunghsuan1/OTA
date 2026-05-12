import sys
import tkinter as tk
from tkinter import filedialog, messagebox, ttk
import socket
import os
import threading
from datetime import datetime

DEFAULT_IP = "192.168.1.100"
DEFAULT_PORT = 8080

class OTAClientUI:
    def __init__(self, root):
        self.root = root
        self.root.title("RA6M5 OTA Client with Terminal")
        self.root.geometry("550x550")
        self.root.resizable(False, False)
        
        # IP Address
        tk.Label(root, text="Target IP:").pack(pady=5)
        self.ip_entry = tk.Entry(root, width=20)
        self.ip_entry.insert(0, DEFAULT_IP)
        self.ip_entry.pack(pady=5)
        
        # File Selection
        self.file_path = tk.StringVar()
        tk.Label(root, text="Firmware File:").pack(pady=5)
        file_frame = tk.Frame(root)
        file_frame.pack(pady=5)
        tk.Entry(file_frame, textvariable=self.file_path, width=40).pack(side=tk.LEFT, padx=5)
        tk.Button(file_frame, text="Browse", command=self.browse_file).pack(side=tk.LEFT)
        
        # Buttons
        btn_frame = tk.Frame(root)
        btn_frame.pack(pady=10)
        self.erase_btn = tk.Button(btn_frame, text="1. ERASE SLOT 2", command=self.start_erase, bg="#ffcccb")
        self.erase_btn.pack(side=tk.LEFT, padx=10)
        self.send_btn = tk.Button(btn_frame, text="2. SEND FILE", command=self.start_send, bg="#ccffcc")
        self.send_btn.pack(side=tk.LEFT, padx=10)
        
        # Progress
        tk.Label(root, text="Progress:").pack(pady=5)
        self.progress = ttk.Progressbar(root, orient=tk.HORIZONTAL, length=400, mode='determinate')
        self.progress.pack(pady=5)
        
        # Status
        self.status_label = tk.Label(root, text="Ready", fg="blue")
        self.status_label.pack(pady=5)
        
        # Terminal-like Log Area
        tk.Label(root, text="Terminal Log:").pack(pady=5)
        log_frame = tk.Frame(root)
        log_frame.pack(pady=5, padx=10, fill=tk.BOTH, expand=True)
        
        self.log_text = tk.Text(log_frame, width=65, height=10, bg="#1e1e1e", fg="#d4d4d4", font=("Consolas", 10))
        self.log_text.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        
        scrollbar = tk.Scrollbar(log_frame, command=self.log_text.yview)
        scrollbar.pack(side=tk.RIGHT, fill=tk.Y)
        self.log_text.config(yscrollcommand=scrollbar.set)
        
        self.log_text.config(state=tk.DISABLED) # Make read-only
        
        self.log("=== OTA Client Terminal Started ===")
        
    def log(self, text):
        self.log_text.config(state=tk.NORMAL)
        timestamp = datetime.now().strftime("[%H:%M:%S]")
        self.log_text.insert(tk.END, f"{timestamp} {text}\n")
        self.log_text.config(state=tk.DISABLED)
        self.log_text.see(tk.END) # Auto scroll to bottom
        self.root.update_idletasks()
        
    def browse_file(self):
        filename = filedialog.askopenfilename(
            initialdir=r"C:\Users\USE\Desktop\SEAN_CODE\OTA\app2",
            filetypes=[("Signed BIN files", "*.bin.signed"), ("All files", "*.*")]
        )
        if filename:
            self.file_path.set(filename)
            self.log(f"Selected file: {os.path.basename(filename)}")
            
    def set_status(self, text, color="black"):
        self.status_label.config(text=text, fg=color)
        
    def start_erase(self):
        ip = self.ip_entry.get()
        self.set_status("Erasing Slot 2... Please wait.", "orange")
        self.log(f"Connecting to {ip}:8080 for Erase...")
        self.erase_btn.config(state=tk.DISABLED)
        
        threading.Thread(target=self.run_erase, args=(ip,)).start()
        
    def run_erase(self, ip):
        try:
            s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            s.settimeout(10)
            s.connect((ip, DEFAULT_PORT))
            
            self.log("[TX] Sending command: ERASE")
            s.sendall(b"ERASE")
            
            self.log("Waiting for response (Flash erase takes time)...")
            resp = s.recv(1024)
            s.close()
            
            self.log(f"[RX] Received: {resp.decode()}")
            
            if resp == b"ERASE_OK":
                self.set_status("Erase Successful!", "green")
                self.log("Result: Slot 2 Erased Successfully!")
                messagebox.showinfo("Success", "Slot 2 Erased Successfully!")
            else:
                self.set_status("Erase Failed!", "red")
                self.log(f"Result: Erase Failed ({resp.decode()})")
                messagebox.showerror("Error", f"Erase Failed: {resp.decode()}")
        except Exception as e:
            self.set_status("Error occurred", "red")
            self.log(f"Error: {e}")
            messagebox.showerror("Error", str(e))
        finally:
            self.erase_btn.config(state=tk.NORMAL)
            
    def start_send(self):
        ip = self.ip_entry.get()
        file_path = self.file_path.get()
        
        if not file_path:
            messagebox.showwarning("Warning", "Please select a file first!")
            return
            
        self.set_status("Sending file...", "orange")
        self.log(f"Connecting to {ip}:8080 to send file...")
        self.send_btn.config(state=tk.DISABLED)
        self.progress['value'] = 0
        
        threading.Thread(target=self.run_send, args=(ip, file_path)).start()
        
    def run_send(self, ip, file_path):
        try:
            s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            s.settimeout(5)
            s.connect((ip, DEFAULT_PORT))
            
            total_size = os.path.getsize(file_path)
            sent_size = 0
            self.log(f"File size: {total_size} Bytes. Starting transmission...")
            
            with open(file_path, "rb") as f:
                while True:
                    data = f.read(1024)
                    if not data:
                        break
                    s.sendall(data)
                    sent_size += len(data)
                    
                    percent = (sent_size / total_size) * 100
                    self.progress['value'] = percent
                    self.set_status(f"Sending: {sent_size}/{total_size} Bytes ({percent:.1f}%)", "orange")
                    
            s.close()
            self.set_status("File Sent Successfully!", "green")
            self.log("[TX] All data sent successfully!")
            self.log("Notice: Remember to manually Reset the board to apply update.")
            messagebox.showinfo("Success", "File Sent Successfully!\nRemember to manually Reset the board.")
        except Exception as e:
            self.set_status("Error occurred", "red")
            self.log(f"Error: {e}")
            messagebox.showerror("Error", str(e))
        finally:
            self.send_btn.config(state=tk.NORMAL)

if __name__ == "__main__":
    root = tk.Tk()
    app = OTAClientUI(root)
    root.mainloop()
