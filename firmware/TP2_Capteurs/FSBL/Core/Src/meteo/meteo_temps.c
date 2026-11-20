/* Entrees heure + saison de l'IA : voir meteo_temps.h */
#include "meteo_temps.h"
#include "main.h"
#include <math.h>
#include <string.h>

#define PI_F 3.14159265f

/* Version de secours si le module reseau n'est pas lie (modes sans reseau) */
__attribute__((weak)) uint32_t reseau_heure_utc(void) { return 0U; }

static int32_t jours_1970(int32_t a, uint32_t m, uint32_t j)
{
  a -= (m <= 2U);
  int32_t ere = (a >= 0 ? a : a - 399) / 400;
  uint32_t ae = (uint32_t)(a - ere * 400);
  uint32_t ja = (153U * (m + (m > 2U ? (uint32_t)-3 : 9U)) + 2U) / 5U + j - 1U;
  return ere * 146097 + (int32_t)(ae * 365U + ae / 4U - ae / 100U + ja) - 719468;
}

/* __DATE__ = "Nov 20 2026", __TIME__ = "08:45:12" (heure locale du PC, France) -> UTC */
static uint32_t epoch_compilation(void)
{
  static const char MOIS[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
  const char *d = __DATE__, *t = __TIME__;
  char mo[4] = { d[0], d[1], d[2], 0 };
  uint32_t m = (uint32_t)((strstr(MOIS, mo) - MOIS) / 3) + 1U;
  uint32_t j = (uint32_t)((d[4] == ' ' ? 0 : d[4] - '0') * 10 + (d[5] - '0'));
  int32_t a = (d[7] - '0') * 1000 + (d[8] - '0') * 100 + (d[9] - '0') * 10 + (d[10] - '0');
  uint32_t s = (uint32_t)((t[0] - '0') * 36000 + (t[1] - '0') * 3600 + (t[3] - '0') * 600 + (t[4] - '0') * 60 +
                          (t[6] - '0') * 10 + (t[7] - '0'));
  uint32_t decalage = (m >= 4U && m <= 10U) ? 7200U : 3600U;   /* heure d'ete / d'hiver (approx.) */
  return (uint32_t)jours_1970(a, m, j) * 86400U + s - decalage;
}

int meteo_temps_calculer(float t[4], int *heure_utc, int *minute)
{
  uint32_t e = reseau_heure_utc();
  int src = METEO_TEMPS_INTERNET;
  if (e == 0U)
  {
    e = epoch_compilation() + HAL_GetTick() / 1000U;
    src = METEO_TEMPS_COMPILATION;
  }
  uint32_t sec = e % 86400U;
  int h = (int)(sec / 3600U);
  /* jour de l'annee (1..366) */
  int32_t z = (int32_t)(e / 86400U);
  int32_t zz = z + 719468, ere = (zz >= 0 ? zz : zz - 146096) / 146097;
  uint32_t je = (uint32_t)(zz - ere * 146097);
  uint32_t ae = (je - je / 1460U + je / 36524U - je / 146096U) / 365U;
  int32_t an = (int32_t)ae + ere * 400;
  uint32_t ja = je - (365U * ae + ae / 4U - ae / 100U), mp = (5U * ja + 2U) / 153U;
  uint32_t mois = mp < 10U ? mp + 3U : mp - 9U;
  an += (mois <= 2U);
  int doy = (int)(z - jours_1970(an, 1U, 1U)) + 1;
  t[0] = sinf(2.0f * PI_F * (float)h / 24.0f);
  t[1] = cosf(2.0f * PI_F * (float)h / 24.0f);
  t[2] = sinf(2.0f * PI_F * (float)doy / 365.25f);
  t[3] = cosf(2.0f * PI_F * (float)doy / 365.25f);
  if (heure_utc != NULL) { *heure_utc = h; }
  if (minute != NULL) { *minute = (int)((sec / 60U) % 60U); }
  return src;
}
