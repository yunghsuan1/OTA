#include "hal_data.h"
#include <stdio.h>

// 解決 LLVM picolibc 找不到 stdout 的連結錯誤
FILE *const stdout = NULL;

/* LED Pins for EK-RA6M5 */
#define LED_BLUE  BSP_IO_PORT_00_PIN_06
#define LED_GREEN BSP_IO_PORT_00_PIN_07
#define LED_RED   BSP_IO_PORT_00_PIN_08

/* --- UART9 Debug 支援 --- */
#include <stdarg.h>

/* 簡單的 UART 字串發送函數 */
void uart9_print(const char * str)
{
    /* 呼叫 FSP 的 UART 發送函數 */
    R_SCI_UART_Write(&g_uart9_ctrl, (uint8_t *)str, strlen(str));
    
    /* 等待發送完成 (簡單的輪詢等待) */
    /* 在真實產品中建議用中斷，但這裡為了簡單除錯用輪詢 */
    /* 這裡假設發送不會卡死 */
    R_BSP_SoftwareDelay(10, BSP_DELAY_UNITS_MILLISECONDS);
}

/* 格式化輸出的 printf (簡易版) */
void uart9_printf(const char *format, ...)
{
    char buffer[128];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    uart9_print(buffer);
}
/* ------------------------ */

/* Quick setup for MCUboot. */
void mcuboot_quick_setup()
{
    /* 初始化 UART9 */
    R_SCI_UART_Open(&g_uart9_ctrl, &g_uart9_cfg);
    
    uart9_print("\r\n*** MCUboot Bootloader Started ***\r\n");

    /* Indicate Bootloader started: Blue LED On */
    R_IOPORT_PinWrite(&g_ioport_ctrl, LED_BLUE, BSP_IO_LEVEL_HIGH);
    R_IOPORT_PinWrite(&g_ioport_ctrl, LED_GREEN, BSP_IO_LEVEL_LOW);
    R_IOPORT_PinWrite(&g_ioport_ctrl, LED_RED, BSP_IO_LEVEL_LOW);

#ifdef MCUBOOT_USE_MBED_TLS
    /* Initialize mbedtls. */
    mbedtls_platform_context ctx = {0};
    assert(0 == mbedtls_platform_setup(&ctx));
#elif (defined(MCUBOOT_USE_OCRYPTO) && defined(RM_MCUBOOT_PORT_USE_OCRYPTO_PORT))
    /* Initialize Ocrypto port. */
    assert(FSP_SUCCESS == RM_OCRYPTO_PORT_Init());
#elif (defined(MCUBOOT_USE_TINYCRYPT) && defined(RM_MCUBOOT_PORT_USE_TINYCRYPT_ACCELERATION))
    /* Initialize TinyCrypt port. */
    assert(FSP_SUCCESS == RM_TINCYRYPT_PORT_Init());
#elif (defined(MCUBOOT_USE_USER_DEFINED_CRYPTO_STACK))
    /* Initialize Custom Crypto (Protected Mode) driver. */
    assert(FSP_SUCCESS == R_SCE_Open(&sce_ctrl, &sce_cfg));
#endif

    /* Verify the boot image and get its location. */
    struct boot_rsp rsp;

    /* --- [防磚機制] 偵測按鍵 S1 (P005) --- */
    #define BUTTON_S1 BSP_IO_PORT_00_PIN_05
    bsp_io_level_t button_state = BSP_IO_LEVEL_HIGH;
    
    /* 讀取 S1 按鍵狀態 */
    R_IOPORT_PinRead(&g_ioport_ctrl, BUTTON_S1, &button_state);
    
    /* 如果 S1 被按下 (Low) */
    if (button_state == BSP_IO_LEVEL_LOW)
    {
        uart9_print("[WARN] S1 Pressed! Forced Fallback to App 1...\r\n");

        /* 亮紅燈與藍燈，代表進入「強制救援模式」 */
        R_IOPORT_PinWrite(&g_ioport_ctrl, LED_BLUE, BSP_IO_LEVEL_HIGH);
        R_IOPORT_PinWrite(&g_ioport_ctrl, LED_RED, BSP_IO_LEVEL_HIGH);
        R_IOPORT_PinWrite(&g_ioport_ctrl, LED_GREEN, BSP_IO_LEVEL_LOW);
        
        /* 延時一下讓燈號明顯 */
        R_BSP_SoftwareDelay(1, BSP_DELAY_UNITS_SECONDS);
        
        /* 強制指回 App 1 的起點 (0x8000) */
        rsp.br_hdr = NULL;
        rsp.br_flash_dev_id = 127;
        rsp.br_image_off = 0x8000; 
        
        /* 進入應用程式 */
        RM_MCUBOOT_PORT_BootApp(&rsp);
    }
    /* ----------------------------------- */

    /* Verify the boot image and get its location. */
    
    uart9_print("[INFO] Calling boot_go()...\r\n");

    /* 檢查 Slot 1 (0x100000) 的開頭資料 */
    uint32_t *p_slot1 = (uint32_t *)0x100000;
    uart9_printf("[DEBUG] Slot 1 (0x100000) Magic: 0x%08X\r\n", *p_slot1);

    /* 印出內建的公鑰 */
    #include "bootutil/sign_key.h"
    extern const struct bootutil_key bootutil_keys[];
    
    uart9_printf("[DEBUG] MCUboot Key Len: %d\r\n", *bootutil_keys[0].len);
    uart9_printf("[DEBUG] MCUboot Key: ");
    for (unsigned int i = 0; i < *bootutil_keys[0].len; i++) {
        uart9_printf("%02X ", bootutil_keys[0].key[i]);
    }
    uart9_printf("\r\n");

    int boot_err = boot_go(&rsp);
    if (0 != boot_err)
    {
        uart9_printf("[ERROR] MCUboot Verification Failed! Err Code: %d\r\n", boot_err);

        /* Verification Failed: Red LED On, Blue Off */
        R_IOPORT_PinWrite(&g_ioport_ctrl, LED_BLUE, BSP_IO_LEVEL_LOW);
        R_IOPORT_PinWrite(&g_ioport_ctrl, LED_RED, BSP_IO_LEVEL_HIGH);
        while(1); /* Hang here for debug visibility */
    }

    uart9_printf("[OK] MCUboot Verification Passed!\r\n");
    uart9_printf("[INFO] Image Offset: 0x%08X\r\n", rsp.br_image_off);
    uart9_printf("[INFO] Flash Device ID: %d\r\n", rsp.br_flash_dev_id);
    uart9_printf("[INFO] Header Address: 0x%p\r\n", rsp.br_hdr);

    /* Verification Success: Green LED On, Blue Off */
    R_IOPORT_PinWrite(&g_ioport_ctrl, LED_BLUE, BSP_IO_LEVEL_LOW);
    R_IOPORT_PinWrite(&g_ioport_ctrl, LED_GREEN, BSP_IO_LEVEL_HIGH);
    
    uart9_print("[INFO] Jumping to Application...\r\n");
    
    /* Enter the application. */
    RM_MCUBOOT_PORT_BootApp(&rsp);
}

void hal_entry(void)
{
    mcuboot_quick_setup();
#if BSP_TZ_SECURE_BUILD
    R_BSP_NonSecureEnter();
#endif
}

#if BSP_TZ_SECURE_BUILD
FSP_CPP_HEADER
BSP_CMSE_NONSECURE_ENTRY void template_nonsecure_callable ();
BSP_CMSE_NONSECURE_ENTRY void template_nonsecure_callable () {}
FSP_CPP_FOOTER
#endif
