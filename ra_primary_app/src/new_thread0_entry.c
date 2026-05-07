#include "new_thread0.h"
#include "FreeRTOS_IP.h"
#include "FreeRTOS_Sockets.h"
#include <string.h>

/* --- 引用驅動程式內部的變數 --- */
extern volatile uint8_t g_packet_received; 

/* --- 自定義診斷變數 --- */
volatile uint32_t g_eesr_val = 0;   /* 網卡中斷狀態寄存器 */
volatile uint32_t g_ether_err_cnt = 0;

/* --- Flash 診斷變數 --- */
volatile uint32_t g_fstatr = 0;   
volatile uint32_t g_fawmon = 0;   
volatile uint32_t g_fbankmon = 0; 

/* --- FreeRTOS 要求的鉤子函數 --- */
void vApplicationPingReplyHook( ePingReplyStatus_t eStatus, uint16_t usIdentifier )
{
    ( void ) eStatus;
    ( void ) usIdentifier;
}

/* --- Flash 回呼函式 --- */
void flash_0_cb(flash_callback_args_t * p_args)
{
    (void) p_args;
}

/* New Thread entry function */
void new_thread0_entry(void *pvParameters) {
  FSP_PARAMETER_NOT_USED(pvParameters);

  /* 1. 解鎖 PSCU 硬體權限 (PSARA, PSARB, PSARC) */
  (*(uint32_t *)0x400C4800) |= (1U << 10); // PSARA: FACI (Flash 控制器)
  (*(uint32_t *)0x400C4810) |= (1U << 15); // PSARB: ETHERC0
  (*(uint32_t *)0x400C4820) |= (1U << 15); // PSARC: EDMAC

  /* 2. 物理重置 PHY (保持網路穩定) */
  R_IOPORT_PinWrite(&g_ioport_ctrl, BSP_IO_PORT_04_PIN_03, BSP_IO_LEVEL_LOW);
  R_BSP_SoftwareDelay(200, BSP_DELAY_UNITS_MILLISECONDS);
  R_IOPORT_PinWrite(&g_ioport_ctrl, BSP_IO_PORT_04_PIN_03, BSP_IO_LEVEL_HIGH);
  R_BSP_SoftwareDelay(1000, BSP_DELAY_UNITS_MILLISECONDS);

  /* 3. 初始化網路堆疊 */
  static uint8_t ucMACAddress[6] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
  static uint8_t ucIPAddress[4] = {192, 168, 1, 100};
  static uint8_t ucNetMask[4] = {255, 255, 255, 0};
  static uint8_t ucGatewayAddress[4] = {192, 168, 1, 1};
  static uint8_t ucDNSServerAddress[4] = {8, 8, 8, 8};

  FreeRTOS_IPInit(ucIPAddress, ucNetMask, ucGatewayAddress, ucDNSServerAddress, ucMACAddress);

  /* --- [Flash 寫入驗證：穩定診斷版] --- */
  volatile fsp_err_t err;
  uint8_t test_data[128];
  uint8_t read_data[128];
  memset(test_data, 0xAA, 128);
  memcpy(test_data, "RA6M5 OTA Flash Test", 20);

  /* 1. 開啟 Flash */
  err = R_FLASH_HP_Open(&g_flash0_ctrl, &g_flash0_cfg);
  
  /* 讀取模式監控 */
  g_fbankmon = (*(uint32_t *)0x40102038); 

  if (FSP_SUCCESS == err)
  {
      /* 2. 擦除測試點 (0x80000 = 512KB) */
      __disable_irq(); 
      err = R_FLASH_HP_Erase(&g_flash0_ctrl, 0x080000, 1);
      __enable_irq();

      g_fstatr = R_FACI_HP->FSTATR;
      g_fawmon = R_FACI_HP->FAWMON;
  }

  if (FSP_SUCCESS == err)
  {
      /* 3. 寫入測試資料 */
      __disable_irq();
      err = R_FLASH_HP_Write(&g_flash0_ctrl, (uint32_t)test_data, 0x080000, 128);
      __enable_irq();
  }

  if (FSP_SUCCESS == err)
  {
      /* 4. 驗證資料 */
      memcpy(read_data, (void *)0x080000, 128);
      if (0 == memcmp(test_data, read_data, 128))
      {
          /* 成功：藍燈快速閃爍 10 次 (P006) */
          for(int i=0; i<10; i++) {
              R_IOPORT_PinWrite(&g_ioport_ctrl, BSP_IO_PORT_00_PIN_06, BSP_IO_LEVEL_HIGH);
              R_BSP_SoftwareDelay(50, BSP_DELAY_UNITS_MILLISECONDS);
              R_IOPORT_PinWrite(&g_ioport_ctrl, BSP_IO_PORT_00_PIN_06, BSP_IO_LEVEL_LOW);
              R_BSP_SoftwareDelay(50, BSP_DELAY_UNITS_MILLISECONDS);
          }
      }
  }
  /* ---------------------- */

  TickType_t xLastWakeTime = xTaskGetTickCount();
  while (1) {
    vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(100));

    /* 藍燈 (P006)：Link Status */
    if (FreeRTOS_IsNetworkUp()) {
        R_IOPORT_PinWrite(&g_ioport_ctrl, BSP_IO_PORT_00_PIN_06, BSP_IO_LEVEL_HIGH);
    } else {
        R_IOPORT_PinWrite(&g_ioport_ctrl, BSP_IO_PORT_00_PIN_06, BSP_IO_LEVEL_LOW);
    }

    /* 綠燈 (P007)：封包存取 */
    if (g_packet_received) {
        g_packet_received = 0;
        R_IOPORT_PinWrite(&g_ioport_ctrl, BSP_IO_PORT_00_PIN_07, BSP_IO_LEVEL_HIGH);
        vTaskDelay(pdMS_TO_TICKS(10));
        R_IOPORT_PinWrite(&g_ioport_ctrl, BSP_IO_PORT_00_PIN_07, BSP_IO_LEVEL_LOW);
    }
  }
}
