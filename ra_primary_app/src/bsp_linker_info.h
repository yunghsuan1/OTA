#ifndef BSP_LINKER_INFO_H
#define BSP_LINKER_INFO_H

/* MCUboot Partition Layout for RA6M5 (2MB Flash) */

/* Offset and Size for Primary Slot (Slot 0) */
#define FLASH_AREA_0P_OFFSET          (0x00008000)
#define FLASH_AREA_0P_SIZE            (0x000F8000) /* ~1MB minus some overhead if needed, or full 1MB */

/* Offset and Size for Secondary Slot (Slot 1) */
#define FLASH_AREA_0S_OFFSET          (0x00100000)
#define FLASH_AREA_0S_SIZE            (0x000F8000)

/* MCUboot Header and Trailer Sizes */
#define IMAGE_HEADER_SIZE             (0x80)
#define IMAGE_TRAILER_SIZE            (0x800)

/* The script specifically looks for HEADER_SIZE in some versions */
#define HEADER_SIZE                   IMAGE_HEADER_SIZE

/* Scratch Area (not used in overwrite-only mode but good to have) */
#define FLASH_AREA_SCRATCH_OFFSET     (0x001F8000)
#define FLASH_AREA_SCRATCH_SIZE       (0x00008000)

#endif /* BSP_LINKER_INFO_H */
