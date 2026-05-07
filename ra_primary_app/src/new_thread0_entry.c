#include "new_thread0.h"
#include "FreeRTOS_IP.h"
#include "FreeRTOS_Sockets.h"
#include <string.h>

/* --- OTA 設定 --- */
#define OTA_FLASH_START_ADDR  (0x080000)  /* OTA 韌體存放起點 (512KB 處) */
#define FLASH_BLOCK_SIZE      (32768)     /* RA6M5 大區塊為 32KB */

/* --- LED 控制 (遵循測試成功的 P006=藍燈, P007=綠燈 邏輯) --- */
#define LED_BLUE_ON    R_IOPORT_PinWrite(&g_ioport_ctrl, BSP_IO_PORT_00_PIN_06, BSP_IO_LEVEL_HIGH)
#define LED_BLUE_OFF   R_IOPORT_PinWrite(&g_ioport_ctrl, BSP_IO_PORT_00_PIN_06, BSP_IO_LEVEL_LOW)
#define LED_GREEN_ON   R_IOPORT_PinWrite(&g_ioport_ctrl, BSP_IO_PORT_00_PIN_07, BSP_IO_LEVEL_HIGH)
#define LED_GREEN_OFF  R_IOPORT_PinWrite(&g_ioport_ctrl, BSP_IO_PORT_00_PIN_07, BSP_IO_LEVEL_LOW)

/* --- 診斷與緩存 --- */
static uint8_t g_ota_buffer[4096]; /* 接收快取 */

/* --- 必要的鉤子函數 (Hooks) --- */
void vApplicationPingReplyHook( ePingReplyStatus_t eStatus, uint16_t usIdentifier )
{
    ( void ) eStatus;
    ( void ) usIdentifier;
}

void flash_0_cb(flash_callback_args_t * p_args)
{
    (void) p_args;
}

/* New Thread entry function */
void new_thread0_entry(void *pvParameters)
{
    FSP_PARAMETER_NOT_USED(pvParameters);
    fsp_err_t err;
    Socket_t xListeningSocket, xConnectedSocket;
    struct freertos_sockaddr xBindAddress;
    socklen_t xSize = sizeof( xBindAddress );
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
    err = R_FLASH_HP_Open(&g_flash0_ctrl, &g_flash0_cfg);
    if (FSP_SUCCESS != err) while(1);

    /* 4. 初始化網路並等待 IP */
    static uint8_t ucMACAddress[6] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
    static uint8_t ucIPAddress[4] = {192, 168, 1, 100};
    static uint8_t ucNetMask[4] = {255, 255, 255, 0};
    static uint8_t ucGatewayAddress[4] = {192, 168, 1, 1};
    static uint8_t ucDNSServerAddress[4] = {8, 8, 8, 8};
    FreeRTOS_IPInit(ucIPAddress, ucNetMask, ucGatewayAddress, ucDNSServerAddress, ucMACAddress);

    while (FreeRTOS_IsNetworkUp() == pdFALSE) {
        vTaskDelay(500);
    }
    LED_GREEN_ON; /* 拿到 IP，綠燈亮起 */

    /* 5. 建立 TCP Server 監聽 8080 */
    xListeningSocket = FreeRTOS_socket( FREERTOS_AF_INET, FREERTOS_SOCK_STREAM, FREERTOS_IPPROTO_TCP );
    xBindAddress.sin_port = FreeRTOS_htons( 8080 );
    FreeRTOS_bind( xListeningSocket, &xBindAddress, sizeof( xBindAddress ) );
    FreeRTOS_listen( xListeningSocket, 5 );

    while (1)
    {
        /* 等待連線 */
        xConnectedSocket = FreeRTOS_accept( xListeningSocket, &xBindAddress, &xSize );
        if (xConnectedSocket != FREERTOS_INVALID_SOCKET)
        {
            FreeRTOS_setsockopt( xConnectedSocket, 0, FREERTOS_SO_RCVTIMEO, &xReceiveTimeOut, sizeof( xReceiveTimeOut ) );
            u32WriteAddr = OTA_FLASH_START_ADDR;
            
            while (1)
            {
                /* 接收資料 */
                lBytesReceived = FreeRTOS_recv( xConnectedSocket, g_ota_buffer, sizeof( g_ota_buffer ), 0 );
                
                if (lBytesReceived > 0)
                {
                    LED_BLUE_ON; /* 寫入時藍燈亮 */
                    
                    /* 每到區塊邊界就自動擦除 32KB */
                    if (u32WriteAddr % FLASH_BLOCK_SIZE == 0)
                    {
                        __disable_irq();
                        R_FLASH_HP_Erase(&g_flash0_ctrl, u32WriteAddr, 1);
                        __enable_irq();
                    }

                    /* 寫入 Flash (長度對齊 128) */
                    uint32_t u32WriteLen = (uint32_t)((lBytesReceived + 127) & ~127);
                    __disable_irq();
                    err = R_FLASH_HP_Write(&g_flash0_ctrl, (uint32_t)g_ota_buffer, u32WriteAddr, u32WriteLen);
                    __enable_irq();

                    if (FSP_SUCCESS == err) {
                        u32WriteAddr += u32WriteLen;
                    }
                    
                    LED_BLUE_OFF;
                }
                else if (lBytesReceived < 0)
                {
                    FreeRTOS_closesocket( xConnectedSocket );
                    break;
                }
            }
        }
    }
}
