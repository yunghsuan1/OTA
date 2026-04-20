#ifndef BSP_LINKER_INFO_H
#define BSP_LINKER_INFO_H

#include "flash_map_backend/flash_map_backend.h"

/* 
 * 手動補足 IDE 找不到的數值定義 (主要是給 IDE 檢查用的)
 */
#ifndef FLASH_DEVICE_INTERNAL_FLASH
  #define FLASH_DEVICE_INTERNAL_FLASH  (0)
#endif

/* 
 * 手動定義 Flash Map 陣列
 * 繞過 FSP 6.1.0 圖形化界面無法設定的問題。
 */

static const struct flash_area flash_map[] =
{
    /* Slot 0: Primary Image */
    {
        .fa_id        = 1,                         /* FLASH_AREA_IMAGE_PRIMARY(0) */
        .fa_device_id = FLASH_DEVICE_INTERNAL_FLASH,
        .fa_off       = 0x00008000,                /* 32KB 偏移 (Bootloader 大小) */
        .fa_size      = 0x000F8000,                /* 1MB - 32KB */
    },
    /* Slot 1: Secondary Image (Bank 1) */
    {
        .fa_id        = 2,                         /* FLASH_AREA_IMAGE_SECONDARY(0) */
        .fa_device_id = FLASH_DEVICE_INTERNAL_FLASH,
        .fa_off       = 0x00100000,                /* Bank 1 起始位址 */
        .fa_size      = 0x00100000,                /* 1MB 大小 */
    }
};

#endif /* BSP_LINKER_INFO_H */
