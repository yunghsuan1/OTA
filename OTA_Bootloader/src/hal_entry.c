#include "hal_data.h"

#define LED1_BLUE   BSP_IO_PORT_00_PIN_06
#define LED2_GREEN  BSP_IO_PORT_00_PIN_07
#define LED3_RED    BSP_IO_PORT_00_PIN_08
#define LED_ON      BSP_IO_LEVEL_HIGH
#define LED_OFF     BSP_IO_LEVEL_LOW

void blink_red(int count) {
    for (int i=0; i<count; i++) {
        R_IOPORT_PinWrite(&g_ioport_ctrl, LED3_RED, LED_ON);
        R_BSP_SoftwareDelay(200, BSP_DELAY_UNITS_MILLISECONDS);
        R_IOPORT_PinWrite(&g_ioport_ctrl, LED3_RED, LED_OFF);
        R_BSP_SoftwareDelay(200, BSP_DELAY_UNITS_MILLISECONDS);
    }
    R_BSP_SoftwareDelay(1, BSP_DELAY_UNITS_SECONDS);
}

void hal_entry(void)
{
    R_IOPORT_Open(&g_ioport_ctrl, &g_bsp_pin_cfg);

    /* 藍燈亮：Bootloader 啟動 */
    R_IOPORT_PinWrite(&g_ioport_ctrl, LED1_BLUE, LED_ON);
    R_BSP_SoftwareDelay(500, BSP_DELAY_UNITS_MILLISECONDS);

    /* 
     * 官方標準 (R11AN0497): 
     * Slot Start = 0x8000
     * Header Size = 0x200 (512 Bytes)
     * Vector Table = 0x8200
     */
    uint32_t vtable = 0x00008200;
    uint32_t msp    = *(volatile uint32_t *)(vtable);
    uint32_t reset_handler = *(volatile uint32_t *)(vtable + 4);

    /* --- 診斷邏輯 --- */
    if (msp == 0xFFFFFFFF) {
        while(1) blink_red(1); // 閃 1 下：0x8200 沒東西
    }
    
    if ((msp < 0x20000000) || (msp > 0x20080000)) {
        while(1) blink_red(2); // 閃 2 下：MSP 範圍錯誤 (對齊或錯位)
    }

    /* 準備跳轉：藍/綠同亮 */
    R_IOPORT_PinWrite(&g_ioport_ctrl, LED2_GREEN, LED_ON);
    R_BSP_SoftwareDelay(500, BSP_DELAY_UNITS_MILLISECONDS);

    /* 執行跳轉 */
    __disable_irq();

    /* 1. Invalidate ICACHE & Wait for completion (Renesas Official Sequence) */
    (*(volatile uint32_t *)0x40011000) = 0x01; 
    while((*(volatile uint32_t *)0x40011000) & 0x01);

    /* 2. Set VTOR */
    SCB->VTOR = vtable;

    /* 3. Set MSP & Jump */
    __set_MSP(msp);

    /* 重點：跳轉前先熄滅所有燈，用來判斷是否成功進入 App */
    R_IOPORT_PinWrite(&g_ioport_ctrl, LED1_BLUE, LED_OFF);
    R_IOPORT_PinWrite(&g_ioport_ctrl, LED2_GREEN, LED_OFF);

    ((void (*)(void))reset_handler)();

    while (1);
}
