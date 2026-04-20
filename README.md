# Renesas EK-RA6M5 Ethernet OTA Project

本專案實作基於瑞薩官方標準 (R11AN0497) 的乙太網路 OTA 韌體更新系列工程。

## 🏗️ 系統架構
*   **Bootloader** (`0x0000 0000`): 自定義診斷引導程序。
*   **Slot 0 (Active)**: 起於 `0x0000 8000`。
    *   **Header**: `0x8000 - 0x81FF` (512 Bytes)。
    *   **Application**: `0x0000 8200` (Vector Table)。
*   **Slot 1 (Secondary)**: `0x0010 0000` (用於接收新韌體)。

---

## 🛠️ 開發日誌與關鍵問題解決

### 1. RFP 燒錄報錯：位址衝突 (Error E3000101)
*   **現象**：同時燒錄 Bootloader 與 App 的 SREC 時，報錯資料重疊。
*   **原因**：瑞薩 FSP 工具鏈預設會將一份向量表強行放在 `0x0000`，即便 Linker Script 已搬移 `FLASH_START` 至 `0x8200`。
*   **解決方案**：
    1.  修改 `script/fsp.lld`，在最後強行覆蓋 `FLASH_START = 0x8200;`。
    2.  開發 `finalize_ota.py` 腳本，使用 `objcopy` 去除 ELF 中 0 位址的冗餘資料，並利用 `imgtool` 重新包裝。

### 2. RTOS 啟動崩潰 (無燈號閃爍)
*   **現象**：Bootloader 跳轉成功，但 App 進入 `tx_kernel_enter` 後掛掉。
*   **原因**：Cortex-M33 在啟動 RTOS 前必須重定向向量表。原先在 Thread 任務中修改 `SCB->VTOR` 太晚了，Kernel 初期就會用到中斷。
*   **解決方案**：修改 `startup.c` 中的 `Reset_Handler`，在 `SystemInit()` 之前就強制設定 `SCB->VTOR = 0x8200;`。

### 3. MCUboot 簽名對齊
*   **細節**：必須使用 512B (0x200) 的 Header 才能保證向量表對準 `0x8200` 物理位址。
*   **腳本指令**：`imgtool sign --header-size 0x200 --pad-header ...`

---

## 🚀 專案工具
*   **`finalize_ota.py`**: 自動化簽名與映像檔處理工具。
*   **功能**：一鍵產出用於 `0x8000` 燒錄的 `signed.bin`。

---
## 📅 目前進度
- [x] 第一階段：穩定跳轉與地基工程 (完成)
- [ ] 第二階段：乙太網路連線與 NetX Duo 初始化
- [ ] 第三階段：Slot 1 Flash 寫入與 OTA 下載邏輯
