#include "new_thread0.h"
#include "FreeRTOS_IP.h"
#include "FreeRTOS_Sockets.h"

/* --- 引用驅動程式內部的變數 (避免重複定義) --- */
extern volatile uint8_t g_packet_received; 

/* --- 自定義診斷變數 --- */
volatile uint32_t g_eesr_val = 0;   /* 網卡中斷狀態寄存器 */
volatile uint32_t g_ether_err_cnt = 0;

/* --- FreeRTOS 要求的鉤子函數 --- */
void vApplicationPingReplyHook( ePingReplyStatus_t eStatus, uint16_t usIdentifier )
{
    ( void ) eStatus;
    ( void ) usIdentifier;
}

/* New Thread entry function */
void new_thread0_entry(void *pvParameters) {
  FSP_PARAMETER_NOT_USED(pvParameters);

  /* 1. 解鎖 PSCU 硬體權限 (ETHERC0 Slave + EDMAC Master) */
  (*(uint32_t *)0x400C4810) |= (1U << 15); // PSARB: ETHERC0
  (*(uint32_t *)0x400C4820) |= (1U << 15); // PSARC: EDMAC

  /* 2. 物理重置 PHY (拉長等待時間) */
  R_IOPORT_PinWrite(&g_ioport_ctrl, BSP_IO_PORT_04_PIN_03, BSP_IO_LEVEL_LOW);
  R_BSP_SoftwareDelay(200, BSP_DELAY_UNITS_MILLISECONDS);
  R_IOPORT_PinWrite(&g_ioport_ctrl, BSP_IO_PORT_04_PIN_03, BSP_IO_LEVEL_HIGH);
  R_BSP_SoftwareDelay(1000, BSP_DELAY_UNITS_MILLISECONDS);

  /* 3. 初始化網路堆疊 (MAC 位址必須與 common_data.c 裡的硬體設定一致) */
  static uint8_t ucMACAddress[6] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
  static uint8_t ucIPAddress[4] = {192, 168, 1, 100};
  static uint8_t ucNetMask[4] = {255, 255, 255, 0};
  static uint8_t ucGatewayAddress[4] = {192, 168, 1, 1};
  static uint8_t ucDNSServerAddress[4] = {8, 8, 8, 8};

  FreeRTOS_IPInit(ucIPAddress, ucNetMask, ucGatewayAddress, ucDNSServerAddress, ucMACAddress);

  TickType_t xLastWakeTime = xTaskGetTickCount();
  
  while (1) {
    vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(100));

    /* 讀取網卡硬體狀態 (EESR 寄存器) */
    g_eesr_val = (*(uint32_t *)0x40050028); 

    /* 藍燈：Link Status */
    if (FreeRTOS_IsNetworkUp()) {
        R_IOPORT_PinWrite(&g_ioport_ctrl, BSP_IO_PORT_00_PIN_06, BSP_IO_LEVEL_HIGH);
    } else {
        R_IOPORT_PinWrite(&g_ioport_ctrl, BSP_IO_PORT_00_PIN_06, BSP_IO_LEVEL_LOW);
    }

    /* 綠燈：引用驅動內部的封包旗標 */
    if (g_packet_received) {
        g_packet_received = 0;
        R_IOPORT_PinWrite(&g_ioport_ctrl, BSP_IO_PORT_00_PIN_07, BSP_IO_LEVEL_HIGH);
        vTaskDelay(pdMS_TO_TICKS(10));
        R_IOPORT_PinWrite(&g_ioport_ctrl, BSP_IO_PORT_00_PIN_07, BSP_IO_LEVEL_LOW);
    }

    /* 紅燈：證明程式沒當機 */
    static uint8_t toggle = 0;
    R_IOPORT_PinWrite(&g_ioport_ctrl, BSP_IO_PORT_00_PIN_08, toggle ? BSP_IO_LEVEL_HIGH : BSP_IO_LEVEL_LOW);
    toggle = !toggle;
  }
}
