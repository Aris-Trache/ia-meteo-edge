/**
  ******************************************************************************
  * @file    app_reseau.c
  * @brief   TP2.3 : FreeRTOS + LwIP (DHCP, ICMP, DNS, sockets BSD) sur la carte.
  *
  *  Taches FreeRTOS :
  *
  *    capteurs_ia (prio 3)  toutes les 2 s : capteurs + IA embarquee (main.c)
  *    reseau      (prio 3)  DHCP, puis toutes les 10 s : ping google.com
  *                          et envoi des mesures a ThingSpeak (si cle configuree)
  *    echo        (prio 2)  serveur TCP "echo" sur le port 7 (socket BSD)
  *    tcpip, eth_rx, eth_lien : taches internes de LwIP et du pilote
  *
  *  LEDs (sujet TP2.3 d) : ROUGE = lecture capteurs, BLEU = communication reseau,
  *  VERT = attente. Logs console : <Lecture Interface Capteurs>, <Communication Reseau>.
  *
  *  Toutes les fonctions reseau utilisent l'API sockets BSD de LwIP
  *  (socket, connect, send, recv, sendto, recvfrom, bind, listen, accept).
  * @author  Aris Trache - ETRS923, USMB Master ESET
  ******************************************************************************
  */
#include "app_reseau.h"
#include "ethernetif.h"
#include "../meteo/meteo_model_params.h"   /* noms des classes */
#include "main.h"

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "lwip/tcpip.h"
#include "lwip/dhcp.h"
#include "lwip/dns.h"
#include "lwip/netif.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "lwip/inet_chksum.h"
#include "lwip/prot/icmp.h"
#include "lwip/prot/ip4.h"

#include <stdio.h>
#include <string.h>

/* Cles ThingSpeak : fichier local NON versionne (voir secrets_thingspeak_exemple.h) */
#if __has_include("secrets_thingspeak.h")
#include "secrets_thingspeak.h"
#endif
#ifndef THINGSPEAK_CLE_ECRITURE
#define THINGSPEAK_CLE_ECRITURE ""
#endif
#ifndef THINGSPEAK_TALKBACK_ID
#define THINGSPEAK_TALKBACK_ID  ""
#endif
#ifndef THINGSPEAK_TALKBACK_CLE
#define THINGSPEAK_TALKBACK_CLE ""
#endif

#define HOTE_PING               "google.com"
#define HOTE_THINGSPEAK         "api.thingspeak.com"
#define PERIODE_RESEAU_MS       20000U   /* ThingSpeak gratuit : 1 envoi / 15 s max */
#define PORT_ECHO               7
#define TIMEOUT_SOCKET_MS       3000

static struct netif interface_eth;
static SemaphoreHandle_t mutex_console;
static SemaphoreHandle_t mutex_mesure;
static MeteoPublication_t derniere_mesure;
static volatile uint32_t nb_ping_ok, nb_ping_total, nb_envois_ts;

/* ---------------------------------------------------------------------------
 * Console partagee entre taches
 * ------------------------------------------------------------------------- */
void console_verrouiller(void)
{
  if (mutex_console != NULL && xTaskGetSchedulerState() == taskSCHEDULER_RUNNING)
  {
    xSemaphoreTake(mutex_console, portMAX_DELAY);
  }
}

void console_liberer(void)
{
  if (mutex_console != NULL && xTaskGetSchedulerState() == taskSCHEDULER_RUNNING)
  {
    xSemaphoreGive(mutex_console);
  }
}

#define LOG(...)  do { console_verrouiller(); printf(__VA_ARGS__); console_liberer(); } while (0)

/* ---------------------------------------------------------------------------
 * Donnees publiees par la tache capteurs/IA
 * ------------------------------------------------------------------------- */
void reseau_publier_mesure(const MeteoPublication_t *m)
{
  if (mutex_mesure != NULL)
  {
    xSemaphoreTake(mutex_mesure, portMAX_DELAY);
    derniere_mesure = *m;
    xSemaphoreGive(mutex_mesure);
  }
}

/* ---------------------------------------------------------------------------
 * Ping (ICMP echo) par socket BSD "brute"
 * ------------------------------------------------------------------------- */
static int ping_une_fois(const ip_addr_t *cible, uint16_t seq, uint32_t *rtt_ms)
{
  struct sockaddr_in dest;
  struct icmp_echo_hdr req;
  uint8_t rep[64];
  struct timeval tv = { 1, 0 };          /* 1 s maximum pour la reponse */
  int ok = 0;

  int s = socket(AF_INET, SOCK_RAW, IP_PROTO_ICMP);
  if (s < 0)
  {
    return 0;
  }
  setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

  memset(&req, 0, sizeof(req));
  ICMPH_TYPE_SET(&req, ICMP_ECHO);
  ICMPH_CODE_SET(&req, 0);
  req.id     = lwip_htons(0xAF25);
  req.seqno  = lwip_htons(seq);
  req.chksum = inet_chksum(&req, sizeof(req));

  memset(&dest, 0, sizeof(dest));
  dest.sin_family = AF_INET;
  inet_addr_from_ip4addr(&dest.sin_addr, ip_2_ip4(cible));

  uint32_t t0 = sys_now();
  if (sendto(s, &req, sizeof(req), 0, (struct sockaddr *)&dest, sizeof(dest)) > 0)
  {
    /* On lit jusqu'a recevoir NOTRE reponse (meme id + meme numero) */
    for (;;)
    {
      int n = recvfrom(s, rep, sizeof(rep), 0, NULL, NULL);
      if (n < (int)(IP_HLEN + sizeof(struct icmp_echo_hdr)))
      {
        break;                                  /* delai depasse */
      }
      struct ip_hdr *iph = (struct ip_hdr *)rep;
      struct icmp_echo_hdr *r = (struct icmp_echo_hdr *)(rep + IPH_HL_BYTES(iph));
      if (ICMPH_TYPE(r) == ICMP_ER && r->id == req.id && r->seqno == req.seqno)
      {
        *rtt_ms = sys_now() - t0;
        ok = 1;
        break;
      }
    }
  }
  closesocket(s);
  return ok;
}

static int resoudre(const char *nom, ip_addr_t *adr)
{
  struct hostent *h = gethostbyname(nom);       /* DNS (serveur donne par le DHCP) */
  if (h == NULL || h->h_addr_list[0] == NULL)
  {
    return 0;
  }
  ip_addr_copy_from_ip4(*adr, *(ip4_addr_t *)h->h_addr_list[0]);
  return 1;
}

static void ping_hote(const char *nom, int n)
{
  ip_addr_t cible;
  uint32_t rtt;

  if (!resoudre(nom, &cible))
  {
    LOG("  ping %s : nom introuvable (DNS)\r\n", nom);
    nb_ping_total++;
    return;
  }
  for (int i = 0; i < n; i++)
  {
    nb_ping_total++;
    if (ping_une_fois(&cible, (uint16_t)nb_ping_total, &rtt))
    {
      nb_ping_ok++;
      LOG("  ping %s (%s) : reponse en %lu ms\r\n", nom, ipaddr_ntoa(&cible), (unsigned long)rtt);
    }
    else
    {
      LOG("  ping %s (%s) : pas de reponse\r\n", nom, ipaddr_ntoa(&cible));
    }
  }
}

/* ---------------------------------------------------------------------------
 * Client HTTP minimal (socket TCP) : ThingSpeak
 * ------------------------------------------------------------------------- */
static int http_get(const char *hote, const char *chemin, char *reponse, size_t taille)
{
  ip_addr_t adr;
  struct sockaddr_in srv;
  struct timeval tv = { TIMEOUT_SOCKET_MS / 1000, 0 };
  char req[384];
  int s, n, total = 0;

  if (!resoudre(hote, &adr))
  {
    return -1;
  }
  s = socket(AF_INET, SOCK_STREAM, 0);
  if (s < 0)
  {
    return -1;
  }
  setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
  setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

  memset(&srv, 0, sizeof(srv));
  srv.sin_family = AF_INET;
  srv.sin_port   = lwip_htons(80);
  inet_addr_from_ip4addr(&srv.sin_addr, ip_2_ip4(&adr));

  if (connect(s, (struct sockaddr *)&srv, sizeof(srv)) != 0)
  {
    closesocket(s);
    return -1;
  }
  n = snprintf(req, sizeof(req),
               "GET %s HTTP/1.0\r\nHost: %s\r\nConnection: close\r\n\r\n", chemin, hote);
  if (send(s, req, (size_t)n, 0) != n)
  {
    closesocket(s);
    return -1;
  }
  while (total < (int)taille - 1)
  {
    n = recv(s, reponse + total, taille - 1U - (size_t)total, 0);
    if (n <= 0)
    {
      break;
    }
    total += n;
  }
  reponse[total] = '\0';
  closesocket(s);
  return total;
}

/* Corps de la reponse HTTP (apres la ligne vide) */
static const char *corps_http(const char *rep)
{
  const char *c = strstr(rep, "\r\n\r\n");
  return (c != NULL) ? c + 4 : "";
}

/* Format "12.34" sans printf flottant (newlib-nano) */
static void flottant_txt(char *b, size_t n, float v)
{
  int32_t c = (int32_t)(v * 100.0f + ((v >= 0.0f) ? 0.5f : -0.5f));
  snprintf(b, n, "%s%ld.%02ld", (c < 0) ? "-" : "", (long)((c < 0 ? -c : c) / 100),
           (long)((c < 0 ? -c : c) % 100));
}

static void envoyer_thingspeak(void)
{
  static char rep[2048];           /* les en-tetes HTTP de ThingSpeak font ~700 octets */
  char chemin[256], t[16], hr[16], p[16], pp[16];
  MeteoPublication_t m;

  if (THINGSPEAK_CLE_ECRITURE[0] == '\0')
  {
    LOG("  ThingSpeak : pas de cle (creer Core/Inc/secrets_thingspeak.h)\r\n");
    return;
  }
  xSemaphoreTake(mutex_mesure, portMAX_DELAY);
  m = derniere_mesure;
  xSemaphoreGive(mutex_mesure);
  if (!m.valide)
  {
    LOG("  ThingSpeak : pas encore de mesure valide\r\n");
    return;
  }

  flottant_txt(t, sizeof(t), m.temp_c);
  flottant_txt(hr, sizeof(hr), m.humidite_rh);
  flottant_txt(p, sizeof(p), m.pression_mer_hpa);
  flottant_txt(pp, sizeof(pp), 100.0f * m.proba_precip);
  /* Champs du canal CAPTEURS (voir matlab/README.md) */
  snprintf(chemin, sizeof(chemin), "/update?api_key=%s&field1=%s&field2=%s&field3=%s&field4=%d&field5=%s",
           THINGSPEAK_CLE_ECRITURE, t, hr, p, m.classe + 1, pp);

  if (http_get(HOTE_THINGSPEAK, chemin, rep, sizeof(rep)) > 0)
  {
    const char *c = corps_http(rep);
    if (c[0] != '\0' && c[0] != '0')
    {
      nb_envois_ts++;
      LOG("  ThingSpeak : mesure enregistree (entree n. %s)\r\n", c);
    }
    else
    {
      /* Diagnostic : ligne de statut HTTP (ex. "HTTP/1.1 200 OK") + reponse */
      const char *fin = strstr(rep, "\r\n");
      int n = (fin != NULL) ? (int)(fin - rep) : 40;
      LOG("  ThingSpeak : refuse [%.*s] reponse \"%.20s\" (cle fausse ou envoi < 15 s)\r\n",
          n, rep, c);
    }
  }
  else
  {
    LOG("  ThingSpeak : serveur injoignable\r\n");
  }

  /* Commande renvoyee par l'IA du Cloud (TalkBack) */
  if (THINGSPEAK_TALKBACK_ID[0] != '\0')
  {
    snprintf(chemin, sizeof(chemin), "/talkbacks/%s/commands/execute?api_key=%s",
             THINGSPEAK_TALKBACK_ID, THINGSPEAK_TALKBACK_CLE);
    if (http_get(HOTE_THINGSPEAK, chemin, rep, sizeof(rep)) > 0)
    {
      int classe_cloud = 0, p_precip = 0;
      const char *c = corps_http(rep);
      if (c[0] == '\0')
      {
        LOG("  TalkBack : pas de nouvelle commande (l'IA Cloud tourne toutes les 10 min)\r\n");
      }
      else if (sscanf(c, "METEO:%d:%d", &classe_cloud, &p_precip) == 2 &&
               classe_cloud >= 1 && classe_cloud <= METEO_NB_CLASSES)
      {
        /* Comparaison Edge (carte) / Cloud (MATLAB) sur les memes mesures */
        LOG("  TalkBack : IA Cloud = %s (precip. %d%%) | IA carte = %s -> %s\r\n",
            METEO_CLASS_NAMES[classe_cloud - 1], p_precip, METEO_CLASS_NAMES[m.classe],
            (classe_cloud - 1 == m.classe) ? "D'ACCORD" : "desaccord");
      }
      else
      {
        LOG("  TalkBack : commande inconnue \"%.40s\"\r\n", c);
      }
    }
  }
}

/* ---------------------------------------------------------------------------
 * Serveur echo TCP (socket BSD : socket, bind, listen, accept)
 * ------------------------------------------------------------------------- */
static void tache_echo(void *arg)
{
  struct sockaddr_in adr;
  char tampon[128];
  (void)arg;

  int ecoute = socket(AF_INET, SOCK_STREAM, 0);
  memset(&adr, 0, sizeof(adr));
  adr.sin_family      = AF_INET;
  adr.sin_port        = lwip_htons(PORT_ECHO);
  adr.sin_addr.s_addr = lwip_htonl(INADDR_ANY);
  bind(ecoute, (struct sockaddr *)&adr, sizeof(adr));
  listen(ecoute, 2);

  for (;;)
  {
    int c = accept(ecoute, NULL, NULL);
    if (c < 0)
    {
      vTaskDelay(pdMS_TO_TICKS(100));
      continue;
    }
    LOG("  echo : client connecte sur le port %d\r\n", PORT_ECHO);
    int n;
    while ((n = recv(c, tampon, sizeof(tampon), 0)) > 0)
    {
      send(c, tampon, (size_t)n, 0);
    }
    closesocket(c);
    LOG("  echo : client deconnecte\r\n");
  }
}

/* ---------------------------------------------------------------------------
 * Tache reseau principale
 * ------------------------------------------------------------------------- */
static void afficher_config_ip(void)
{
  const ip_addr_t *dns0 = dns_getserver(0);
  LOG("  IP %s", ip4addr_ntoa(netif_ip4_addr(&interface_eth)));
  LOG("  masque %s", ip4addr_ntoa(netif_ip4_netmask(&interface_eth)));
  LOG("  passerelle %s", ip4addr_ntoa(netif_ip4_gw(&interface_eth)));
  LOG("  DNS %s\r\n", ipaddr_ntoa(dns0));
}

static void tache_reseau(void *arg)
{
  TickType_t reveil;
  (void)arg;

  LOG("<Communication Reseau> initialisation LwIP + Ethernet\r\n");
  tcpip_init(NULL, NULL);

  LOCK_TCPIP_CORE();
  netif_add(&interface_eth, IP4_ADDR_ANY4, IP4_ADDR_ANY4, IP4_ADDR_ANY4, NULL,
            ethernetif_init, tcpip_input);
  netif_set_default(&interface_eth);
  netif_set_up(&interface_eth);                 /* interface "administrativement" active :
                                                   DHCP l'exige. Le LIEN (cable) est gere a part
                                                   par la tache eth_lien (netif_set_link_up/down) */
  dhcp_start(&interface_eth);                   /* le partage Windows sert de serveur DHCP ;
                                                   DHCP emet des que le lien monte */
  UNLOCK_TCPIP_CORE();

  /* Attente de l'adresse IP (DHCP) */
  LOG("  DHCP : attente d'une adresse...\r\n");
  uint32_t t0 = sys_now();
  while (!dhcp_supplied_address(&interface_eth))
  {
    BSP_LED_Toggle(LED_BLUE);
    vTaskDelay(pdMS_TO_TICKS(250));
    if ((sys_now() - t0) > 30000U)
    {
      LOG("  DHCP : toujours rien apres 30 s (cable ? partage de connexion Windows ?)\r\n");
      t0 = sys_now();
    }
  }
  BSP_LED_Off(LED_BLUE);
  LOG("<Communication Reseau> DHCP OK :");
  afficher_config_ip();

  xTaskCreate(tache_echo, "echo", 1024, NULL, 2, NULL);

  /* Sujet TP2.3 c : ping google.com */
  BSP_LED_On(LED_BLUE);
  LOG("<Communication Reseau> ping %s\r\n", HOTE_PING);
  ping_hote(HOTE_PING, 4);
  BSP_LED_Off(LED_BLUE);

  reveil = xTaskGetTickCount();
  for (;;)
  {
    vTaskDelayUntil(&reveil, pdMS_TO_TICKS(PERIODE_RESEAU_MS));
    BSP_LED_On(LED_BLUE);                       /* bleu = communication reseau */
    LOG("<Communication Reseau>\r\n");
    ping_hote(HOTE_PING, 1);
    envoyer_thingspeak();
    uint32_t rx, tx, vide;
    ethernetif_stats(&rx, &tx, &vide);
    LOG("  bilan : ping %lu/%lu OK, ThingSpeak %lu envois, trames rx %lu tx %lu, pile reseau libre %lu mots\r\n",
        (unsigned long)nb_ping_ok, (unsigned long)nb_ping_total, (unsigned long)nb_envois_ts,
        (unsigned long)rx, (unsigned long)tx, (unsigned long)uxTaskGetStackHighWaterMark(NULL));
    BSP_LED_Off(LED_BLUE);
  }
}

/* ---------------------------------------------------------------------------
 * Lancement (depuis main, avant l'ordonnanceur)
 * ------------------------------------------------------------------------- */
void reseau_lancer(TaskFunction_t tache_capteurs, uint32_t pile_capteurs_mots)
{
  mutex_console = xSemaphoreCreateMutex();
  mutex_mesure  = xSemaphoreCreateMutex();
  configASSERT(mutex_console != NULL && mutex_mesure != NULL);

  xTaskCreate(tache_reseau, "reseau", 2048, NULL, 3, NULL);
  xTaskCreate(tache_capteurs, "capteurs_ia", pile_capteurs_mots, NULL, 3, NULL);

  printf("FreeRTOS : demarrage de l'ordonnanceur (tas libre %lu octets)\r\n",
         (unsigned long)xPortGetFreeHeapSize());
  vTaskStartScheduler();
  /* On n'arrive ici que si la memoire manque pour la tache Idle */
  Error_Handler();
}

/* ---------------------------------------------------------------------------
 * Crochets FreeRTOS : erreurs graves -> message + LED rouge fixe
 * ------------------------------------------------------------------------- */
void vApplicationStackOverflowHook(TaskHandle_t t, char *nom)
{
  (void)t;
  printf("\r\n!!! FreeRTOS : debordement de pile dans la tache '%s'\r\n", nom);
  Error_Handler();
}

void vApplicationMallocFailedHook(void)
{
  printf("\r\n!!! FreeRTOS : tas epuise (augmenter configTOTAL_HEAP_SIZE)\r\n");
  Error_Handler();
}

void vAssertCalled(const char *fichier, int ligne)
{
  printf("\r\n!!! FreeRTOS assert : %s ligne %d\r\n", fichier, ligne);
  Error_Handler();
}
