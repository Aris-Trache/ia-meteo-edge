/**
  ******************************************************************************
  * @file    capteurs.c
  * @brief   Capteurs meteo sur I2C : X-NUCLEO-IKS01A3 ou X-NUCLEO-IKS4A1,
  *          reconnue automatiquement au demarrage.
  *
  *  Architecture :
  *
  *    main.c  ->  capteurs.c  ->  drivers ST xxx_reg.c  ->  platform_read/write
  *                (ce fichier)     (registres, conversions)    -> HAL_I2C_Mem_xxx
  *
  *  Les drivers ST ne connaissent pas le microcontroleur : ils appellent deux
  *  fonctions "plateforme" via le pointeur stmdev_ctx_t. C'est ici qu'on les
  *  implemente avec le HAL I2C du STM32N6.
  *
  *  Pourquoi deux cartes ? Le sujet parle de l'IKS01A3, mais la carte fournie
  *  est une IKS4A1 (la remplacante) : le scan I2C trouve 0x44 (SHT40),
  *  0x5D avec WHO_AM_I = 0xB4 (LPS22DF) et 0x38 (STTS22H). Plutot que de
  *  reecrire le programme, on detecte les composants et on les range par
  *  ROLE (humidite / pression / temperature de precision).
  *
  *  Adresses I2C : la doc donne des adresses 7 bits (ex. HTS221 = 0x5F).
  *  Le HAL attend l'adresse 8 bits = 7 bits << 1 (ex. 0xBE). Les drivers ST
  *  fournissent l'adresse 8 bits avec le bit R/W a 1 (ex. 0xBF) : on masque
  *  donc le bit 0 (& 0xFE) avant de la passer au HAL.
  *
  * @author  Aris Trache - ETRS923, USMB Master ESET
  ******************************************************************************
  */
#include "capteurs.h"
#include <stdio.h>
#include <string.h>

#include "stmems/hts221_reg.h"
#include "stmems/lps22hh_reg.h"
#include "stmems/stts751_reg.h"
#include "stmems/lps22df_reg.h"
#include "stmems/stts22h_reg.h"

/* --------------------------------------------------------------------------
 * Couche plateforme : un "peripherique" = un bus I2C + une adresse
 * -------------------------------------------------------------------------- */
#define I2C_TIMEOUT_MS      100U

typedef struct
{
  I2C_HandleTypeDef *hi2c;
  uint16_t           adresse_8b;     /* adresse 7 bits << 1 */
  uint8_t            bit_auto_inc;   /* bit a ajouter au registre si len > 1 */
} PeripheriqueI2C_t;

/* Ecriture de len octets a partir du registre reg */
static int32_t platform_write(void *handle, uint8_t reg,
                              const uint8_t *buf, uint16_t len)
{
  PeripheriqueI2C_t *dev = (PeripheriqueI2C_t *)handle;
  if (len > 1U)
  {
    reg |= dev->bit_auto_inc;
  }
  return (HAL_I2C_Mem_Write(dev->hi2c, dev->adresse_8b, reg,
                            I2C_MEMADD_SIZE_8BIT, (uint8_t *)buf, len,
                            I2C_TIMEOUT_MS) == HAL_OK) ? 0 : -1;
}

/* Lecture de len octets a partir du registre reg */
static int32_t platform_read(void *handle, uint8_t reg,
                             uint8_t *buf, uint16_t len)
{
  PeripheriqueI2C_t *dev = (PeripheriqueI2C_t *)handle;
  if (len > 1U)
  {
    reg |= dev->bit_auto_inc;
  }
  return (HAL_I2C_Mem_Read(dev->hi2c, dev->adresse_8b, reg,
                           I2C_MEMADD_SIZE_8BIT, buf, len,
                           I2C_TIMEOUT_MS) == HAL_OK) ? 0 : -1;
}

static void platform_delay(uint32_t ms)
{
  HAL_Delay(ms);
}

static void ctx_init(stmdev_ctx_t *ctx, PeripheriqueI2C_t *dev, void *priv)
{
  ctx->write_reg = platform_write;
  ctx->read_reg  = platform_read;
  ctx->mdelay    = platform_delay;
  ctx->handle    = dev;
  ctx->priv_data = priv;
}

/* Renvoie 1 si un composant repond (ACK) a cette adresse 8 bits */
static int present(I2C_HandleTypeDef *hi2c, uint16_t adresse_8b)
{
  return HAL_I2C_IsDeviceReady(hi2c, adresse_8b, 2, I2C_TIMEOUT_MS) == HAL_OK;
}

/* --------------------------------------------------------------------------
 * Etat interne
 * -------------------------------------------------------------------------- */
typedef struct { float x0, y0, x1, y1; } Droite_t;   /* calibration HTS221 */

static PeripheriqueI2C_t dev_hr, dev_p, dev_t;       /* un par ROLE */
static stmdev_ctx_t      ctx_hr, ctx_p, ctx_t;
static stts22h_priv_t    priv_stts22h;
static Droite_t          cal_hum, cal_temp;
static uint32_t          capteurs_presents = 0U;

/* Interpolation lineaire a partir des 2 points de calibration usine */
static float interpoler(const Droite_t *d, int16_t x)
{
  return ((d->y1 - d->y0) * (float)x + ((d->x1 * d->y0) - (d->x0 * d->y1)))
         / (d->x1 - d->x0);
}

static float borner_hr(float hr)
{
  if (hr < 0.0f)   { return 0.0f;   }   /* bornes physiques */
  if (hr > 100.0f) { return 100.0f; }
  return hr;
}

/* --------------------------------------------------------------------------
 * Scan du bus
 * -------------------------------------------------------------------------- */
void capteurs_scan_i2c(I2C_HandleTypeDef *hi2c)
{
  uint32_t n = 0U;
  printf("Scan I2C :");
  for (uint16_t a7 = 0x08U; a7 < 0x78U; a7++)
  {
    if (HAL_I2C_IsDeviceReady(hi2c, (uint16_t)(a7 << 1), 1, 5) == HAL_OK)
    {
      printf(" 0x%02X", a7);
      n++;
    }
  }
  printf("  -> %lu peripherique(s)\r\n", (unsigned long)n);
}

/* ==========================================================================
 * ROLE HUMIDITE : HTS221 (IKS01A3) ou SHT40 (IKS4A1)
 * ========================================================================== */
static int init_hts221(I2C_HandleTypeDef *hi2c)
{
  uint8_t id = 0U;

  dev_hr.hi2c         = hi2c;
  dev_hr.adresse_8b   = HTS221_I2C_ADDRESS & 0xFEU;   /* 0xBE (0x5F) */
  dev_hr.bit_auto_inc = 0x80U;                        /* MSB = auto-increment */
  ctx_init(&ctx_hr, &dev_hr, NULL);

  if (!present(hi2c, dev_hr.adresse_8b)) { return 0; }
  if (hts221_device_id_get(&ctx_hr, &id) != 0 || id != HTS221_ID)
  {
    printf("  HTS221  : WHO_AM_I=0x%02X (attendu 0x%02X)\r\n", id, HTS221_ID);
    return 0;
  }

  /* Points de calibration usine (stockes dans le capteur) */
  hts221_hum_adc_point_0_get(&ctx_hr, &cal_hum.x0);
  hts221_hum_rh_point_0_get(&ctx_hr, &cal_hum.y0);
  hts221_hum_adc_point_1_get(&ctx_hr, &cal_hum.x1);
  hts221_hum_rh_point_1_get(&ctx_hr, &cal_hum.y1);
  hts221_temp_adc_point_0_get(&ctx_hr, &cal_temp.x0);
  hts221_temp_deg_point_0_get(&ctx_hr, &cal_temp.y0);
  hts221_temp_adc_point_1_get(&ctx_hr, &cal_temp.x1);
  hts221_temp_deg_point_1_get(&ctx_hr, &cal_temp.y1);

  hts221_block_data_update_set(&ctx_hr, PROPERTY_ENABLE);  /* MSB/LSB coherents */
  hts221_data_rate_set(&ctx_hr, HTS221_ODR_1Hz);
  hts221_power_on_set(&ctx_hr, PROPERTY_ENABLE);

  printf("  HTS221  : OK (adresse 0x%02X, WHO_AM_I=0x%02X)\r\n",
         dev_hr.adresse_8b >> 1, id);
  return 1;
}

/* --- SHT40 (Sensirion) : pas de registres, des COMMANDES d'un octet ------ */
#define SHT40_ADRESSE_8B        (0x44U << 1)
#define SHT40_CMD_MESURE_HAUTE  0xFDU   /* haute repetabilite, ~8,3 ms */
#define SHT40_CMD_NUM_SERIE     0x89U
#define SHT40_CMD_RESET         0x94U

/* CRC-8 Sensirion : polynome 0x31, init 0xFF (exemple datasheet : 0xBEEF -> 0x92) */
static uint8_t sht40_crc8(const uint8_t *d, uint32_t n)
{
  uint8_t crc = 0xFFU;
  for (uint32_t i = 0U; i < n; i++)
  {
    crc ^= d[i];
    for (uint32_t b = 0U; b < 8U; b++)
    {
      crc = (crc & 0x80U) ? (uint8_t)((crc << 1) ^ 0x31U) : (uint8_t)(crc << 1);
    }
  }
  return crc;
}

/* Envoie une commande, attend, lit 6 octets (mot1, crc1, mot2, crc2) */
static int sht40_commande(uint8_t cmd, uint32_t attente_ms,
                          uint16_t *mot1, uint16_t *mot2)
{
  uint8_t rx[6];
  if (HAL_I2C_Master_Transmit(dev_hr.hi2c, SHT40_ADRESSE_8B, &cmd, 1,
                              I2C_TIMEOUT_MS) != HAL_OK) { return 0; }
  HAL_Delay(attente_ms);
  if (HAL_I2C_Master_Receive(dev_hr.hi2c, SHT40_ADRESSE_8B, rx, 6,
                             I2C_TIMEOUT_MS) != HAL_OK) { return 0; }
  if (sht40_crc8(&rx[0], 2) != rx[2] || sht40_crc8(&rx[3], 2) != rx[5])
  {
    return 0;                                   /* trame corrompue */
  }
  *mot1 = (uint16_t)((rx[0] << 8) | rx[1]);
  *mot2 = (uint16_t)((rx[3] << 8) | rx[4]);
  return 1;
}

static int init_sht40(I2C_HandleTypeDef *hi2c)
{
  uint16_t sn_h = 0U, sn_l = 0U;
  uint8_t  cmd = SHT40_CMD_RESET;

  dev_hr.hi2c       = hi2c;
  dev_hr.adresse_8b = SHT40_ADRESSE_8B;
  if (!present(hi2c, dev_hr.adresse_8b)) { return 0; }

  (void)HAL_I2C_Master_Transmit(hi2c, SHT40_ADRESSE_8B, &cmd, 1, I2C_TIMEOUT_MS);
  HAL_Delay(2);                                  /* reset logiciel < 1 ms */

  /* Le numero de serie (avec CRC valides) prouve que c'est bien un SHT4x */
  if (!sht40_commande(SHT40_CMD_NUM_SERIE, 2, &sn_h, &sn_l))
  {
    printf("  SHT40   : repond en 0x44 mais CRC invalide\r\n");
    return 0;
  }
  printf("  SHT40   : OK (adresse 0x44, n. serie 0x%04X%04X)\r\n", sn_h, sn_l);
  return 1;
}

/* ==========================================================================
 * ROLE PRESSION : LPS22HH (IKS01A3) ou LPS22DF (IKS4A1)
 * Meme adresse possible (0x5D / 0x5C) : c'est le WHO_AM_I qui tranche.
 * ========================================================================== */
static int trouver_adresse_pression(I2C_HandleTypeDef *hi2c)
{
  const uint16_t candidats[] = { 0xBAU,    /* 0x5D : SA0 = 1 (cartes ST) */
                                 0xB8U };  /* 0x5C : SA0 = 0 */
  dev_p.hi2c         = hi2c;
  dev_p.bit_auto_inc = 0x00U;    /* auto-increment gere par le capteur */
  for (uint32_t i = 0U; i < sizeof(candidats) / sizeof(candidats[0]); i++)
  {
    if (present(hi2c, candidats[i]))
    {
      dev_p.adresse_8b = candidats[i];
      return 1;
    }
  }
  dev_p.adresse_8b = 0U;
  return 0;
}

static int init_lps22hh(void)
{
  uint8_t  rst = 1U;
  uint32_t t0;

  ctx_init(&ctx_p, &dev_p, NULL);
  lps22hh_reset_set(&ctx_p, PROPERTY_ENABLE);          /* config par defaut */
  t0 = HAL_GetTick();
  do
  {
    lps22hh_reset_get(&ctx_p, &rst);
  } while (rst && (HAL_GetTick() - t0) < 50U);          /* jamais de boucle infinie */

  lps22hh_block_data_update_set(&ctx_p, PROPERTY_ENABLE);
  lps22hh_data_rate_set(&ctx_p, LPS22HH_1_Hz_LOW_NOISE);
  printf("  LPS22HH : OK (adresse 0x%02X, WHO_AM_I=0x%02X)\r\n",
         dev_p.adresse_8b >> 1, LPS22HH_ID);
  return 1;
}

static int init_lps22df(void)
{
  lps22df_md_t md;

  ctx_init(&ctx_p, &dev_p, NULL);
  if (lps22df_init_set(&ctx_p, LPS22DF_RESET) != 0)   { return 0; }
  if (lps22df_init_set(&ctx_p, LPS22DF_DRV_RDY) != 0) { return 0; } /* BDU + auto-inc */

  md.odr = LPS22DF_1Hz;            /* 1 mesure/s suffit pour la meteo      */
  md.avg = LPS22DF_16_AVG;         /* moyenne interne : bruit ~0,3 Pa RMS  */
  md.lpf = LPS22DF_LPF_ODR_DIV_4;  /* filtre passe-bas : lisse les rafales */
  if (lps22df_mode_set(&ctx_p, &md) != 0) { return 0; }

  printf("  LPS22DF : OK (adresse 0x%02X, WHO_AM_I=0x%02X)\r\n",
         dev_p.adresse_8b >> 1, LPS22DF_ID);
  return 1;
}

/* Renvoie le bit CAPTEUR_xxx trouve, ou 0 */
static uint32_t init_pression(I2C_HandleTypeDef *hi2c)
{
  uint8_t id = 0U;

  if (!trouver_adresse_pression(hi2c)) { return 0U; }
  HAL_Delay(10);                                    /* temps de boot du capteur */

  /* WHO_AM_I est au registre 0x0F sur les deux composants */
  ctx_init(&ctx_p, &dev_p, NULL);
  if (platform_read(&dev_p, 0x0FU, &id, 1) != 0) { return 0U; }

  if (id == LPS22HH_ID && init_lps22hh()) { return CAPTEUR_LPS22HH; }
  if (id == LPS22DF_ID && init_lps22df()) { return CAPTEUR_LPS22DF; }

  printf("  Pression: WHO_AM_I=0x%02X inconnu (LPS22HH=0x%02X, LPS22DF=0x%02X)\r\n",
         id, LPS22HH_ID, LPS22DF_ID);
  return 0U;
}

/* ==========================================================================
 * ROLE TEMPERATURE DE PRECISION : STTS751 (IKS01A3) ou STTS22H (IKS4A1)
 * ========================================================================== */
static int init_stts751(I2C_HandleTypeDef *hi2c)
{
  /* L'adresse depend de la resistance d'adressage et de la version (0/1) */
  const uint16_t candidats[] = {
    STTS751_1xxxx_ADD_7K5 & 0xFEU, STTS751_0xxxx_ADD_7K5 & 0xFEU,
    STTS751_1xxxx_ADD_12K & 0xFEU, STTS751_0xxxx_ADD_12K & 0xFEU,
    STTS751_1xxxx_ADD_20K & 0xFEU, STTS751_0xxxx_ADD_20K & 0xFEU,
    STTS751_1xxxx_ADD_33K & 0xFEU, STTS751_0xxxx_ADD_33K & 0xFEU };
  stts751_id_t id;

  memset(&id, 0, sizeof(id));
  dev_t.hi2c         = hi2c;
  dev_t.bit_auto_inc = 0x00U;      /* le driver lit octet par octet */
  ctx_init(&ctx_t, &dev_t, NULL);

  for (uint32_t i = 0U; i < sizeof(candidats) / sizeof(candidats[0]); i++)
  {
    dev_t.adresse_8b = candidats[i];
    if (present(hi2c, candidats[i])
        && stts751_device_id_get(&ctx_t, &id) == 0
        && id.manufacturer_id == STTS751_ID_MAN)
    {
      stts751_temp_data_rate_set(&ctx_t, STTS751_TEMP_ODR_1Hz);
      stts751_resolution_set(&ctx_t, STTS751_12bit);   /* 0,0625 degC */
      printf("  STTS751 : OK (adresse 0x%02X, fabricant=0x%02X, produit=0x%02X)\r\n",
             dev_t.adresse_8b >> 1, id.manufacturer_id, id.product_id);
      return 1;
    }
  }
  dev_t.adresse_8b = 0U;
  return 0;
}

static int init_stts22h(I2C_HandleTypeDef *hi2c)
{
  /* 4 adresses possibles selon la resistance sur la broche ADDR */
  const uint16_t candidats[] = {
    STTS22H_I2C_ADD_H & 0xFEU,       STTS22H_I2C_ADD_L & 0xFEU,
    STTS22H_I2C_ADD_15K_OHM & 0xFEU, STTS22H_I2C_ADD_56K_OHM & 0xFEU };
  uint8_t id = 0U;

  dev_t.hi2c         = hi2c;
  dev_t.bit_auto_inc = 0x00U;
  priv_stts22h.has_autoincrement = 0U;
  ctx_init(&ctx_t, &dev_t, &priv_stts22h);

  for (uint32_t i = 0U; i < sizeof(candidats) / sizeof(candidats[0]); i++)
  {
    dev_t.adresse_8b = candidats[i];
    if (present(hi2c, candidats[i])
        && stts22h_dev_id_get(&ctx_t, &id) == 0 && id == STTS22H_ID)
    {
      stts22h_auto_increment_set(&ctx_t, PROPERTY_ENABLE);  /* lecture L+H d'un coup */
      stts22h_block_data_update_set(&ctx_t, PROPERTY_ENABLE);
      stts22h_temp_data_rate_set(&ctx_t, STTS22H_1Hz);      /* 0,01 degC / LSB */
      printf("  STTS22H : OK (adresse 0x%02X, WHO_AM_I=0x%02X)\r\n",
             dev_t.adresse_8b >> 1, id);
      return 1;
    }
  }
  dev_t.adresse_8b = 0U;
  return 0;
}

/* ==========================================================================
 * API publique
 * ========================================================================== */
uint32_t capteurs_init(I2C_HandleTypeDef *hi2c)
{
  capteurs_presents = 0U;
  printf("Initialisation des capteurs :\r\n");

  if      (init_hts221(hi2c)) { capteurs_presents |= CAPTEUR_HTS221; }
  else if (init_sht40(hi2c))  { capteurs_presents |= CAPTEUR_SHT40;  }
  else                        { printf("  Humidite : AUCUN capteur (HTS221 / SHT40)\r\n"); }

  {
    uint32_t p = init_pression(hi2c);
    if (p != 0U) { capteurs_presents |= p; }
    else         { printf("  Pression : AUCUN capteur (LPS22HH / LPS22DF)\r\n"); }
  }

  if      (init_stts751(hi2c)) { capteurs_presents |= CAPTEUR_STTS751; }
  else if (init_stts22h(hi2c)) { capteurs_presents |= CAPTEUR_STTS22H; }
  else                         { printf("  T precision : AUCUN capteur (STTS751 / STTS22H)\r\n"); }

  printf("Carte detectee : %s\r\n", capteurs_carte());
  return capteurs_presents;
}

const char *capteurs_carte(void)
{
  const uint32_t iks01a3 = CAPTEUR_HTS221 | CAPTEUR_LPS22HH | CAPTEUR_STTS751;
  const uint32_t iks4a1  = CAPTEUR_SHT40  | CAPTEUR_LPS22DF | CAPTEUR_STTS22H;

  if (capteurs_presents == 0U)                   { return "aucune"; }
  if ((capteurs_presents & ~iks01a3) == 0U)      { return "X-NUCLEO-IKS01A3"; }
  if ((capteurs_presents & ~iks4a1) == 0U)       { return "X-NUCLEO-IKS4A1"; }
  return "mixte";
}

const char *capteurs_nom_role(uint32_t role_mesure)
{
  switch (role_mesure)
  {
    case MESURE_HUMIDITE:
      return (capteurs_presents & CAPTEUR_HTS221) ? "HTS221"
           : (capteurs_presents & CAPTEUR_SHT40)  ? "SHT40" : "-";
    case MESURE_PRESSION:
      return (capteurs_presents & CAPTEUR_LPS22HH) ? "LPS22HH"
           : (capteurs_presents & CAPTEUR_LPS22DF) ? "LPS22DF" : "-";
    case MESURE_TEMP_PREC:
      return (capteurs_presents & CAPTEUR_STTS751) ? "STTS751"
           : (capteurs_presents & CAPTEUR_STTS22H) ? "STTS22H" : "-";
    default:
      return "-";
  }
}

uint32_t capteurs_lire(Mesures_t *m)
{
  int16_t  brut16;
  uint32_t brut32;

  memset(m, 0, sizeof(*m));

  /* ---- humidite ---- */
  if (capteurs_presents & CAPTEUR_HTS221)
  {
    int16_t brut_t;
    if (hts221_humidity_raw_get(&ctx_hr, &brut16) == 0
        && hts221_temperature_raw_get(&ctx_hr, &brut_t) == 0)
    {
      m->humidite_rh = borner_hr(interpoler(&cal_hum, brut16));
      m->temp_hr_c   = interpoler(&cal_temp, brut_t);
      m->valide     |= MESURE_HUMIDITE;
    }
  }
  else if (capteurs_presents & CAPTEUR_SHT40)
  {
    uint16_t st = 0U, srh = 0U;
    if (sht40_commande(SHT40_CMD_MESURE_HAUTE, 10, &st, &srh))
    {
      /* Formules de la datasheet Sensirion SHT4x */
      m->temp_hr_c   = -45.0f + 175.0f * (float)st  / 65535.0f;
      m->humidite_rh = borner_hr(-6.0f + 125.0f * (float)srh / 65535.0f);
      m->valide     |= MESURE_HUMIDITE;
    }
  }

  /* ---- pression ---- */
  if (capteurs_presents & CAPTEUR_LPS22HH)
  {
    if (lps22hh_pressure_raw_get(&ctx_p, &brut32) == 0
        && lps22hh_temperature_raw_get(&ctx_p, &brut16) == 0)
    {
      m->pression_hpa = lps22hh_from_lsb_to_hpa(brut32);
      m->temp_p_c     = lps22hh_from_lsb_to_celsius(brut16);
      m->valide      |= MESURE_PRESSION;
    }
  }
  else if (capteurs_presents & CAPTEUR_LPS22DF)
  {
    lps22df_data_t d;
    if (lps22df_data_get(&ctx_p, &d) == 0)
    {
      m->pression_hpa = d.pressure.hpa;
      m->temp_p_c     = d.heat.deg_c;
      m->valide      |= MESURE_PRESSION;
    }
  }

  /* ---- temperature de precision ---- */
  if (capteurs_presents & CAPTEUR_STTS751)
  {
    if (stts751_temperature_raw_get(&ctx_t, &brut16) == 0)
    {
      m->temp_precision_c = stts751_from_lsb_to_celsius(brut16);
      m->valide          |= MESURE_TEMP_PREC;
    }
  }
  else if (capteurs_presents & CAPTEUR_STTS22H)
  {
    if (stts22h_temperature_raw_get(&ctx_t, &brut16) == 0)
    {
      m->temp_precision_c = stts22h_from_lsb_to_celsius(brut16);
      m->valide          |= MESURE_TEMP_PREC;
    }
  }

  return m->valide;
}
