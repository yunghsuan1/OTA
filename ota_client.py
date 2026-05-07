import socket
import os
import sys

# --- 設定區域 ---
DEFAULT_IP = "192.168.1.100"  # 請根據你板子的實際 IP 修改
PORT = 8080
CHUNK_SIZE = 4096  # 每次發送的塊大小

def send_ota_data(ip, file_path=None):
    try:
        # 準備資料
        if file_path and os.path.exists(file_path):
            print(f"[*] 讀取韌體檔案: {file_path}")
            with open(file_path, "rb") as f:
                data = f.read()
        else:
            print("[*] 未指定檔案，發送 64KB 測試資料驗證 Flash...")
            data = b"RA6M5_OTA_TEST_" + b"A" * (64 * 1024 - 15)

        # 建立連線
        print(f"[*] 正在連線至 {ip}:{PORT}...")
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            s.settimeout(10)
            s.connect((ip, PORT))
            print("[+] 連線成功！開始傳送...")

            total_size = len(data)
            sent_size = 0
            
            # 分塊傳送
            for i in range(0, total_size, CHUNK_SIZE):
                chunk = data[i:i + CHUNK_SIZE]
                s.sendall(chunk)
                sent_size += len(chunk)
                print(f"\r[>] 進度: {sent_size}/{total_size} Bytes ({sent_size/total_size:.1%})", end="")
                time_sleep_val = 0.01 # 稍微停頓讓板子寫入 Flash
                import time
                time.sleep(time_sleep_val)

            print("\n[+1] 傳送完成！")
            
    except Exception as e:
        print(f"\n[!] 錯誤: {e}")

if __name__ == "__main__":
    target_ip = DEFAULT_IP
    if len(sys.argv) > 1:
        target_ip = sys.argv[1]
    
    file_to_send = None
    if len(sys.argv) > 2:
        file_to_send = sys.argv[2]

    send_ota_data(target_ip, file_to_send)
