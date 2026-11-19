/* ETRS923 : optimise meme en Debug (comparaison equitable avec le CPU, voir docs/TP3_NPU.md) */
#pragma GCC optimize ("O3")
/**
  ******************************************************************************
  * @file    npu_sw_ops.c
  * @brief   Les 3 operations du reseau qui restent sur le CPU (epochs "SW") :
  *          quantification de l'entree, softmax, dequantification de la sortie.
  *
  *  ST les fournit dans ll_sw_integer.c, qui depend de la bibliotheque precompilee
  *  NetworkRuntime (1,2 Mo) et d'une centaine d'en-tetes. Pour 3 operations sur
  *  14 et 3 valeurs, on les ecrit ici en C simple, avec les memes formules que
  *  TFLite (arrondi au plus proche, saturation int8). Le softmax est calcule en
  *  flottant puis requantifie : l'ecart avec le softmax entier de TFLite est
  *  au plus d'un pas de quantification (1/256), sans effet sur la classe choisie.
  * @author  Aris Trache - ETRS923, USMB Master ESET
  ******************************************************************************
  */
#include "ll_sw.h"
#include <math.h>
#include <stdint.h>

static float lire_echelle(const Tensor_info *t)
{
  return *(const float *)t->mem.start_offset;
}

static int32_t lire_zero(const Tensor_info *t)
{
  return (int32_t)(*(const int8_t *)t->mem.start_offset);
}

static int8_t quantifier(float x, float echelle, int32_t zero)
{
  int32_t q = (int32_t)lroundf(x / echelle) + zero;
  if (q > 127)
  {
    q = 127;
  }
  if (q < -128)
  {
    q = -128;
  }
  return (int8_t)q;
}

void ll_sw_forward_quantizelinear(void *info)
{
  const Quantizelinear_sw_info *s = (const Quantizelinear_sw_info *)info;
  const float *x = (const float *)s->general.input.mem.start_offset;
  int8_t *y = (int8_t *)s->general.output.mem.start_offset;
  float e = lire_echelle(&s->os);
  int32_t z = lire_zero(&s->ozp);
  for (uint32_t i = 0; i < s->general.input.dim.num_elem; i++)
  {
    y[i] = quantifier(x[i], e, z);
  }
}

void ll_sw_forward_dequantizelinear(void *info)
{
  const Dequantizelinear_sw_info *s = (const Dequantizelinear_sw_info *)info;
  const int8_t *x = (const int8_t *)s->general.input.mem.start_offset;
  float *y = (float *)s->general.output.mem.start_offset;
  float e = lire_echelle(&s->is);
  int32_t z = lire_zero(&s->izp);
  for (uint32_t i = 0; i < s->general.input.dim.num_elem; i++)
  {
    y[i] = (float)((int32_t)x[i] - z) * e;
  }
}

void ll_sw_forward_softmax_integer(void *info)
{
  const Softmax_integer_sw_info *s = (const Softmax_integer_sw_info *)info;
  const int8_t *x = (const int8_t *)s->general.input.mem.start_offset;
  int8_t *y = (int8_t *)s->general.output.mem.start_offset;
  uint32_t n = s->general.input.dim.num_elem;
  float ei = lire_echelle(&s->is), eo = lire_echelle(&s->os);
  int32_t zi = lire_zero(&s->izp), zo = lire_zero(&s->ozp);
  float v[8], m, somme = 0.0f;

  if (n > 8U)
  {
    n = 8U;                            /* notre reseau : 3 classes */
  }
  m = (float)((int32_t)x[0] - zi) * ei;
  for (uint32_t i = 0; i < n; i++)
  {
    v[i] = (float)((int32_t)x[i] - zi) * ei;
    if (v[i] > m)
    {
      m = v[i];
    }
  }
  for (uint32_t i = 0; i < n; i++)
  {
    v[i] = expf(v[i] - m);
    somme += v[i];
  }
  for (uint32_t i = 0; i < n; i++)
  {
    y[i] = quantifier(v[i] / somme, eo, zo);
  }
}
