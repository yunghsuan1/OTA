import os
import subprocess
import sys

# --- 自動偵測路徑 ---
BASE_DIR = os.path.dirname(os.path.abspath(__file__))

# --- 設定工具與檔案絕對路徑 ---
OBJCOPY    = "arm-none-eabi-objcopy"
IMGTOOL    = r"C:\Users\USE\Desktop\SEAN_CODE\OTA\OTA_Bootloader\ra\mcu-tools\MCUboot\scripts\imgtool.py" 
KEY_FILE   = r"C:\Users\USE\Desktop\SEAN_CODE\OTA\OTA_Bootloader\ra\mcu-tools\MCUboot\root-ec-p256.pem"

INPUT_ELF  = os.path.join(BASE_DIR, "Debug", "OTA_RTOS_App.elf")
RAW_BIN    = os.path.join(BASE_DIR, "Debug", "OTA_RTOS_App_raw.bin")
SIGNED_BIN = os.path.join(BASE_DIR, "Debug", "OTA_RTOS_App_signed.bin")

def run_cmd(cmd):
    print(f"執行中: {cmd}")
    result = subprocess.run(cmd, shell=True, capture_output=True, text=True)
    if result.returncode != 0:
        print(f"\n❌ [錯誤]: {result.stderr}")
        return False
    return True

def main():
    if not os.path.exists(INPUT_ELF):
        print(f"❌ 錯誤：找不到 ELF 檔案於 {INPUT_ELF}")
        return

    print("\n--- 步驟 1: 提取 App 核心實體資料 ---")
    # 修改點：使用瑞薩 FSP 專用的段落名稱 ($$)
    # 如果 Linker 設定正確，這會從 0x8200 開始抓取
    cmd_extract = (f'"{OBJCOPY}" -j "__flash_vectors$$" -j "__flash_readonly$$" '
                   f'-j "__flash_init_array$$" -O binary "{INPUT_ELF}" "{RAW_BIN}"')
    
    if not run_cmd(cmd_extract):
        sys.exit(1)

    # 檢查檔案大小
    if os.path.getsize(RAW_BIN) == 0:
        print("❌ 錯誤：產出的 BIN 檔案長度為 0！正在嘗試替代抓取方案...")
        # 替代方案：不指定段落，直接轉 binary
        run_cmd(f'"{OBJCOPY}" -O binary "{INPUT_ELF}" "{RAW_BIN}"')

    print("\n--- 步驟 2: 進行 MCUboot 簽名 (512B Header) ---")
    # --pad-header 讓 imgtool 直接在檔案前補上 Header 位址
    cmd_sign = (f'python "{IMGTOOL}" sign --key "{KEY_FILE}" '
                f'--header-size 0x200 --pad-header --align 128 --version 1.0.1 '
                f'--slot-size 0xF8000 "{RAW_BIN}" "{SIGNED_BIN}"')
    
    if run_cmd(cmd_sign):
        print("\n" + "="*50)
        print("✅ 簽名成功產出！")
        print(f"📍 原始 BIN 大小: {os.path.getsize(RAW_BIN)} bytes")
        print(f"📍 簽名檔路徑: {SIGNED_BIN}")
        print(f"📍 請將此檔案燒錄至 Flash 位址: 0x00008000")
        print("="*50)

if __name__ == "__main__":
    main()
