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
#include <stdlib.h>
#include "page_web.h"                 /* tableau de bord servi par la carte */

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
#define PERIODE_RATTRAPAGE_MS   16000U   /* vidage de la memoire apres coupure (> 15 s) */
#define PORT_WEB                80
#define TAMPON_HORS_LIGNE       90       /* 90 x 20 s = 30 min de mesures gardees sans Internet */
#define HISTO_WEB               90       /* 90 x 2 s = 3 min de courbe sur la page web */

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
/* Historique court pour la page web (dixiemes) */
static int16_t histo_hr[HISTO_WEB], histo_pp[HISTO_WEB], histo_t[HISTO_WEB];
static uint16_t histo_n, histo_tete;
static volatile uint32_t nb_predictions;
static volatile uint32_t bench_ns[4];          /* float sans caches, float, int8, NPU */

void reseau_publier_mesure(const MeteoPublication_t *m)
{
  if (mutex_mesure != NULL)
  {
    xSemaphoreTake(mutex_mesure, portMAX_DELAY);
    derniere_mesure = *m;
    histo_hr[histo_tete] = (int16_t)(m->humidite_rh * 10.0f);
    histo_pp[histo_tete] = (int16_t)(m->proba[METEO_NB_CLASSES - 1] * 1000.0f);
    histo_t[histo_tete]  = (int16_t)(m->temp_c * 10.0f);
    histo_tete = (uint16_t)((histo_tete + 1U) % HISTO_WEB);
    if (histo_n < HISTO_WEB) { histo_n++; }
    nb_predictions++;
    xSemaphoreGive(mutex_mesure);
  }
}

void reseau_publier_bench(uint32_t ns_off, uint32_t ns_f32, uint32_t ns_s8, uint32_t ns_npu)
{
  bench_ns[0] = ns_off; bench_ns[1] = ns_f32; bench_ns[2] = ns_s8; bench_ns[3] = ns_npu;
}

/* ---------------------------------------------------------------------------
 * Mode hors ligne : sans Internet, l'IA embarquee continue ; les mesures sont
 * gardees en memoire puis renvoyees au Cloud, avec leur VRAIE date, au retour
 * du reseau. L'heure vient de l'en-tete "Date:" des reponses ThingSpeak.
 * ------------------------------------------------------------------------- */
typedef struct
{
  uint32_t t_ms;                 /* sys_now() au moment de la mesure */
  float    temp_c, hr, p_mer, pp;
  int8_t   classe;
} EntreeHorsLigne_t;

static EntreeHorsLigne_t tampon_hl[TAMPON_HORS_LIGNE];
static uint16_t hl_tete, hl_n;                 /* file circulaire (plus ancienne = hl_tete) */
static volatile int en_ligne = 1;
static volatile uint32_t nb_rattrapees;
static uint32_t epoch_ref, tick_ref;           /* heure UTC (s) connue a sys_now() = tick_ref */
static volatile int cloud_classe = -1;         /* derniere reponse de l'IA Cloud (TalkBack) */

static void hl_ajouter(const EntreeHorsLigne_t *e)
{
  uint16_t i = (uint16_t)((hl_tete + hl_n) % TAMPON_HORS_LIGNE);
  tampon_hl[i] = *e;
  if (hl_n < TAMPON_HORS_LIGNE) { hl_n++; }
  else { hl_tete = (uint16_t)((hl_tete + 1U) % TAMPON_HORS_LIGNE); }   /* plein : on perd la + ancienne */
}

/* Jours depuis 1970-01-01 (algorithme civil de H. Hinnant) et inverse */
static int32_t jours_depuis_1970(int32_t a, uint32_t m, uint32_t j)
{
  a -= (m <= 2U);
  int32_t ere = (a >= 0 ? a : a - 399) / 400;
  uint32_t ae = (uint32_t)(a - ere * 400);
  uint32_t ja = (153U * (m + (m > 2U ? (uint32_t)-3 : 9U)) + 2U) / 5U + j - 1U;
  uint32_t je = ae * 365U + ae / 4U - ae / 100U + ja;
  return ere * 146097 + (int32_t)je - 719468;
}

static void date_iso(uint32_t epoch, char *b, size_t n)
{
  int32_t z = (int32_t)(epoch / 86400U) + 719468;
  uint32_t sec = epoch % 86400U;
  int32_t ere = (z >= 0 ? z : z - 146096) / 146097;
  uint32_t je = (uint32_t)(z - ere * 146097);
  uint32_t ae = (je - je / 1460U + je / 36524U - je / 146096U) / 365U;
  int32_t a = (int32_t)ae + ere * 400;
  uint32_t ja = je - (365U * ae + ae / 4U - ae / 100U);
  uint32_t mp = (5U * ja + 2U) / 153U;
  uint32_t j = ja - (153U * mp + 2U) / 5U + 1U;
  uint32_t m = mp < 10U ? mp + 3U : mp - 9U;
  a += (m <= 2U);
  snprintf(b, n, "%04ld-%02lu-%02luT%02lu:%02lu:%02luZ", (long)a, (unsigned long)m, (unsigned long)j,
           (unsigned long)(sec / 3600U), (unsigned long)((sec / 60U) % 60U), (unsigned long)(sec % 60U));
}

/* "Date: Thu, 19 Nov 2026 16:52:44 GMT" -> heure de reference */
static void maj_heure(const char *rep)
{
  static const char MOIS[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
  const char *d = strstr(rep, "\r\nDate: ");
  char mo[4] = { 0 };
  int j, a, h, mi, se;
  if (d == NULL || sscanf(d + 8, "%*3s, %d %3s %d %d:%d:%d", &j, mo, &a, &h, &mi, &se) != 6)
  {
    return;
  }
  const char *pm = strstr(MOIS, mo);
  if (pm == NULL) { return; }
  uint32_t m = (uint32_t)((pm - MOIS) / 3) + 1U;
  epoch_ref = (uint32_t)jours_depuis_1970(a, m, (uint32_t)j) * 86400U +
              (uint32_t)(h * 3600 + mi * 60 + se);
  tick_ref = sys_now();
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

/* Envoie une entree : 1 = enregistree, 0 = refusee par ThingSpeak, -1 = Internet injoignable */
static int envoyer_entree(const EntreeHorsLigne_t *e, int avec_date, char *rep, size_t taille)
{
  char chemin[256], t[16], hr[16], p[16], pp[16], date[40] = "";
  flottant_txt(t, sizeof(t), e->temp_c);
  flottant_txt(hr, sizeof(hr), e->hr);
  flottant_txt(p, sizeof(p), e->p_mer);
  flottant_txt(pp, sizeof(pp), 100.0f * e->pp);
  if (avec_date && epoch_ref != 0U)
  {
    strcpy(date, "&created_at=");
    date_iso(epoch_ref + (e->t_ms - tick_ref) / 1000U, date + 12, sizeof(date) - 12U);
  }
  /* Champs du canal CAPTEURS (voir matlab/README.md) */
  snprintf(chemin, sizeof(chemin), "/update?api_key=%s&field1=%s&field2=%s&field3=%s&field4=%d&field5=%s%s",
           THINGSPEAK_CLE_ECRITURE, t, hr, p, e->classe + 1, pp, date);
  if (http_get(HOTE_THINGSPEAK, chemin, rep, taille) <= 0)
  {
    return -1;
  }
  maj_heure(rep);
  const char *c = corps_http(rep);
  if (c[0] != '\0' && c[0] != '0')
  {
    nb_envois_ts++;
    return 1;
  }
  const char *fin = strstr(rep, "\r\n");
  int n = (fin != NULL) ? (int)(fin - rep) : 40;
  LOG("  ThingSpeak : refuse [%.*s] reponse \"%.20s\" (cle fausse ou envoi < 15 s)\r\n", n, rep, c);
  return 0;
}

static void envoyer_thingspeak(void)
{
  static char rep[2048];           /* les en-tetes HTTP de ThingSpeak font ~700 octets */
  char chemin[160];
  MeteoPublication_t m;
  EntreeHorsLigne_t e;

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
  e.t_ms = sys_now(); e.temp_c = m.temp_c; e.hr = m.humidite_rh; e.p_mer = m.pression_mer_hpa;
  e.pp = m.proba[METEO_NB_CLASSES - 1]; e.classe = (int8_t)m.classe;

  if (hl_n > 0U)
  {
    /* Rattrapage : on renvoie la plus ancienne mesure gardee, avec sa date d'origine */
    const EntreeHorsLigne_t *v = &tampon_hl[hl_tete];
    int r = envoyer_entree(v, 1, rep, sizeof(rep));
    if (r < 0)
    {
      hl_ajouter(&e);
      en_ligne = 0;
      LOG("  HORS LIGNE : l'IA continue sur la carte, mesure gardee en memoire (%u en attente)\r\n", hl_n);
      return;
    }
    en_ligne = 1;
    hl_tete = (uint16_t)((hl_tete + 1U) % TAMPON_HORS_LIGNE);
    hl_n--;
    if (r > 0) { nb_rattrapees++; }
    LOG("  RATTRAPAGE : mesure prise il y a %lu s renvoyee au Cloud avec sa date (reste %u)\r\n",
        (unsigned long)((sys_now() - v->t_ms) / 1000U), hl_n);
    return;                        /* 1 envoi / 15 s max : la mesure courante attend */
  }

  int r = envoyer_entree(&e, 0, rep, sizeof(rep));
  if (r < 0)
  {
    hl_ajouter(&e);
    en_ligne = 0;
    LOG("  HORS LIGNE : Internet injoignable -> l'IA continue sur la carte, mesure gardee (%u en attente)\r\n", hl_n);
    return;
  }
  en_ligne = 1;
  if (r > 0)
  {
    LOG("  ThingSpeak : mesure enregistree (entree n. %s)\r\n", corps_http(rep));
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
        cloud_classe = classe_cloud - 1;
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
 * Serveur web embarque (port 80) : la carte sert elle-meme son tableau de bord.
 *   GET /     -> page HTML (page_web.h)
 *   GET /api  -> etat en JSON (mesure, probabilites, temps d'inference, Cloud)
 * Marche sans Internet : il suffit du cable carte <-> PC.
 * ------------------------------------------------------------------------- */
static int json_etat(char *b, size_t n)
{
  MeteoPublication_t m;
  int16_t hr[HISTO_WEB], pp[HISTO_WEB], tt[HISTO_WEB];
  uint16_t k, tete;
  char t[16], h[16], p[16], f[4][16];

  xSemaphoreTake(mutex_mesure, portMAX_DELAY);
  m = derniere_mesure; k = histo_n; tete = histo_tete;
  memcpy(hr, histo_hr, sizeof(hr)); memcpy(pp, histo_pp, sizeof(pp)); memcpy(tt, histo_t, sizeof(tt));
  xSemaphoreGive(mutex_mesure);

  flottant_txt(t, sizeof(t), m.temp_c);
  flottant_txt(h, sizeof(h), m.humidite_rh);
  flottant_txt(p, sizeof(p), m.pression_mer_hpa);
  for (int i = 0; i < 4; i++) { flottant_txt(f[i], sizeof(f[i]), (float)bench_ns[i] / 1000.0f); }
  int pr[3];
  for (int i = 0; i < 3; i++) { pr[i] = (int)(m.proba[i] * 100.0f + 0.5f); }

  int l = snprintf(b, n, "{\"T\":%s,\"H\":%s,\"P\":%s,\"c\":%d,\"pr\":[%d,%d,%d],\"n\":%lu,"
                   "\"up\":%lu,\"net\":%d,\"att\":%u,\"rat\":%lu,\"ts\":%lu,\"cl\":%d,"
                   "\"off\":%s,\"f32\":%s,\"s8\":%s,\"npu\":%s,\"hH\":[",
                   t, h, p, m.classe, pr[0], pr[1], pr[2], (unsigned long)nb_predictions,
                   (unsigned long)(sys_now() / 1000U), en_ligne, hl_n, (unsigned long)nb_rattrapees,
                   (unsigned long)nb_envois_ts, cloud_classe, f[0], f[1], f[2], f[3]);
  for (int pass = 0; pass < 3; pass++)
  {
    const int16_t *v = (pass == 0) ? hr : (pass == 1) ? pp : tt;
    for (uint16_t i = 0; i < k && l < (int)n - 16; i++)
    {
      int16_t x = v[(tete + HISTO_WEB - k + i) % HISTO_WEB];
      l += snprintf(b + l, n - (size_t)l, "%s%s%d.%d", i ? "," : "", (x < 0) ? "-" : "",
                    (x < 0 ? -x : x) / 10, (x < 0 ? -x : x) % 10);
    }
    l += snprintf(b + l, n - (size_t)l, (pass == 0) ? "],\"hP\":[" : (pass == 1) ? "],\"hT\":[" : "]}");
  }
  return l;
}

static void envoyer_tout(int c, const char *d, size_t n)
{
  while (n > 0U)
  {
    int k = send(c, d, (n > 1024U) ? 1024U : n, 0);
    if (k <= 0) { return; }
    d += k; n -= (size_t)k;
  }
}

static void tache_web(void *arg)
{
  static char req[512], json[3072], ent[160];
  struct sockaddr_in adr;
  struct timeval tv = { 2, 0 };
  (void)arg;

  int ecoute = socket(AF_INET, SOCK_STREAM, 0);
  memset(&adr, 0, sizeof(adr));
  adr.sin_family      = AF_INET;
  adr.sin_port        = lwip_htons(PORT_WEB);
  adr.sin_addr.s_addr = lwip_htonl(INADDR_ANY);
  bind(ecoute, (struct sockaddr *)&adr, sizeof(adr));
  listen(ecoute, 4);

  for (;;)
  {
    int c = accept(ecoute, NULL, NULL);
    if (c < 0) { vTaskDelay(pdMS_TO_TICKS(100)); continue; }
    setsockopt(c, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(c, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    int n = recv(c, req, sizeof(req) - 1U, 0);
    if (n > 0)
    {
      req[n] = '\0';
      const char *corps; size_t lg; const char *type;
      if (strncmp(req, "GET /api", 8) == 0)
      {
        lg = (size_t)json_etat(json, sizeof(json)); corps = json; type = "application/json";
      }
      else
      {
        corps = PAGE_WEB; lg = sizeof(PAGE_WEB) - 1U; type = "text/html; charset=utf-8";
      }
      int k = snprintf(ent, sizeof(ent), "HTTP/1.0 200 OK\r\nContent-Type: %s\r\nContent-Length: %u\r\n"
                       "Cache-Control: no-store\r\nConnection: close\r\n\r\n", type, (unsigned)lg);
      envoyer_tout(c, ent, (size_t)k);
      envoyer_tout(c, corps, lg);
    }
    closesocket(c);
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
  xTaskCreate(tache_web, "web", 1536, NULL, 2, NULL);
  LOG("<Serveur web> tableau de bord en direct : http://%s/  (marche sans Internet)\r\n",
      ip4addr_ntoa(netif_ip4_addr(&interface_eth)));

  /* Sujet TP2.3 c : ping google.com */
  BSP_LED_On(LED_BLUE);
  LOG("<Communication Reseau> ping %s\r\n", HOTE_PING);
  ping_hote(HOTE_PING, 4);
  BSP_LED_Off(LED_BLUE);

  reveil = xTaskGetTickCount();
  for (;;)
  {
    vTaskDelayUntil(&reveil, pdMS_TO_TICKS((hl_n > 0U && en_ligne) ? PERIODE_RATTRAPAGE_MS : PERIODE_RESEAU_MS));
    BSP_LED_On(LED_BLUE);                       /* bleu = communication reseau */
    LOG("<Communication Reseau>\r\n");
    if (en_ligne)
    {
      ping_hote(HOTE_PING, 1);                  /* hors ligne : inutile (DNS en echec) */
    }
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
