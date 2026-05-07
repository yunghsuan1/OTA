#include "hal_data.h"

/* LED Pins for EK-RA6M5 */
#define LED_BLUE  BSP_IO_PORT_00_PIN_06
#define LED_GREEN BSP_IO_PORT_00_PIN_07
#define LED_RED   BSP_IO_PORT_00_PIN_08

/* Quick setup for MCUboot. */
void mcuboot_quick_setup()
{
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
    if (0 != boot_go(&rsp))
    {
        /* Verification Failed: Red LED On, Blue Off */
        R_IOPORT_PinWrite(&g_ioport_ctrl, LED_BLUE, BSP_IO_LEVEL_LOW);
        R_IOPORT_PinWrite(&g_ioport_ctrl, LED_RED, BSP_IO_LEVEL_HIGH);
        while(1); /* Hang here for debug visibility */
    }

    /* Verification Success: Green LED On, Blue Off */
    R_IOPORT_PinWrite(&g_ioport_ctrl, LED_BLUE, BSP_IO_LEVEL_LOW);
    R_IOPORT_PinWrite(&g_ioport_ctrl, LED_GREEN, BSP_IO_LEVEL_HIGH);

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
