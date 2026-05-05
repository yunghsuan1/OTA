#ifndef BSP_LINKER_INFO_H
#define BSP_LINKER_INFO_H

/* This block is required by rm_mcuboot_port_sign.py */
#ifdef SIGN_SCRIPT_PY
#define HEADER_SIZE 0x80
#define SLOT_SIZE 0xF8000
#endif

#endif /* BSP_LINKER_INFO_H */
