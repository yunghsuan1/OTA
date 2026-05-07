# Renesas EK-RA6M5 Ethernet OTA 開發手冊

本專案實作基於 Renesas EK-RA6M5 開發板與乙太網路架構的 OTA (Over-the-Air) 韌體更新功能。目前已完成 MCUboot 與 FreeRTOS + TCP 網路堆疊的整合。

---

## 💡 目前狀態：Debug-Flat 模式 (開發中)
為了方便開發 App 邏輯（如乙太網路、感測器等），目前的 App 設定為 **直接從 0x0 啟動**，跳過了 Bootloader。

- **App 位址**: `0x00000000`
- **中斷向量表**: `0x00000000`
- **SmartBundle**: 已移除 (不連動 Bootloader 設定)

---

## 🚀 如何復原為 OTA 模式 (正式測試)
當 App 功能開發完成，需要透過 Bootloader 進行簽署與驗證時，請執行以下步驟：

### 1. 恢復記憶體分區 (Solution Memories)
在 `[RA6M5_TCP_OTA] Solution` 的 **Memories** 頁籤中：
- `FLASH_CM33_B` (Bootloader): Start `0x0`, Size `0x8000` (32KB)
- `__BL_0_P_H` (Header Area): Start `0x8000`, Size `0x100` (256B)
- `FLASH_CM33_S` (App Slot): Start **`0x8100`**, Size `0xF7F00` (約 1MB)

### 2. 恢復 SmartBundle 連動 (Build Variables)
1. 對 `ra_primary_app` 點右鍵 -> **Properties**。
2. 進入 **C/C++ Build** -> **Build Variables**。
3. 點擊 **Add...** 新增一個變數：
   - **Name**: `SmartBundle`
   - **Type**: `File`
   - **Value**: `${workspace_loc:/ra_mcuboot}/${ConfigName}/ra_mcuboot.sbd`

### 3. 更新專案並編譯
1. 在 `ra_primary_app` 按 **Generate Project Content**。
2. 執行 **Clean Project** 與 **Build Project**。

### 4. 燒錄驗證 (RFP)
- `ra_mcuboot.srec` -> 位址 `0x0`
- `ra_primary_app.bin.signed` -> 位址 **`0x8000`**

---

## 📊 OTA 架構與流程圖

### 5.1 引導與更新邏輯
```mermaid
graph TD
    %% 啟動與判定
    Power[上電啟動 / 系統重置] --> S1{1. 偵測 S1 行為?}
    
    %% S1 救援路徑
    S1 -- "長按模式 (救援模式)" --> Rescue[執行強制 Rollback]
    Rescue --> Swap_Op
    
    %% 正常路徑
    S1 -- "短按模式 (正常啟動)" --> HeaderObj{2. 檢查 Slot 1 Header}
    HeaderObj -- "新韌體待更新" --> Swap_Op[3. 執行 A/B 槽位對調]
    HeaderObj -- "無更新 / 驗證失敗" --> Boot0{4. 驗證 Slot 0 簽章}
    
    Swap_Op --> Boot0
    
    Boot0 -- "驗證通過" --> Jump[5. 更新 VTOR 並跳轉至 0x8100]
    Boot0 -- "驗證失敗" --> Halt[系統停止 / 紅燈告警]

    subgraph "Application 運行環境 (Slot 0)"
    Jump --> Run[應用程式正常執行]
    Run --> Incoming[接收網路 OTA 封包]
    Incoming --> FlashWrite[寫入 Secondary Slot - 1MB 處]
    FlashWrite --> SetPending[設定重置後更新標記]
    SetPending --> Reboot[執行系統重置]
    Reboot --> Power
    end
```

---

## 🛠️ 專案開發進度 (To-Do List)

### 6.1 應用程式端 (App)
- [x] **LED 偵測**：完成 P006~P008 腳位配置與閃燈測試。
- [x] **網路服務**：實作 TCP Server (Port 8080) 用於接收 OTA 資料。
- [x] **Flash 寫入**：實作分塊寫入邏輯，成功避開 Dual Mode 斷層。
- [ ] **韌體切換**：實作 Bank Swap 或 MCUboot 觸發邏輯。
- [ ] **狀態更新**：在重置前標記「更新待處理 (Pending)」旗標。

---

## 📡 OTA 使用說明 (TCP Bridge)

目前已實現透過乙太網路遠端寫入 Flash 的功能。

### 1. 板子端準備
*   燒錄最新程式後，確認 **綠燈 (P007)** 恆亮，代表網路已就緒。
*   板子 IP 預設為 `192.168.1.100`，監聽端口 `8080`。

### 2. 電腦端傳送
使用專案根目錄下的 `ota_client.py` 進行傳送：
```bash
# 發送測試資料 (64KB)
python ota_client.py 192.168.1.100

# 發送特定韌體檔案
python ota_client.py 192.168.1.100 your_firmware.bin
```

### 3. 驗證方式
*   傳送時，板子 **藍燈 (P006)** 會閃爍。
*   傳送完成後，在 e2 studio 中檢查位址 `0x080000` 即可看到資料。

---

## 📝 修復日誌 (Troubleshooting)
- **Problem**: 乙太網路 Ping 不通。
- **Solution**: 開啟 IOPORT (R_IOPORT_Open) 並將 MSTPCRB 的 bit 15 設為 0 以啟動 ETHERC 模組。
- **Problem**: Linker Overflow。
- **Solution**: 在 Solution Memories 中手動劃分 Secure 區域，確保 BL 與 App 位址不衝突。
- **Problem**: **[TrustZone] 網路連線 10 秒後斷線，且 ARP 通但 Ping 不通。**
- **Solution**: 
    1. **記憶體牆修復**：將 Ethernet 描述符與封包池強制搬移至 `.ns_buffer` 段（Non-secure RAM），防止 EDMAC 觸發 ADE 錯誤。
    2. **搬運工權限解鎖**：在啟動時手動修改 `PSARB` (bit 15) 與 `PSARC` (bit 15) 以解鎖 ETHERC 與 EDMAC 的存取權限。
    3. **MAC 位址同步**：確保 FreeRTOS+TCP 堆疊 MAC 與硬體暫存器 MAC 完全一致（同步為 `00:11:22:33:44:55`），防止硬體過濾器誤殺單播封包。
- **Problem**: **[Flash] 調用 R_FLASH_HP_Erase 報錯 FSP_ERR_ERASE_FAILED (0x10004)。**
- **Solution**: 
    1. **硬體權限解鎖**：RA6M5 硬體預設鎖定 Flash 控制器，需手動解鎖 `PSARA` (bit 10) 暫存器權限。
    2. **位址斷層避讓**：在 Dual Mode 模式下，1MB 邊界處 (`0x100000`) 為位址斷層，改用 `0x080000` (512KB) 或正確對齊 Bank 位址。
    3. **中斷保護**：在執行 Code Flash P/E 期間必須關閉中斷 (`__disable_irq`) 防止 CPU 存取異常。
    4. **診斷驗證**：透過 `FAWMON` 監控門鎖狀態，確認 `FAWEN` 位元為 1 (Disabled) 以確保門鎖未關閉。
