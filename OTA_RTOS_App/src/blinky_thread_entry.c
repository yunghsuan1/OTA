/*
* Copyright (c) 2020 - 2025 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

#include "blinky_thread.h"
#include "nx_api.h"

/* 引入底層暫存器定義 */
#include "r_ether_api.h"

extern bsp_leds_t g_bsp_leds;
extern NX_IP g_ip0;
extern NX_PACKET_POOL g_packet_pool0;
extern ioport_instance_ctrl_t g_ioport_ctrl;
extern const ioport_cfg_t g_bsp_pin_cfg;

/* 手動實做變數實體 */
NX_IP          g_ip0;
NX_PACKET_POOL g_packet_pool0;

/* [重要] 封包池必須放在 .ns_buffer 區段，否則 Non-secure 的 EDMAC 硬體無法存取 (TrustZone 限制) */
uint8_t g_packet_pool0_pool_memory[(G_PACKET_POOL0_PACKET_NUM * (G_PACKET_POOL0_PACKET_SIZE + sizeof(NX_PACKET)))] BSP_PLACE_IN_SECTION(".ns_buffer");

uint8_t g_ip0_stack_memory[G_IP0_TASK_STACK_SIZE];
uint8_t g_ip0_arp_cache_memory[G_IP0_ARP_CACHE_SIZE];

void g_packet_pool0_quick_setup (void) {
    nx_packet_pool_create(&g_packet_pool0, "g_packet_pool0", G_PACKET_POOL0_PACKET_SIZE, g_packet_pool0_pool_memory, sizeof(g_packet_pool0_pool_memory));
}

void g_ip0_quick_setup (void) {
    nx_ip_create(&g_ip0, "g_ip0", G_IP0_ADDRESS, G_IP0_SUBNET_MASK, &g_packet_pool0, G_IP0_NETWORK_DRIVER, g_ip0_stack_memory, G_IP0_TASK_STACK_SIZE, G_IP0_TASK_PRIORITY);
    nx_arp_enable(&g_ip0, g_ip0_arp_cache_memory, G_IP0_ARP_CACHE_SIZE);
}

void g_ip0_error_handler (UINT status) { (void)status; }

/* Blinky Thread entry function */
void blinky_thread_entry (void)
{
    bsp_leds_t leds = g_bsp_leds;

    /* [關鍵：開啟腳位配置] */
    R_IOPORT_Open(&g_ioport_ctrl, &g_bsp_pin_cfg);

    /* [關鍵：喚醒乙太網路模組電源] 
     * 確保 MSTPB15 (Ethernet 0) 被清除為 0 (啟動) */
    R_MSTP->MSTPCRB &= ~(1U << 15);

    /* [硬體重置 PHY] */
    R_BSP_PinAccessEnable();
    R_BSP_PinWrite(BSP_IO_PORT_04_PIN_03, BSP_IO_LEVEL_LOW);
    R_BSP_SoftwareDelay(100, BSP_DELAY_UNITS_MILLISECONDS);
    R_BSP_PinWrite(BSP_IO_PORT_04_PIN_03, BSP_IO_LEVEL_HIGH);
    R_BSP_SoftwareDelay(200, BSP_DELAY_UNITS_MILLISECONDS);
    R_BSP_PinAccessDisable();

    /* [關鍵：等待 PHY 穩定] 
     * 重置訊號後，PHY 需要一段時間才能完成內部初始化並開始 MDIO 通訊 */
    tx_thread_sleep(200); 

    /* [啟動堆疊] */
    nx_system_initialize();
    g_packet_pool0_quick_setup();
    g_ip0_quick_setup();
    nx_icmp_enable(&g_ip0);
    nx_tcp_enable(&g_ip0);
    nx_udp_enable(&g_ip0);

    ULONG last_rx_count = 0;
    ULONG current_rx_count = 0;

    while (1)
    {
        /* --- 診斷數據獲取 --- */
        
        /* 1. 讀取 EDMAC 狀態暫存器 (Ethernet Event Status Register)
         * Bit 18 (0x00040000): FR (Frame Received) - 只要有封包進來，這格就會被硬體設為 1 */
        uint32_t eesr = R_ETHERC_EDMAC->EESR;
        
        /* [診斷] 讀取 PHY 狀態：嘗試透過 PIR 暫存器手動確認 PHY 是否有 Link (Register 1)
         * 我們直接看 R_ETHERC0->ECSR 暫存器，它能反映 PHY 的 LCHNG (Link Change) 狀態 */
        uint32_t ecsr = R_ETHERC0->ECSR;
        bool link_up = (ecsr & (1U << 2)); // LCHNG 位元 (Bit 2)
        ULONG dummy_ul;
        nx_ip_info_get(&g_ip0, &dummy_ul, &dummy_ul, &current_rx_count, &dummy_ul, &dummy_ul, &dummy_ul, &dummy_ul, &dummy_ul, &dummy_ul, &dummy_ul);

        R_BSP_PinAccessEnable();

        /* LED 1 (藍): 硬體感應燈 --- 直接反應 EESR 暫存器狀態
         * 如果藍燈閃爍，代表硬體「看到了」電訊號封包。 */
        static uint32_t hw_blink_timer = 0;
        if (eesr & 0x00040000) { 
            hw_blink_timer = 2; 
            R_ETHERC_EDMAC->EESR = 0x00040000; // 清除硬體旗標，以便下次偵測
        }
        
        if (hw_blink_timer > 0) {
            R_BSP_PinWrite((bsp_io_port_pin_t) leds.p_leds[0], BSP_IO_LEVEL_HIGH);
            hw_blink_timer--;
        } else {
            /* 如果物理連結 (Link) 是通的，藍燈保持微亮或恆亮做為診斷 */
            if (link_up) {
                R_BSP_PinWrite((bsp_io_port_pin_t) leds.p_leds[0], BSP_IO_LEVEL_HIGH);
            } else {
                R_BSP_PinWrite((bsp_io_port_pin_t) leds.p_leds[0], BSP_IO_LEVEL_LOW);
            }
        }

        /* LED 2 (綠): 軟體收包燈 --- 反應 NetX Duo 是否成功處理封包
         * 如果藍燈閃而綠燈不閃，代表封包卡在「中斷」或「驅動程式」。 */
        static uint32_t sw_blink_timer = 0;
        if (current_rx_count > last_rx_count) {
            sw_blink_timer = 2; 
            last_rx_count = current_rx_count;
        }
        
        if (sw_blink_timer > 0) {
            R_BSP_PinWrite((bsp_io_port_pin_t) leds.p_leds[1], BSP_IO_LEVEL_HIGH);
            sw_blink_timer--;
        } else {
            R_BSP_PinWrite((bsp_io_port_pin_t) leds.p_leds[1], BSP_IO_LEVEL_LOW);
        }

        /* LED 3 (紅): 心跳燈 */
        static bsp_io_level_t heartbeat = BSP_IO_LEVEL_LOW;
        heartbeat = (heartbeat == BSP_IO_LEVEL_LOW) ? BSP_IO_LEVEL_HIGH : BSP_IO_LEVEL_LOW;
        R_BSP_PinWrite((bsp_io_port_pin_t) leds.p_leds[2], heartbeat);

        R_BSP_PinAccessDisable();

        tx_thread_sleep(TX_TIMER_TICKS_PER_SECOND / 10);
    }
}
