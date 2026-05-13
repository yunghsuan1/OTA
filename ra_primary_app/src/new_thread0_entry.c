#include "FreeRTOS_IP.h"
#include "FreeRTOS_Sockets.h"
#include "new_thread0.h"
#include <string.h>
#include <stdlib.h>

/* --- 版本開關 --- */
#define CURRENT_APP_VERSION 1 /* 1: 舊版 (原功能), 2: 新版 (跑馬燈功能) */

/* --- OTA 設定 --- */
#define OTA_FLASH_START_ADDR                                                   \
  (0x100000)                     /* OTA 韌體存放起點 (倉庫位址)              \
                                  */
#define FLASH_BLOCK_SIZE (32768) /* RA6M5 大區塊為 32KB */

/* --- LED 控制 (遵循測試成功的 P006=藍燈, P007=綠燈 邏輯) --- */
#define LED_BLUE_ON                                                            \
  R_IOPORT_PinWrite(&g_ioport_ctrl, BSP_IO_PORT_00_PIN_06, BSP_IO_LEVEL_HIGH)
#define LED_BLUE_OFF                                                           \
  R_IOPORT_PinWrite(&g_ioport_ctrl, BSP_IO_PORT_00_PIN_06, BSP_IO_LEVEL_LOW)
#define LED_GREEN_ON                                                           \
  R_IOPORT_PinWrite(&g_ioport_ctrl, BSP_IO_PORT_00_PIN_07, BSP_IO_LEVEL_HIGH)
#define LED_GREEN_OFF                                                          \
  R_IOPORT_PinWrite(&g_ioport_ctrl, BSP_IO_PORT_00_PIN_07, BSP_IO_LEVEL_LOW)

/* --- Flash 診斷變數 --- */
volatile fsp_err_t g_flash_open_err = 0;
volatile fsp_err_t g_flash_erase_err = 0;
volatile fsp_err_t g_flash_write_err = 0;
volatile uint32_t g_fstatr = 0;
volatile uint32_t g_fawmon = 0;
volatile uint32_t g_fbankmon = 0;

/* --- 診斷與緩存 --- */
static uint32_t g_ota_buffer[1024]; /* 接收快取，使用 uint32_t 確保 4-byte 對齊 */
static uint32_t g_expected_file_size = 0;
static uint32_t g_received_bytes = 0;
static uint32_t g_buffer_index = 0;

/* --- CRC32 計算函數 --- */
uint32_t calculate_crc32(const uint8_t *data, size_t len) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 1) crc = (crc >> 1) ^ 0xEDB88320;
            else crc >>= 1;
        }
    }
    return ~crc;
}

/* --- 必要的鉤子函數 (Hooks) --- */
void vApplicationPingReplyHook(ePingReplyStatus_t eStatus,
                               uint16_t usIdentifier) {
  (void)eStatus;
  (void)usIdentifier;
}

void flash_0_cb(flash_callback_args_t *p_args) { (void)p_args; }

#if (CURRENT_APP_VERSION == 2)
/* App 2 專用的跑馬燈 Task */
void marquee_task(void *pvParameters) {
  (void)pvParameters;
  while (1) {
    /* LED 1 (藍燈 P006) */
    R_IOPORT_PinWrite(&g_ioport_ctrl, BSP_IO_PORT_00_PIN_06, BSP_IO_LEVEL_HIGH);
    vTaskDelay(pdMS_TO_TICKS(200));
    R_IOPORT_PinWrite(&g_ioport_ctrl, BSP_IO_PORT_00_PIN_06, BSP_IO_LEVEL_LOW);

    /* LED 2 (綠燈 P007) */
    R_IOPORT_PinWrite(&g_ioport_ctrl, BSP_IO_PORT_00_PIN_07, BSP_IO_LEVEL_HIGH);
    vTaskDelay(pdMS_TO_TICKS(200));
    R_IOPORT_PinWrite(&g_ioport_ctrl, BSP_IO_PORT_00_PIN_07, BSP_IO_LEVEL_LOW);

    /* LED 3 (紅燈 P008) */
    R_IOPORT_PinWrite(&g_ioport_ctrl, BSP_IO_PORT_00_PIN_08, BSP_IO_LEVEL_HIGH);
    vTaskDelay(pdMS_TO_TICKS(200));
    R_IOPORT_PinWrite(&g_ioport_ctrl, BSP_IO_PORT_00_PIN_08, BSP_IO_LEVEL_LOW);
  }
}
#endif

/* New Thread entry function */
void new_thread0_entry(void *pvParameters) {
  FSP_PARAMETER_NOT_USED(pvParameters);
  fsp_err_t err;
  Socket_t xListeningSocket, xConnectedSocket;
  struct freertos_sockaddr xBindAddress;
  socklen_t xSize = sizeof(xBindAddress);
  static const TickType_t xReceiveTimeOut = pdMS_TO_TICKS(5000); /* 5 秒超時，用於觸發傳送結束 */
  int32_t lBytesReceived;
  uint32_t u32WriteAddr = OTA_FLASH_START_ADDR;

  /* 1. 解鎖硬體權限 (PSARA, PSARB, PSARC) */
  (*(uint32_t *)0x400C4800) |= (1U << 10); // 解鎖 Flash 控制器
  (*(uint32_t *)0x400C4810) |= (1U << 15); // 解鎖網卡
  (*(uint32_t *)0x400C4820) |= (1U << 15); // 解鎖 DMA

  /* 2. 物理重置 PHY */
  R_IOPORT_PinWrite(&g_ioport_ctrl, BSP_IO_PORT_04_PIN_03, BSP_IO_LEVEL_LOW);
  R_BSP_SoftwareDelay(200, BSP_DELAY_UNITS_MILLISECONDS);
  R_IOPORT_PinWrite(&g_ioport_ctrl, BSP_IO_PORT_04_PIN_03, BSP_IO_LEVEL_HIGH);
  R_BSP_SoftwareDelay(1000, BSP_DELAY_UNITS_MILLISECONDS);

  /* 3. 初始化 Flash */
  g_flash_open_err = R_FLASH_HP_Open(&g_flash0_ctrl, &g_flash0_cfg);
  if (FSP_SUCCESS != g_flash_open_err)
    while (1)
      ;

  /* 4. 初始化網路並等待 IP */
  static uint8_t ucMACAddress[6] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
  static uint8_t ucIPAddress[4] = {192, 168, 1, 100};
  static uint8_t ucNetMask[4] = {255, 255, 255, 0};
  static uint8_t ucGatewayAddress[4] = {192, 168, 1, 1};
  static uint8_t ucDNSServerAddress[4] = {8, 8, 8, 8};
  FreeRTOS_IPInit(ucIPAddress, ucNetMask, ucGatewayAddress, ucDNSServerAddress,
                  ucMACAddress);

  while (FreeRTOS_IsNetworkUp() == pdFALSE) {
    vTaskDelay(500);
  }
  LED_GREEN_ON; /* 拿到 IP，綠燈亮起 */

#if (CURRENT_APP_VERSION == 2)
  /* 啟動跑馬燈任務 */
  xTaskCreate(marquee_task, "Marquee", 512, NULL, 1, NULL);
#endif

  /* 5. 建立 TCP Server 監聽 8080 */
  xListeningSocket = FreeRTOS_socket(FREERTOS_AF_INET, FREERTOS_SOCK_STREAM,
                                     FREERTOS_IPPROTO_TCP);
  xBindAddress.sin_port = FreeRTOS_htons(8080);
  FreeRTOS_bind(xListeningSocket, &xBindAddress, sizeof(xBindAddress));
  FreeRTOS_listen(xListeningSocket, 5);

  while (1) {
    /* 等待連線 */
    xConnectedSocket = FreeRTOS_accept(xListeningSocket, &xBindAddress, &xSize);
    if (xConnectedSocket != FREERTOS_INVALID_SOCKET) {
      /* 還原為阻塞模式，使用 OS 的超時設定 */
      FreeRTOS_setsockopt(xConnectedSocket, 0, FREERTOS_SO_RCVTIMEO,
                          &xReceiveTimeOut, sizeof(xReceiveTimeOut));
      u32WriteAddr = OTA_FLASH_START_ADDR;

      while (1) {
        /* 接收資料 */
        lBytesReceived = FreeRTOS_recv(xConnectedSocket, ((uint8_t *)g_ota_buffer) + g_buffer_index,
                                       sizeof(g_ota_buffer) - g_buffer_index, 0);

        if (lBytesReceived > 0) {
          g_buffer_index += (uint32_t)lBytesReceived;

          /* --- 支援網路指令：設定檔案大小 --- */
          if (g_buffer_index >= 5 && memcmp(g_ota_buffer, "SIZE:", 5) == 0) {
              /* 讀取冒號後面的數字 */
              g_expected_file_size = (uint32_t)strtoul((char *)g_ota_buffer + 5, NULL, 10);
              g_received_bytes = 0;
              g_buffer_index = 0; /* 清空緩衝區，因為指令已處理 */
              
              /* 回傳 ACK 讓 Python 知道可以開始傳檔案 */
              FreeRTOS_send(xConnectedSocket, "SIZE_OK", 7, 0);
              continue;
          }

          /* --- 支援網路指令：清除 Bank 1 --- */
          if (g_buffer_index >= 5 && memcmp(g_ota_buffer, "ERASE", 5) == 0) {
            LED_BLUE_ON;
            __disable_irq();
            /* 擦除 0x100000 開始的 31 個 32KB 區塊 (共 992KB) */
            g_flash_erase_err =
                R_FLASH_HP_Erase(&g_flash0_ctrl, OTA_FLASH_START_ADDR, 31);
            __enable_irq();
            LED_BLUE_OFF;

            g_buffer_index = 0; /* 重置緩衝區 */
            if (FSP_SUCCESS == g_flash_erase_err) {
              FreeRTOS_send(xConnectedSocket, "ERASE_OK", 8, 0);
            } else {
              FreeRTOS_send(xConnectedSocket, "ERASE_ERR", 9, 0);
            }
            continue; /* 繼續等待資料或斷線 */
          }

          /* --- 流式寫入 Flash (維持 128 Byte 對齊且不跨 32KB Block) --- */
          while (g_buffer_index >= 128) {
            /* 限制寫入長度，不要跨越 32KB Block 邊界 */
            uint32_t space_in_block = FLASH_BLOCK_SIZE - (u32WriteAddr % FLASH_BLOCK_SIZE);
            uint32_t write_len = g_buffer_index & ~127; /* 長度對齊 128 */
            
            if (write_len > space_in_block) {
                write_len = space_in_block; /* 縮減長度到剛好填滿目前 block */
            }
            
            /* 如果 write_len 為 0，代表我們需要換 block 或是資料不夠 128 */
            if (write_len == 0) {
                break; /* 等待更多資料 */
            }

            __disable_irq();
            g_flash_write_err =
                R_FLASH_HP_Write(&g_flash0_ctrl, (uint32_t)g_ota_buffer,
                                 u32WriteAddr, write_len);
            __enable_irq();

            if (FSP_SUCCESS == g_flash_write_err) {
              u32WriteAddr += write_len;
              g_received_bytes += write_len;

              /* 移出已處理的資料 */
              memmove(g_ota_buffer, g_ota_buffer + write_len,
                      g_buffer_index - write_len);
              g_buffer_index -= write_len;
              
              /* 每寫入約 100KB 就回傳進度 */
              static uint32_t last_prog = 0;
              if (g_received_bytes - last_prog >= 102400) {
                  char prog_reply[32];
                  int p_len = snprintf(prog_reply, sizeof(prog_reply), "PROG:%lu\n", (unsigned long)g_received_bytes);
                  FreeRTOS_send(xConnectedSocket, prog_reply, p_len, 0);
                  last_prog = g_received_bytes;
              }
            } else {
              /* 寫入失敗！透過網路回傳錯誤代碼 */
              char err_reply[32];
              int err_len = snprintf(err_reply, sizeof(err_reply), "WRITE_ERR:%d\n", (int)g_flash_write_err);
              FreeRTOS_send(xConnectedSocket, err_reply, err_len, 0);
              break; /* 發生錯誤，跳出寫入迴圈 */
            }
            
            /* 在每次寫入 128 Byte 間稍微讓出時間給 Ethernet 中斷 */
            vTaskDelay(pdMS_TO_TICKS(1));
          }
        } else if (lBytesReceived <= 0) {
          /* 連線中斷 (收到了 FIN 或是出錯) 或超時 */
          if (g_expected_file_size > 0 && g_received_bytes > 100000) {
              /* 假定傳送結束 */
              char done_reply[64];
              int d_len = snprintf(done_reply, sizeof(done_reply), "DONE:%lu\n", (unsigned long)g_received_bytes);
              FreeRTOS_send(xConnectedSocket, done_reply, d_len, 0);
              
              uint32_t calculated_crc = calculate_crc32((const uint8_t *)OTA_FLASH_START_ADDR, g_received_bytes);
              
              char reply[64];
              int reply_len = snprintf(reply, sizeof(reply), "CRC:%08X\n", (unsigned int)calculated_crc);
              FreeRTOS_send(xConnectedSocket, reply, reply_len, 0);
              
              /* 觸發驗證與亮燈 */
#if (CURRENT_APP_VERSION == 1)
              uint32_t *p_magic = (uint32_t *)OTA_FLASH_START_ADDR;
              if (*p_magic == 0x96f3b83d) {
                /* 亮三顆燈 5 秒 */
                R_IOPORT_PinWrite(&g_ioport_ctrl, BSP_IO_PORT_00_PIN_06, BSP_IO_LEVEL_HIGH);
                R_IOPORT_PinWrite(&g_ioport_ctrl, BSP_IO_PORT_00_PIN_07, BSP_IO_LEVEL_HIGH);
                R_IOPORT_PinWrite(&g_ioport_ctrl, BSP_IO_PORT_00_PIN_08, BSP_IO_LEVEL_HIGH);
                vTaskDelay(pdMS_TO_TICKS(5000));
                R_IOPORT_PinWrite(&g_ioport_ctrl, BSP_IO_PORT_00_PIN_06, BSP_IO_LEVEL_LOW);
                R_IOPORT_PinWrite(&g_ioport_ctrl, BSP_IO_PORT_00_PIN_07, BSP_IO_LEVEL_LOW);
                R_IOPORT_PinWrite(&g_ioport_ctrl, BSP_IO_PORT_00_PIN_08, BSP_IO_LEVEL_LOW);
              }
#endif
              g_expected_file_size = 0;
          }
          FreeRTOS_closesocket(xConnectedSocket);
          break; /* 結束接收迴圈 */
        }
      }
    }
  }
}
