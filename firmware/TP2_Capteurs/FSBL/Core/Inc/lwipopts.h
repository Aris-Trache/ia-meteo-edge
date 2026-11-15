/**
  ******************************************************************************
  * @file    lwipopts.h
  * @brief   Configuration de LwIP 2.2.1 (pile TCP/IP) pour le TP2.3.
  *
  *  Fonctions demandees par le sujet : DHCP + ICMP + API sockets BSD, ping d'un
  *  nom (google.com) donc DNS. On ajoute TCP pour le client HTTP ThingSpeak.
  *
  *  Memoire : le tas LwIP (MEM_SIZE) et les tampons de reception Ethernet sont
  *  places dans la zone NON CACHEABLE de la RAM (section .eth_noncache, 64 Ko en
  *  haut de l'AXISRAM2, voir le .ld et MPU_Config). Le DMA Ethernet lit/ecrit la
  *  RAM sans passer par le D-cache du Cortex-M55 : sans cette precaution, il
  *  lirait des donnees perimees.
  * @author  Aris Trache - ETRS923, USMB Master ESET
  ******************************************************************************
  */
#ifndef LWIPOPTS_H
#define LWIPOPTS_H

/* ---- Systeme : LwIP tourne avec FreeRTOS (pas en mode "bare metal") ---- */
#define NO_SYS                          0
#define SYS_LIGHTWEIGHT_PROT            1
#define LWIP_TCPIP_CORE_LOCKING         1
#define LWIP_FREERTOS_CHECK_CORE_LOCKING 0
#define TCPIP_THREAD_NAME               "tcpip"
#define TCPIP_THREAD_STACKSIZE          4096            /* octets */
#define TCPIP_THREAD_PRIO               5
#define TCPIP_MBOX_SIZE                 16
#define DEFAULT_THREAD_STACKSIZE        2048
#define DEFAULT_RAW_RECVMBOX_SIZE       8
#define DEFAULT_UDP_RECVMBOX_SIZE       8
#define DEFAULT_TCP_RECVMBOX_SIZE       8
#define DEFAULT_ACCEPTMBOX_SIZE         4

/* ---- Memoire ---- */
#define MEM_ALIGNMENT                   4
#define MEM_SIZE                        (12 * 1024)    /* sujet : >= 5000 o */
#define LWIP_RAM_HEAP_POINTER           lwip_ram_heap  /* defini dans ethernetif.c, zone non cacheable */
#if !defined(__ASSEMBLER__)
extern unsigned char lwip_ram_heap[];
#endif
#define MEMP_NUM_PBUF                   16
#define MEMP_NUM_UDP_PCB                6
#define MEMP_NUM_TCP_PCB                6
#define MEMP_NUM_TCP_PCB_LISTEN         2
#define MEMP_NUM_TCP_SEG                16
#define MEMP_NUM_NETCONN                8
#define MEMP_NUM_SYS_TIMEOUT            (LWIP_NUM_SYS_TIMEOUT_INTERNAL + 4)
#define PBUF_POOL_SIZE                  4              /* la reception utilise son propre pool */
#define PBUF_POOL_BUFSIZE               1536

/* ---- Interface ---- */
#define LWIP_ARP                        1
#define ETH_PAD_SIZE                    0
#define LWIP_NETIF_LINK_CALLBACK        1
#define LWIP_NETIF_STATUS_CALLBACK      1
#define LWIP_NETIF_HOSTNAME             1
#define LWIP_NETIF_TX_SINGLE_PBUF       1   /* une trame = un seul tampon (copie en zone non cacheable) */

/* ---- Protocoles ---- */
#define LWIP_IPV4                       1
#define LWIP_IPV6                       0
#define LWIP_ICMP                       1
#define LWIP_RAW                        1   /* socket ICMP brute pour le ping */
#define LWIP_DHCP                       1
#define LWIP_DNS                        1
#define DNS_MAX_SERVERS                 2
#define LWIP_UDP                        1
#define LWIP_TCP                        1
#define TCP_MSS                         1460
#define TCP_SND_BUF                     (4 * TCP_MSS)
#define TCP_WND                         (4 * TCP_MSS)
#define TCP_SND_QUEUELEN                (4 * TCP_SND_BUF / TCP_MSS)
#define LWIP_TCP_KEEPALIVE              1

/* ---- API ---- */
#define LWIP_SUPPORT_CUSTOM_PBUF        1   /* pbufs de reception = tampons DMA (zero copie) */
#define LWIP_NETCONN                    1
#define LWIP_SOCKET                     1   /* API sockets BSD demandee par le sujet */
#define LWIP_COMPAT_SOCKETS             1
#define LWIP_POSIX_SOCKETS_IO_NAMES     0
#define LWIP_SO_RCVTIMEO                1
#define LWIP_SO_SNDTIMEO                1
#define LWIP_SOCKET_SELECT              1
#define LWIP_NETIF_API                  1   /* netifapi_* : lien monte/descendu depuis une tache */
#define SO_REUSE                        1

/* ---- Sommes de controle ----
 * Calculees par logiciel (simple et sur). Le sujet signale des "CRC fail" quand
 * le calcul materiel est mal configure : on evite le probleme a la source. */
#define CHECKSUM_GEN_IP                 1
#define CHECKSUM_GEN_UDP                1
#define CHECKSUM_GEN_TCP                1
#define CHECKSUM_GEN_ICMP               1
#define CHECKSUM_CHECK_IP               1
#define CHECKSUM_CHECK_UDP              1
#define CHECKSUM_CHECK_TCP              1
#define CHECKSUM_CHECK_ICMP             1

/* ---- Statistiques et traces ----
 * Sujet : "verifier que les messages LWIP apparaissent sur la console".
 * TRACES_LWIP a 1 : les messages DHCP / ICMP / DNS de LwIP s'affichent.
 * Attention : LwIP teste "#ifdef LWIP_DEBUG", donc on ne le definit QUE si on
 * veut les traces (le definir a 0 les activerait quand meme). */
#define TRACES_LWIP                     1
#define LWIP_STATS                      1
#define LWIP_STATS_DISPLAY              0
#if TRACES_LWIP
#define LWIP_DEBUG
#endif
#define LWIP_DEBUG_TIMERNAMES           0   /* pas de "dns_tmr" chaque seconde */
#define LWIP_DBG_MIN_LEVEL              LWIP_DBG_LEVEL_ALL
#define DHCP_DEBUG                      LWIP_DBG_ON
#define ETHARP_DEBUG                    LWIP_DBG_OFF
#define ICMP_DEBUG                      LWIP_DBG_ON
#define DNS_DEBUG                       LWIP_DBG_OFF

#endif /* LWIPOPTS_H */
