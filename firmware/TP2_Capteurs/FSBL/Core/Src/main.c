/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "i2c.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "capteurs.h"
#include "meteo/meteo_features.h"
#include "meteo/meteo_mlp_ref.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* Choix de la demonstration (un seul projet pour TP2 + TP3) :
     1 = TP2 partie 1 : chenillard 3 LEDs + logs console
     2 = TP2 partie 2 : lecture des capteurs I2C + LEDs temoins d'etat
     3 = TP3 partie 3 : capteurs + IA embarquee (classification meteo sur la carte) */
#define TP2_PARTIE              3

/* TP3 : periode d'ajout a l'historique du modele. Le modele a ete entraine sur des
   donnees HORAIRES : 3600000 ms est la seule valeur physiquement correcte.
   Mettre 60000 (1 "heure" = 1 minute) UNIQUEMENT pour une demonstration rapide. */
#define METEO_PERIODE_HISTO_MS  3600000U
#define METEO_BENCH_N           1000U   /* taille d'un paquet d'inferences du benchmark */
#define METEO_BENCH_MIN_MS      300U    /* duree minimale de mesure (precision du tick) */

#define CHENILLARD_PERIODE_MS   3000U   /* TP2 1.b : temporisation de 3 s par LED */
#define PERIODE_LECTURE_MS      2000U   /* TP2 2   : une mesure toutes les 2 s    */
#define LED_ROUGE_MIN_MS        150U    /* duree mini LED rouge, pour que l'oeil la voie */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

COM_InitTypeDef BspCOMInit;

/* USER CODE BEGIN PV */
static uint32_t prochaine_lecture_ms = 0U;   /* echeance absolue de la prochaine mesure */

/* TP3 : etat de l'IA embarquee */
static MeteoHistorique_t meteo_hist;
static uint32_t meteo_debut_heure_ms = 0U;
static float    meteo_somme_t = 0.0f, meteo_somme_rh = 0.0f, meteo_somme_p = 0.0f;
static uint32_t meteo_nb_mesures = 0U;
static uint32_t bench_ns_off = 0U, bench_ns_on = 0U;   /* duree d'une inference [ns] */
static uint32_t nb_predictions = 0U;

/* Sequence du chenillard : ordre des LEDs et nom affiche dans les logs.
   Ajouter / retirer une etape = modifier ce tableau, sans toucher a la boucle. */
typedef struct
{
  Led_TypeDef led;
  const char *nom;
} EtapeChenillard_t;

static const EtapeChenillard_t chenillard[] =
{
  { LED_RED,   "ROUGE" },
  { LED_GREEN, "VERT"  },
  { LED_BLUE,  "BLEU"  },
};
#define NB_ETAPES  (sizeof(chenillard) / sizeof(chenillard[0]))
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
static void log_etat(const char *etat);
static void leds_toutes_eteintes(void);
static void etape_chenillard(void) __attribute__((unused));
static void etape_capteurs(void) __attribute__((unused));
static void afficher_valeur(const char *nom, float v, const char *unite, int valide);
static void etape_ia_meteo(void) __attribute__((unused));
static uint32_t mesurer_temps_inference(const char *config) __attribute__((unused));
static void afficher_bench(void) __attribute__((unused));
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_I2C1_Init();
  /* USER CODE BEGIN 2 */

  /* USER CODE END 2 */

  /* Initialize leds */
  BSP_LED_Init(LED_BLUE);
  BSP_LED_Init(LED_RED);
  BSP_LED_Init(LED_GREEN);

  /* Initialize USER push-button, will be used to trigger an interrupt each time it's pressed.*/
  BSP_PB_Init(BUTTON_USER, BUTTON_MODE_EXTI);

  /* Initialize COM1 port (115200, 8 bits (7-bit data + 1 stop bit), no parity */
  BspCOMInit.BaudRate   = 115200;
  BspCOMInit.WordLength = COM_WORDLENGTH_8B;
  BspCOMInit.StopBits   = COM_STOPBITS_1;
  BspCOMInit.Parity     = COM_PARITY_NONE;
  BspCOMInit.HwFlowCtl  = COM_HWCONTROL_NONE;
  if (BSP_COM_Init(COM1, &BspCOMInit) != BSP_ERROR_NONE)
  {
    Error_Handler();
  }

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  log_etat("debut d'application");

  /* Test materiel au demarrage : les trois LEDs allumees ensemble (TP2 1.a) */
  BSP_LED_On(LED_RED);
  BSP_LED_On(LED_GREEN);
  BSP_LED_On(LED_BLUE);
  log_etat("ROUGE + VERT + BLEU allumees");
  HAL_Delay(1000);
  leds_toutes_eteintes();

#if TP2_PARTIE >= 2
  /* TP2 2 : detection des capteurs sur le bus I2C1 (Arduino D14/D15) */
  capteurs_scan_i2c(&hi2c1);
  if (capteurs_init(&hi2c1) == 0U)
  {
    log_etat("AUCUN capteur detecte : verifier la carte IKS01A3 / IKS4A1");
  }
  BSP_LED_On(LED_GREEN);                      /* vert = disponible, en attente */
  prochaine_lecture_ms = HAL_GetTick();
#endif
#if TP2_PARTIE == 3
  meteo_historique_init(&meteo_hist);
  meteo_debut_heure_ms = HAL_GetTick();
  /* TP3 3.c : effet des caches L1 du Cortex-M55 (desactives par defaut dans le FSBL).
     Le programme est pilote par scrutation (pas de DMA) : activer le D-cache est sans
     risque de coherence. On les laisse actives pour la suite. */
  bench_ns_off = mesurer_temps_inference("caches OFF");
  SCB_EnableICache();
  SCB_EnableDCache();
  bench_ns_on  = mesurer_temps_inference("caches ON ");
#endif

  while (1)
  {

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
#if TP2_PARTIE == 1
    etape_chenillard();
#elif TP2_PARTIE == 2
    etape_capteurs();
#else
    etape_ia_meteo();
#endif
  }
  /* USER CODE END 3 */
}
/* USER CODE BEGIN CLK 1 */
/* USER CODE END CLK 1 */

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the System Power Supply
  */
  if (HAL_PWREx_ConfigSupply(PWR_EXTERNAL_SOURCE_SUPPLY) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure the main internal regulator output voltage
  */
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
  {
    Error_Handler();
  }

  /* Enable HSI */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSIDiv = RCC_HSI_DIV1;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL1.PLLState = RCC_PLL_NONE;
  RCC_OscInitStruct.PLL2.PLLState = RCC_PLL_NONE;
  RCC_OscInitStruct.PLL3.PLLState = RCC_PLL_NONE;
  RCC_OscInitStruct.PLL4.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Get current CPU/System buses clocks configuration and if necessary switch
 to intermediate HSI clock to ensure target clock can be set
  */
  HAL_RCC_GetClockConfig(&RCC_ClkInitStruct);
  if ((RCC_ClkInitStruct.CPUCLKSource == RCC_CPUCLKSOURCE_IC1) ||
     (RCC_ClkInitStruct.SYSCLKSource == RCC_SYSCLKSOURCE_IC2_IC6_IC11))
  {
    RCC_ClkInitStruct.ClockType = (RCC_CLOCKTYPE_CPUCLK | RCC_CLOCKTYPE_SYSCLK);
    RCC_ClkInitStruct.CPUCLKSource = RCC_CPUCLKSOURCE_HSI;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct) != HAL_OK)
    {
      /* Initialization Error */
      Error_Handler();
    }
  }

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_NONE;
  RCC_OscInitStruct.PLL1.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL1.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL1.PLLM = 4;
  RCC_OscInitStruct.PLL1.PLLN = 75;
  RCC_OscInitStruct.PLL1.PLLFractional = 0;
  RCC_OscInitStruct.PLL1.PLLP1 = 1;
  RCC_OscInitStruct.PLL1.PLLP2 = 1;
  RCC_OscInitStruct.PLL2.PLLState = RCC_PLL_NONE;
  RCC_OscInitStruct.PLL3.PLLState = RCC_PLL_NONE;
  RCC_OscInitStruct.PLL4.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_CPUCLK|RCC_CLOCKTYPE_HCLK
                              |RCC_CLOCKTYPE_SYSCLK|RCC_CLOCKTYPE_PCLK1
                              |RCC_CLOCKTYPE_PCLK2|RCC_CLOCKTYPE_PCLK5
                              |RCC_CLOCKTYPE_PCLK4;
  RCC_ClkInitStruct.CPUCLKSource = RCC_CPUCLKSOURCE_IC1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_IC2_IC6_IC11;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV1;
  RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV1;
  RCC_ClkInitStruct.APB5CLKDivider = RCC_APB5_DIV1;
  RCC_ClkInitStruct.IC1Selection.ClockSelection = RCC_ICCLKSOURCE_PLL1;
  RCC_ClkInitStruct.IC1Selection.ClockDivider = 2;
  RCC_ClkInitStruct.IC2Selection.ClockSelection = RCC_ICCLKSOURCE_PLL1;
  RCC_ClkInitStruct.IC2Selection.ClockDivider = 3;
  RCC_ClkInitStruct.IC6Selection.ClockSelection = RCC_ICCLKSOURCE_PLL1;
  RCC_ClkInitStruct.IC6Selection.ClockDivider = 4;
  RCC_ClkInitStruct.IC11Selection.ClockSelection = RCC_ICCLKSOURCE_PLL1;
  RCC_ClkInitStruct.IC11Selection.ClockDivider = 3;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
/**
  * @brief  TP2 partie 1 : un tour complet de chenillard (rouge -> vert -> bleu).
  */
static void etape_chenillard(void)
{
  log_etat("debut de chenillard");
  for (uint32_t i = 0; i < NB_ETAPES; i++)
  {
    leds_toutes_eteintes();
    BSP_LED_On(chenillard[i].led);
    log_etat(chenillard[i].nom);
    HAL_Delay(CHENILLARD_PERIODE_MS);
  }
}

/**
  * @brief  TP2 partie 2 : une acquisition des capteurs.
  *         Cadencement sur une echeance ABSOLUE (prochaine_lecture_ms) :
  *         la duree de la lecture et des printf ne decale pas la periode,
  *         contrairement a un HAL_Delay() enchaine (derive vue en partie 1).
  *         LED verte = en attente, LED rouge = lecture en cours.
  */
static void etape_capteurs(void)
{
  Mesures_t m;
  uint32_t  t_debut, duree_ms;

  /* Attente de l'echeance (comparaison signee : robuste au debordement de HAL_GetTick) */
  while ((int32_t)(HAL_GetTick() - prochaine_lecture_ms) < 0)
  {
  }
  prochaine_lecture_ms += PERIODE_LECTURE_MS;

  BSP_LED_Off(LED_GREEN);
  BSP_LED_On(LED_RED);                        /* rouge = occupe, lecture en cours */
  log_etat("Lecture Interface Capteurs");

  t_debut = HAL_GetTick();
  capteurs_lire(&m);
  duree_ms = HAL_GetTick() - t_debut;

  /* La lecture ne dure que quelques ms : on maintient le rouge un minimum
     de temps pour que le temoin soit visible a l'oeil nu. */
  while ((HAL_GetTick() - t_debut) < LED_ROUGE_MIN_MS)
  {
  }
  BSP_LED_Off(LED_RED);
  BSP_LED_On(LED_GREEN);                      /* vert = disponible, attente */

  /* Les noms affiches viennent de la carte detectee (IKS01A3 ou IKS4A1) */
  printf("[%s] ", capteurs_nom_role(MESURE_HUMIDITE));
  afficher_valeur("T",  m.temp_hr_c,        "degC", (m.valide & MESURE_HUMIDITE)  != 0U);
  afficher_valeur("HR", m.humidite_rh,      "%RH",  (m.valide & MESURE_HUMIDITE)  != 0U);
  printf("[%s] ", capteurs_nom_role(MESURE_PRESSION));
  afficher_valeur("P",  m.pression_hpa,     "hPa",  (m.valide & MESURE_PRESSION)  != 0U);
  afficher_valeur("T",  m.temp_p_c,         "degC", (m.valide & MESURE_PRESSION)  != 0U);
  printf("[%s] ", capteurs_nom_role(MESURE_TEMP_PREC));
  afficher_valeur("T",  m.temp_precision_c, "degC", (m.valide & MESURE_TEMP_PREC) != 0U);
  printf("| lecture I2C %lu ms\r\n", (unsigned long)duree_ms);
}

/**
  * @brief  TP3 partie 3 : mesure -> features -> reseau de neurones -> classe meteo.
  *         Meme cadencement absolu que la partie 2. LED bleue = inference en cours.
  */
static void etape_ia_meteo(void)
{
  Mesures_t m;
  float brut[METEO_NB_FEATURES], norm[METEO_NB_FEATURES], proba[METEO_NB_CLASSES];

  while ((int32_t)(HAL_GetTick() - prochaine_lecture_ms) < 0)
  {
  }
  prochaine_lecture_ms += PERIODE_LECTURE_MS;

  BSP_LED_Off(LED_GREEN);
  BSP_LED_On(LED_RED);
  capteurs_lire(&m);
  BSP_LED_Off(LED_RED);

  /* Il faut HR + P. La temperature vient du capteur d'humidite : l'humidite
     RELATIVE est mesuree a SA temperature, donc le point de rosee n'est juste
     qu'avec ce couple-la. */
  int ok_hr = (m.valide & MESURE_HUMIDITE) != 0U;
  int ok_p  = (m.valide & MESURE_PRESSION) != 0U;
  float t   = m.temp_hr_c;
  if (!ok_hr || !ok_p)
  {
    log_etat("IA : mesures incompletes (capteur d'humidite + de pression requis)");
    BSP_LED_On(LED_GREEN);
    return;
  }

  /* Le capteur mesure la pression ABSOLUE ; le modele attend la pression niveau mer */
  float p_mer = meteo_pression_niveau_mer(m.pression_hpa, t, METEO_ALTITUDE_M);

  /* Historique : moyenne des mesures de chaque periode (1 h) */
  meteo_somme_t += t; meteo_somme_rh += m.humidite_rh; meteo_somme_p += p_mer; meteo_nb_mesures++;
  if ((HAL_GetTick() - meteo_debut_heure_ms) >= METEO_PERIODE_HISTO_MS)
  {
    float n = (float)meteo_nb_mesures;
    meteo_historique_ajouter(&meteo_hist, meteo_somme_t / n, meteo_somme_rh / n, meteo_somme_p / n);
    meteo_somme_t = meteo_somme_rh = meteo_somme_p = 0.0f; meteo_nb_mesures = 0U;
    meteo_debut_heure_ms += METEO_PERIODE_HISTO_MS;
  }

  BSP_LED_On(LED_BLUE);                       /* bleu = inference IA */
  meteo_calculer_features(&meteo_hist, t, m.humidite_rh, p_mer, brut);
  meteo_normaliser(brut, norm);
  int classe = meteo_mlp_ref_predire(norm, proba);
  BSP_LED_Off(LED_BLUE);
  BSP_LED_On(LED_GREEN);

  log_etat("Prediction IA embarquee");
  afficher_valeur("T", t, "degC", 1);
  afficher_valeur("HR", m.humidite_rh, "%", 1);
  afficher_valeur("P_mer", p_mer, "hPa", 1);
  afficher_valeur("dP3h", brut[METEO_F_DP3], "hPa", 1);
  printf("| historique %u/%u h\r\n", (unsigned)meteo_hist.n, (unsigned)METEO_HISTORY_HOURS);
  printf("   => %s  (", METEO_CLASS_NAMES[classe]);
  for (int i = 0; i < METEO_NB_CLASSES; i++)
  {
    printf("%s %lu%%%s", METEO_CLASS_NAMES[i], (unsigned long)(proba[i] * 100.0f + 0.5f),
           (i < METEO_NB_CLASSES - 1) ? ", " : ")\r\n");
  }
  if ((++nb_predictions % 15U) == 0U)          /* toutes les 30 s */
  {
    afficher_bench();
  }
}

/**
  * @brief  TP3 3.c : temps d'execution moyen du reseau sur la carte.
  *         On enchaine des paquets de METEO_BENCH_N inferences jusqu'a depasser
  *         METEO_BENCH_MIN_MS : la mesure reste precise (< 0,5 %) que l'inference
  *         dure 1 us ou 1 ms, malgre la resolution de 1 ms du tick.
  * @param  config  texte decrivant la configuration testee (affiche dans le log)
  */
static uint32_t mesurer_temps_inference(const char *config)
{
  float x[METEO_NB_FEATURES] = {0}, p[METEO_NB_CLASSES];
  volatile int sink = 0;
  uint32_t n = 0U, dt;
  uint32_t t0 = HAL_GetTick();
  do
  {
    for (uint32_t i = 0; i < METEO_BENCH_N; i++)
    {
      x[0] = (float)(i & 7) * 0.1f;            /* entree variable : evite l'optimisation */
      sink += meteo_mlp_ref_predire(x, p);
    }
    n += METEO_BENCH_N;
    dt = HAL_GetTick() - t0;
  } while (dt < METEO_BENCH_MIN_MS);

  /* temps en nanosecondes par inference, calcul entier sur 64 bits */
  uint32_t ns = (uint32_t)(((uint64_t)dt * 1000000ULL) / n);
  printf("Benchmark IA [%s | reseau %s] : %lu inferences en %lu ms -> %lu.%02lu us / inference (CPU %lu MHz)\r\n",
         config, meteo_mlp_ref_optim(), (unsigned long)n, (unsigned long)dt,
         (unsigned long)(ns / 1000U), (unsigned long)((ns % 1000U) / 10U),
         (unsigned long)(SystemCoreClock / 1000000U));
  (void)sink;
  return ns;
}

/* Rappel periodique du benchmark (le debut du log defile vite dans la console) */
static void afficher_bench(void)
{
  printf("   [bench reseau %s] caches OFF %lu.%02lu us | caches ON %lu.%02lu us | gain x%lu\r\n",
         meteo_mlp_ref_optim(),
         (unsigned long)(bench_ns_off / 1000U), (unsigned long)((bench_ns_off % 1000U) / 10U),
         (unsigned long)(bench_ns_on / 1000U),  (unsigned long)((bench_ns_on % 1000U) / 10U),
         (unsigned long)(bench_ns_on ? (bench_ns_off + bench_ns_on / 2U) / bench_ns_on : 0U));
}

/**
  * @brief  Affiche "NOM=12.34 unite " avec 2 decimales, sans printf flottant
  *         (evite l'option _printf_float qui ajoute plusieurs Ko de code).
  */
static void afficher_valeur(const char *nom, float v, const char *unite, int valide)
{
  if (!valide)
  {
    printf("%s=--- %s  ", nom, unite);
    return;
  }
  int32_t c = (int32_t)(v * 100.0f + ((v >= 0.0f) ? 0.5f : -0.5f));   /* centiemes arrondis */
  uint32_t a = (uint32_t)((c < 0) ? -c : c);
  printf("%s=%s%lu.%02lu %s  ", nom, (c < 0) ? "-" : "",
         (unsigned long)(a / 100U), (unsigned long)(a % 100U), unite);
}

/**
  * @brief  Affiche un etat de fonctionnement sur la console (UART via ST-LINK),
  *         horodate en millisecondes depuis le demarrage : [   3000 ms] <ROUGE>
  */
static void log_etat(const char *etat)
{
  printf("[%7lu ms] <%s>\r\n", (unsigned long)HAL_GetTick(), etat);
}

/**
  * @brief  Eteint les trois LEDs utilisateur.
  */
static void leds_toutes_eteintes(void)
{
  BSP_LED_Off(LED_RED);
  BSP_LED_Off(LED_GREEN);
  BSP_LED_Off(LED_BLUE);
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @param None
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  /* Erreur fatale : LED rouge fixe, tout le reste eteint, et on s'arrete ici */
  __disable_irq();
  BSP_LED_Off(LED_GREEN);
  BSP_LED_Off(LED_BLUE);
  BSP_LED_On(LED_RED);
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
