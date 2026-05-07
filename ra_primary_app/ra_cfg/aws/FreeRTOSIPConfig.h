/* generated configuration header file - do not edit */
#ifndef FREERTOSIPCONFIG_H_
#define FREERTOSIPCONFIG_H_

#ifndef FREERTOS_IP_CONFIG_H
#define FREERTOS_IP_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* --- Diagnostic Settings --- */
#define ipconfigHAS_DEBUG_PRINTF    0
#define ipconfigHAS_PRINTF          0
#define ipconfigBYTE_ORDER          pdFREERTOS_LITTLE_ENDIAN

/* --- [關鍵修復] 強制將網路緩衝區放在非安全區 --- */
/* 如果不放在 .ns_buffer，RA6M5 的 EDMAC (Non-secure Master) 會因為權限問題無法讀取單播封包 */
#define ipconfigFREERTOS_NETWORK_BUFFER_SECTION __attribute__((section(".ns_buffer")))

/* --- [關鍵修復] 關閉硬體校驗和卸載 --- */
/* 在 TrustZone 專案中，硬體校驗常因記憶體對齊/屬性衝突導致 Ping 失敗，改用軟體校驗最穩 */
#define ipconfigDRIVER_INCLUDED_RX_IP_CHECKSUM     0
#define ipconfigDRIVER_INCLUDED_TX_IP_CHECKSUM     0

/* --- 基本 FreeRTOS+TCP 設定 --- */
#define ipconfigSOCK_DEFAULT_RECEIVE_BLOCK_TIME    (2000)
#define ipconfigSOCK_DEFAULT_SEND_BLOCK_TIME       (2000)
#define ipconfigUSE_DNS_CACHE                      (1)
#define ipconfigDNS_REQUEST_ATTEMPTS               (2)
#define ipconfigIP_TASK_PRIORITY                   (configMAX_PRIORITIES - 1)
#define ipconfigIP_TASK_STACK_SIZE_WORDS           (configMINIMAL_STACK_SIZE * 10)
#define ipconfigUSE_NETWORK_EVENT_HOOK             0
#define ipconfigUDP_MAX_SEND_BLOCK_TIME_TICKS      (15000 / portTICK_PERIOD_MS)
#define ipconfigUSE_DHCP                           0
#define ipconfigDHCP_REGISTER_HOSTNAME             1
#define ipconfigDHCP_USES_UNICAST                  1
#define ipconfigUSE_DHCP_HOOK                      1
#define ipconfigMAXIMUM_DISCOVER_TX_PERIOD         (120000 / portTICK_PERIOD_MS)
#define ipconfigARP_CACHE_ENTRIES                  6
#define ipconfigMAX_ARP_RETRANSMISSIONS            (5)
#define ipconfigMAX_ARP_AGE                        150
#define ipconfigINCLUDE_FULL_INET_ADDR             1

/* --- 緩衝區設定 (已優化) --- */
#define ipconfigNUM_NETWORK_BUFFER_DESCRIPTORS     32
#define ipconfigEVENT_QUEUE_LENGTH                 (ipconfigNUM_NETWORK_BUFFER_DESCRIPTORS + 5)
#define ipconfigALLOW_SOCKET_SEND_WITHOUT_BIND      0
#define ipconfigUDP_TIME_TO_LIVE                   128
#define ipconfigTCP_TIME_TO_LIVE                   128
#define ipconfigUSE_TCP                            (1)
#define ipconfigUSE_TCP_WIN                        (0)
#define ipconfigNETWORK_MTU                        1500
#define ipconfigUSE_DNS                            1
#define ipconfigREPLY_TO_INCOMING_PINGS            1
#define ipconfigSUPPORT_OUTGOING_PINGS             1
#define ipconfigSUPPORT_SELECT_FUNCTION            0
#define ipconfigFILTER_OUT_NON_ETHERNET_II_FRAMES  1
#define ipconfigETHERNET_DRIVER_FILTERS_FRAME_TYPES 1
#define ipconfigPACKET_FILLER_SIZE                 2
#define ipconfigTCP_WIN_SEG_COUNT                  240
#define ipconfigTCP_RX_BUFFER_LENGTH               (3000)
#define ipconfigTCP_TX_BUFFER_LENGTH               (3000)
#define ipconfigIS_VALID_PROG_ADDRESS( x )         ( ( x ) != NULL )
#define ipconfigTCP_KEEP_ALIVE                     (1)
#define ipconfigTCP_KEEP_ALIVE_INTERVAL            (120)
#define ipconfigSOCKET_HAS_USER_SEMAPHORE          (0)
#define ipconfigSOCKET_HAS_USER_WAKE_CALLBACK      (1)
#define ipconfigUSE_CALLBACKS                      (0)
#define ipconfigZERO_COPY_TX_DRIVER                (0)
#define ipconfigZERO_COPY_RX_DRIVER                (0)
#define ipconfigUSE_LINKED_RX_MESSAGES             (0)
#define ipconfigUSE_IPv6                           (0)
#define ipconfigIPv4_BACKWARD_COMPATIBLE           (1)

#if defined(__GNUC__) || defined(__ARMCC_VERSION)
  #define portINLINE          __inline
#elif defined(__ICCARM__)
  #define portINLINE          inline
#endif

void vApplicationMQTTGetKeys(const char **ppcRootCA, const char **ppcClientCert, const char **ppcClientPrivateKey);

#ifdef __cplusplus
}
#endif

#endif /* FREERTOS_IP_CONFIG_H */
#endif /* FREERTOSIPCONFIG_H_ */
