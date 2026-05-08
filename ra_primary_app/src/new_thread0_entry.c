#include "FreeRTOS_IP.h"
#include "FreeRTOS_Sockets.h"
#include "new_thread0.h"
#include <string.h>

/* --- 版本開關 --- */
#define CURRENT_APP_VERSION 1 /* 1: 舊版 (原功能), 2: 新版 (跑馬燈功能) */

/* --- OTA 設定 --- */
#define OTA_FLASH_START_ADDR (0x200000) /* OTA 韌體存放起點 (Bank 1 內部位址) \
                                         */
#define FLASH_BLOCK_SIZE (32768)        /* RA6M5 大區塊為 32KB */

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
static uint8_t g_ota_buffer[4096]; /* 接收快取 */

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
  static const TickType_t xReceiveTimeOut = portMAX_DELAY;
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
      FreeRTOS_setsockopt(xConnectedSocket, 0, FREERTOS_SO_RCVTIMEO,
                          &xReceiveTimeOut, sizeof(xReceiveTimeOut));
      u32WriteAddr = OTA_FLASH_START_ADDR;

      while (1) {
        /* 接收資料 */
        lBytesReceived = FreeRTOS_recv(xConnectedSocket, g_ota_buffer,
                                       sizeof(g_ota_buffer), 0);

        if (lBytesReceived > 0) {
          LED_BLUE_ON; /* 寫入時藍燈亮 */

          /* 每到區塊邊界就自動擦除 32KB */
          if (u32WriteAddr % FLASH_BLOCK_SIZE == 0) {
            __disable_irq();
            g_flash_erase_err =
                R_FLASH_HP_Erase(&g_flash0_ctrl, u32WriteAddr, 1);
            __enable_irq();
          }

          /* 寫入 Flash (長度對齊 128) */
          uint32_t u32WriteLen = (uint32_t)((lBytesReceived + 127) & ~127);
          __disable_irq();
          g_flash_write_err =
              R_FLASH_HP_Write(&g_flash0_ctrl, (uint32_t)g_ota_buffer,
                               u32WriteAddr, u32WriteLen);
          __enable_irq();

          if (FSP_SUCCESS == g_flash_write_err) {
            u32WriteAddr += u32WriteLen;
          }

          LED_BLUE_OFF;
        } else if (lBytesReceived < 0) {
          /* 連線中斷，代表傳送結束 */
          FreeRTOS_closesocket(xConnectedSocket);

#if (CURRENT_APP_VERSION == 1)
          /* --- 自動驗證：檢查 MCUboot 簽署檔的 Magic Number --- */
          uint32_t *p_magic = (uint32_t *)OTA_FLASH_START_ADDR;
          if (*p_magic == 0x96f3b83d) {
            /* 驗證成功：三燈齊亮 5 秒 */
            R_IOPORT_PinWrite(&g_ioport_ctrl, BSP_IO_PORT_00_PIN_06,
                              BSP_IO_LEVEL_HIGH);
            R_IOPORT_PinWrite(&g_ioport_ctrl, BSP_IO_PORT_00_PIN_07,
                              BSP_IO_LEVEL_HIGH);
            R_IOPORT_PinWrite(&g_ioport_ctrl, BSP_IO_PORT_00_PIN_08,
                              BSP_IO_LEVEL_HIGH);

            vTaskDelay(pdMS_TO_TICKS(5000));

            /* 熄滅 */
            R_IOPORT_PinWrite(&g_ioport_ctrl, BSP_IO_PORT_00_PIN_06,
                              BSP_IO_LEVEL_LOW);
            R_IOPORT_PinWrite(&g_ioport_ctrl, BSP_IO_PORT_00_PIN_07,
                              BSP_IO_LEVEL_LOW);
            R_IOPORT_PinWrite(&g_ioport_ctrl, BSP_IO_PORT_00_PIN_08,
                              BSP_IO_LEVEL_LOW);
          }
#endif
          break;
        }
      }
    }
  }
}
