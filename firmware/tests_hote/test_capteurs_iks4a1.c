/* Test hote de capteurs.c : bus I2C SIMULE d'une X-NUCLEO-IKS4A1
   (SHT40 0x44 avec CRC, LPS22DF 0x5D, STTS22H 0x38 + les autres composants de la carte).
   On verifie : detection, configuration des registres, conversions en unites physiques. */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "capteurs.h"
static uint8_t r_df[256], r_22h[256];
static uint32_t tick=0; static uint8_t sht_cmd=0;
static int ack(uint16_t a8){ uint8_t a=a8>>1; return a==0x44||a==0x5D||a==0x38||a==0x19||a==0x1E||a==0x6A||a==0x6B; }
HAL_StatusTypeDef HAL_I2C_IsDeviceReady(I2C_HandleTypeDef*h,uint16_t a,uint32_t t,uint32_t to){return ack(a)?HAL_OK:HAL_ERROR;}
static uint8_t *regs(uint16_t a8){ switch(a8>>1){case 0x5D:return r_df;case 0x38:return r_22h;default:return 0;} }
HAL_StatusTypeDef HAL_I2C_Mem_Read(I2C_HandleTypeDef*h,uint16_t a,uint16_t reg,uint16_t s,uint8_t*b,uint16_t n,uint32_t to){
  uint8_t*r=regs(a); if(!r) return HAL_ERROR;
  if((a>>1)==0x38 && n>1 && !(r[0x04]&0x08)) { printf("ERREUR: STTS22H lecture multi sans IF_ADD_INC\n"); }
  for(int i=0;i<n;i++) b[i]=r[(reg+i)&0xFF];
  return HAL_OK;}
HAL_StatusTypeDef HAL_I2C_Mem_Write(I2C_HandleTypeDef*h,uint16_t a,uint16_t reg,uint16_t s,uint8_t*b,uint16_t n,uint32_t to){
  uint8_t*r=regs(a); if(!r) return HAL_ERROR;
  for(int i=0;i<n;i++) r[(reg+i)&0xFF]=b[i];
  if((a>>1)==0x5D && (r[0x11]&0x04)) r[0x11]&=~0x04; /* swreset auto-clear */
  return HAL_OK;}
static uint8_t crc8(const uint8_t*d,int n){uint8_t c=0xFF;for(int i=0;i<n;i++){c^=d[i];for(int b=0;b<8;b++)c=(c&0x80)?(uint8_t)((c<<1)^0x31):(uint8_t)(c<<1);}return c;}
HAL_StatusTypeDef HAL_I2C_Master_Transmit(I2C_HandleTypeDef*h,uint16_t a,uint8_t*b,uint16_t n,uint32_t to){ if((a>>1)!=0x44) return HAL_ERROR; sht_cmd=b[0]; return HAL_OK;}
HAL_StatusTypeDef HAL_I2C_Master_Receive(I2C_HandleTypeDef*h,uint16_t a,uint8_t*b,uint16_t n,uint32_t to){
  if((a>>1)!=0x44||n!=6) return HAL_ERROR; uint16_t w1,w2;
  if(sht_cmd==0x89){w1=0x1234;w2=0xABCD;}
  else if(sht_cmd==0xFD){ /* 21.30 degC, 47.5 %RH */ w1=(uint16_t)lround((21.30+45)/175*65535); w2=(uint16_t)lround((47.5+6)/125*65535);}
  else return HAL_ERROR;
  b[0]=w1>>8;b[1]=w1;b[2]=crc8(b,2);b[3]=w2>>8;b[4]=w2;b[5]=crc8(b+3,2); return HAL_OK;}
void HAL_Delay(uint32_t ms){tick+=ms;} uint32_t HAL_GetTick(void){return tick++;}
int main(void){
  I2C_HandleTypeDef h; Mesures_t m;
  { uint8_t t[2]={0xBE,0xEF}; printf("CRC datasheet 0xBEEF -> 0x%02X (attendu 0x92)\n", crc8(t,2)); }
  r_df[0x0F]=0xB4; r_22h[0x01]=0xA0;
  /* LPS22DF : 978.25 hPa -> raw24 = 978.25*4096 ; T = 22.10 degC -> 2210 */
  int32_t p=(int32_t)lround(978.25*4096); r_df[0x28]=p; r_df[0x29]=p>>8; r_df[0x2A]=p>>16;
  int16_t tt=2210; r_df[0x2B]=tt; r_df[0x2C]=tt>>8;
  int16_t t2=2187; r_22h[0x06]=t2; r_22h[0x07]=t2>>8;  /* 21.87 degC */
  capteurs_scan_i2c(&h);
  uint32_t pr=capteurs_init(&h);
  printf("presents=0x%02X carte=%s roles: %s %s %s\n",pr,capteurs_carte(),
    capteurs_nom_role(MESURE_HUMIDITE),capteurs_nom_role(MESURE_PRESSION),capteurs_nom_role(MESURE_TEMP_PREC));
  printf("LPS22DF CTRL1=0x%02X CTRL2=0x%02X CTRL3=0x%02X | STTS22H CTRL=0x%02X\n",r_df[0x10],r_df[0x11],r_df[0x12],r_22h[0x04]);
  uint32_t v=capteurs_lire(&m);
  printf("valide=0x%X HR=%.3f T_hr=%.3f P=%.3f T_p=%.3f T_prec=%.3f\n",v,m.humidite_rh,m.temp_hr_c,m.pression_hpa,m.temp_p_c,m.temp_precision_c);
  int ok = v==7 && fabs(m.humidite_rh-47.5)<0.01 && fabs(m.temp_hr_c-21.3)<0.01 && fabs(m.pression_hpa-978.25)<0.01
        && fabs(m.temp_p_c-22.10)<0.001 && fabs(m.temp_precision_c-21.87)<0.001 && !strcmp(capteurs_carte(),"X-NUCLEO-IKS4A1");
  printf(ok?"TEST IKS4A1 : OK\n":"TEST IKS4A1 : ECHEC\n"); return !ok; }
