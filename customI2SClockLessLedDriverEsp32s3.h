/*
  ATTENTION - version modifiée du I2SclockLessLedDriveresp32s3.h  (https://github.com/hpwit/I2SClockLessLedDriveresp32s3)
  Pour permettre le choix au runtime du type de led et du schéma de couleurs(GRB,RGB etc.)
*/


#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "soc/soc_memory_types.h"
#include "esp_attr.h"
#include "esp_check.h"
#include "esp_intr_alloc.h"
#include "esp_heap_caps.h"
#include "esp_pm.h"
//#include "esp_lcd_panel_io_interface.h"
//#include "esp_lcd_panel_io.h"
#include "esp_rom_gpio.h"
#include "soc/soc_caps.h"
#include "soc/rtc.h" // for `rtc_clk_xtal_freq_get()`
#include "soc/soc_memory_types.h"
#include "hal/dma_types.h"
#include "hal/gpio_hal.h"
#include "esp_private/gdma.h"
#include "driver/gpio.h"
//#include "esp_private/periph_ctrl.h"

//#include "esp_lcd_common.h"
#include "soc/lcd_periph.h"
#include "hal/lcd_ll.h"
#include "hal/lcd_hal.h"

#include "esp_log.h"
#include "soc/gdma_reg.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_interface.h"
#include "esp_lcd_panel_io_interface.h"
#include "esp_lcd_panel_ops.h"
#include "esp_timer.h"

/*
#ifndef NUMSTRIPS
#define NUMSTRIPS 16
#endif
*/

#ifndef SNAKEPATTERN
#define SNAKEPATTERN 1
#endif

#ifndef ALTERNATEPATTERN
#define ALTERNATEPATTERN 1
#endif

#define I2S_DEVICE 0

#define AA (0x00AA00AAL)
#define CC (0x0000CCCCL)
#define FF (0xF0F0F0F0L)
#define FF2 (0x0F0F0F0FL)

#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define __OFFSET  (24*3*2*2*2)
#ifndef HARDWARESPRITES
#define HARDWARESPRITES 0
#endif

#if HARDWARESPRITES == 1
#include "hardwareSprite.h"
#endif

struct ColorOrder {
  int _p_r ;
  int _p_g ;
  int _p_b ;
  int _nb_components;
};

const ColorOrder colorOrders[8] = {
  {1,0,2,4},  // GRBW
  {0,1,2,4},  // RGBW
  {0,2,1,3},  // RGB
  {1,0,2,3},  // GRB
  {0,2,1,3},  // RBG
  {2,0,1,3},  // GBR
  {2,1,0,3},  // BGR
  {1,2,0,3},  // BRG
};

enum colorarrangment
{
    ORDER_GRBW,
    ORDER_RGBW,
    ORDER_RGB,
    ORDER_GRB,
    ORDER_RBG,
    ORDER_GBR,
    ORDER_BGR,
    ORDER_BRG,
};


#ifdef USE_PIXELSLIB
//#include "pixelslib.h"
#else
//#include "___pixeltypes.h"
#endif


#define LCD_DRIVER_PSRAM_DATA_ALIGNMENT 64
#define CLOCKLESS_PIXEL_CLOCK_HZ  (24 * 100 * 1000)



typedef union
{
    uint8_t bytes[16];
    uint32_t shorts[8];
    uint32_t raw[2];
} Lines;


enum displayMode
{
    NO_WAIT,
    WAIT,
    LOOP,
    LOOP_INTERUPT,
};

bool DRIVER_READY = true;


typedef struct led_driver_t led_driver_t;

struct led_driver_t {
  size_t (*init)();
  void (*update)(uint8_t* colors, size_t len);
};
volatile xSemaphoreHandle I2SClocklessLedDriverS3_sem = NULL;
volatile bool isDisplaying=false;
volatile bool iswaiting =false;
static bool IRAM_ATTR flush_ready(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx) {
   // printf("we're here");
  DRIVER_READY = true;
  isDisplaying=false;
  if(iswaiting)
  {
    portBASE_TYPE HPTaskAwoken = 0;
    iswaiting=false;
    xSemaphoreGiveFromISR(I2SClocklessLedDriverS3_sem, &HPTaskAwoken);
    if (HPTaskAwoken == pdTRUE)
        portYIELD_FROM_ISR(HPTaskAwoken);
  }
  return false;
}
/* ====================================== MODELE DU CODE D'ORIGINE BASE SUR DES DEFINE ====================================
static void IRAM_ATTR transpose16x1_noinline2(unsigned char *A, uint16_t *B)
{

    uint32_t x, y, x1, y1, t;

    y = *(unsigned int *)(A);
#if NUMSTRIPS > 4
    x = *(unsigned int *)(A + 4);
#else
    x = 0;
#endif

#if NUMSTRIPS > 8
    y1 = *(unsigned int *)(A + 8);
#else
    y1 = 0;
#endif
#if NUMSTRIPS > 12
    x1 = *(unsigned int *)(A + 12);
#else
    x1 = 0;
#endif

    // pre-transform x
#if NUMSTRIPS > 4
    t = (x ^ (x >> 7)) & AA;
    x = x ^ t ^ (t << 7);
    t = (x ^ (x >> 14)) & CC;
    x = x ^ t ^ (t << 14);
#endif
#if NUMSTRIPS > 12
    t = (x1 ^ (x1 >> 7)) & AA;
    x1 = x1 ^ t ^ (t << 7);
    t = (x1 ^ (x1 >> 14)) & CC;
    x1 = x1 ^ t ^ (t << 14);
#endif
    // pre-transform y
    t = (y ^ (y >> 7)) & AA;
    y = y ^ t ^ (t << 7);
    t = (y ^ (y >> 14)) & CC;
    y = y ^ t ^ (t << 14);
#if NUMSTRIPS > 8
    t = (y1 ^ (y1 >> 7)) & AA;
    y1 = y1 ^ t ^ (t << 7);
    t = (y1 ^ (y1 >> 14)) & CC;
    y1 = y1 ^ t ^ (t << 14);
#endif
    // final transform
    t = (x & FF) | ((y >> 4) & FF2);
    y = ((x << 4) & FF) | (y & FF2);
    x = t;

    t = (x1 & FF) | ((y1 >> 4) & FF2);
    y1 = ((x1 << 4) & FF) | (y1 & FF2);
    x1 = t;

    *((uint16_t *)(B)) = (uint16_t)(((x & 0xff000000) >> 8 | ((x1 & 0xff000000))) >> 16);
    *((uint16_t *)(B + 3)) = (uint16_t)(((x & 0xff0000) >> 16 | ((x1 & 0xff0000) >> 8)));
    *((uint16_t *)(B + 6)) = (uint16_t)(((x & 0xff00) | ((x1 & 0xff00) << 8)) >> 8);
    *((uint16_t *)(B + 9)) = (uint16_t)((x & 0xff) | ((x1 & 0xff) << 8));
    *((uint16_t *)(B + 12)) = (uint16_t)(((y & 0xff000000) >> 8 | ((y1 & 0xff000000))) >> 16);
    *((uint16_t *)(B + 15)) = (uint16_t)(((y & 0xff0000) | ((y1 & 0xff0000) << 8)) >> 16);
    *((uint16_t *)(B + 18)) = (uint16_t)(((y & 0xff00) | ((y1 & 0xff00) << 8)) >> 8);
    *((uint16_t *)(B + 21)) = (uint16_t)((y & 0xff) | ((y1 & 0xff) << 8));
}
 ==========================================================================================================
*/

static void IRAM_ATTR transpose16x1_noinline2_1to4strips(unsigned char *A, uint16_t *B)
{

    uint32_t x, y, x1, y1, t;

    y = *(unsigned int *)(A);
    x = 0;
    y1 = 0;
    x1 = 0;

    // pre-transform x
    // pre-transform y
    t = (y ^ (y >> 7)) & AA;
    y = y ^ t ^ (t << 7);
    t = (y ^ (y >> 14)) & CC;
    y = y ^ t ^ (t << 14);
    // final transform
    t = (x & FF) | ((y >> 4) & FF2);
    y = ((x << 4) & FF) | (y & FF2);
    x = t;

    t = (x1 & FF) | ((y1 >> 4) & FF2);
    y1 = ((x1 << 4) & FF) | (y1 & FF2);
    x1 = t;

    *((uint16_t *)(B)) = (uint16_t)(((x & 0xff000000) >> 8 | ((x1 & 0xff000000))) >> 16);
    *((uint16_t *)(B + 3)) = (uint16_t)(((x & 0xff0000) >> 16 | ((x1 & 0xff0000) >> 8)));
    *((uint16_t *)(B + 6)) = (uint16_t)(((x & 0xff00) | ((x1 & 0xff00) << 8)) >> 8);
    *((uint16_t *)(B + 9)) = (uint16_t)((x & 0xff) | ((x1 & 0xff) << 8));
    *((uint16_t *)(B + 12)) = (uint16_t)(((y & 0xff000000) >> 8 | ((y1 & 0xff000000))) >> 16);
    *((uint16_t *)(B + 15)) = (uint16_t)(((y & 0xff0000) | ((y1 & 0xff0000) << 8)) >> 16);
    *((uint16_t *)(B + 18)) = (uint16_t)(((y & 0xff00) | ((y1 & 0xff00) << 8)) >> 8);
    *((uint16_t *)(B + 21)) = (uint16_t)((y & 0xff) | ((y1 & 0xff) << 8));
}

static void IRAM_ATTR transpose16x1_noinline2_5to8strips(unsigned char *A, uint16_t *B)
{

    uint32_t x, y, x1, y1, t;

    y = *(unsigned int *)(A);
    x = *(unsigned int *)(A + 4);
    y1 = 0;
    x1 = 0;

    // pre-transform x
    t = (x ^ (x >> 7)) & AA;
    x = x ^ t ^ (t << 7);
    t = (x ^ (x >> 14)) & CC;
    x = x ^ t ^ (t << 14);
    // pre-transform y
    t = (y ^ (y >> 7)) & AA;
    y = y ^ t ^ (t << 7);
    t = (y ^ (y >> 14)) & CC;
    y = y ^ t ^ (t << 14);
    // final transform
    t = (x & FF) | ((y >> 4) & FF2);
    y = ((x << 4) & FF) | (y & FF2);
    x = t;

    t = (x1 & FF) | ((y1 >> 4) & FF2);
    y1 = ((x1 << 4) & FF) | (y1 & FF2);
    x1 = t;

    *((uint16_t *)(B)) = (uint16_t)(((x & 0xff000000) >> 8 | ((x1 & 0xff000000))) >> 16);
    *((uint16_t *)(B + 3)) = (uint16_t)(((x & 0xff0000) >> 16 | ((x1 & 0xff0000) >> 8)));
    *((uint16_t *)(B + 6)) = (uint16_t)(((x & 0xff00) | ((x1 & 0xff00) << 8)) >> 8);
    *((uint16_t *)(B + 9)) = (uint16_t)((x & 0xff) | ((x1 & 0xff) << 8));
    *((uint16_t *)(B + 12)) = (uint16_t)(((y & 0xff000000) >> 8 | ((y1 & 0xff000000))) >> 16);
    *((uint16_t *)(B + 15)) = (uint16_t)(((y & 0xff0000) | ((y1 & 0xff0000) << 8)) >> 16);
    *((uint16_t *)(B + 18)) = (uint16_t)(((y & 0xff00) | ((y1 & 0xff00) << 8)) >> 8);
    *((uint16_t *)(B + 21)) = (uint16_t)((y & 0xff) | ((y1 & 0xff) << 8));
}

static void IRAM_ATTR transpose16x1_noinline2_9to12strips(unsigned char *A, uint16_t *B)
{

    uint32_t x, y, x1, y1, t;

    y = *(unsigned int *)(A);
    x = *(unsigned int *)(A + 4);
    y1 = *(unsigned int *)(A + 8);
    x1 = 0;

    // pre-transform x
    t = (x ^ (x >> 7)) & AA;
    x = x ^ t ^ (t << 7);
    t = (x ^ (x >> 14)) & CC;
    x = x ^ t ^ (t << 14);
    // pre-transform y
    t = (y ^ (y >> 7)) & AA;
    y = y ^ t ^ (t << 7);
    t = (y ^ (y >> 14)) & CC;
    y = y ^ t ^ (t << 14);
    t = (y1 ^ (y1 >> 7)) & AA;
    y1 = y1 ^ t ^ (t << 7);
    t = (y1 ^ (y1 >> 14)) & CC;
    y1 = y1 ^ t ^ (t << 14);
    // final transform
    t = (x & FF) | ((y >> 4) & FF2);
    y = ((x << 4) & FF) | (y & FF2);
    x = t;

    t = (x1 & FF) | ((y1 >> 4) & FF2);
    y1 = ((x1 << 4) & FF) | (y1 & FF2);
    x1 = t;

    *((uint16_t *)(B)) = (uint16_t)(((x & 0xff000000) >> 8 | ((x1 & 0xff000000))) >> 16);
    *((uint16_t *)(B + 3)) = (uint16_t)(((x & 0xff0000) >> 16 | ((x1 & 0xff0000) >> 8)));
    *((uint16_t *)(B + 6)) = (uint16_t)(((x & 0xff00) | ((x1 & 0xff00) << 8)) >> 8);
    *((uint16_t *)(B + 9)) = (uint16_t)((x & 0xff) | ((x1 & 0xff) << 8));
    *((uint16_t *)(B + 12)) = (uint16_t)(((y & 0xff000000) >> 8 | ((y1 & 0xff000000))) >> 16);
    *((uint16_t *)(B + 15)) = (uint16_t)(((y & 0xff0000) | ((y1 & 0xff0000) << 8)) >> 16);
    *((uint16_t *)(B + 18)) = (uint16_t)(((y & 0xff00) | ((y1 & 0xff00) << 8)) >> 8);
    *((uint16_t *)(B + 21)) = (uint16_t)((y & 0xff) | ((y1 & 0xff) << 8));
}

static void IRAM_ATTR transpose16x1_noinline2_13to16strips(unsigned char *A, uint16_t *B)
{

    uint32_t x, y, x1, y1, t;

    y = *(unsigned int *)(A);
    x = *(unsigned int *)(A + 4);
    y1 = *(unsigned int *)(A + 8);
    x1 = *(unsigned int *)(A + 12);

    // pre-transform x
    t = (x ^ (x >> 7)) & AA;
    x = x ^ t ^ (t << 7);
    t = (x ^ (x >> 14)) & CC;
    x = x ^ t ^ (t << 14);
    t = (x1 ^ (x1 >> 7)) & AA;
    x1 = x1 ^ t ^ (t << 7);
    t = (x1 ^ (x1 >> 14)) & CC;
    x1 = x1 ^ t ^ (t << 14);
    // pre-transform y
    t = (y ^ (y >> 7)) & AA;
    y = y ^ t ^ (t << 7);
    t = (y ^ (y >> 14)) & CC;
    y = y ^ t ^ (t << 14);
    t = (y1 ^ (y1 >> 7)) & AA;
    y1 = y1 ^ t ^ (t << 7);
    t = (y1 ^ (y1 >> 14)) & CC;
    y1 = y1 ^ t ^ (t << 14);
    // final transform
    t = (x & FF) | ((y >> 4) & FF2);
    y = ((x << 4) & FF) | (y & FF2);
    x = t;

    t = (x1 & FF) | ((y1 >> 4) & FF2);
    y1 = ((x1 << 4) & FF) | (y1 & FF2);
    x1 = t;

    *((uint16_t *)(B)) = (uint16_t)(((x & 0xff000000) >> 8 | ((x1 & 0xff000000))) >> 16);
    *((uint16_t *)(B + 3)) = (uint16_t)(((x & 0xff0000) >> 16 | ((x1 & 0xff0000) >> 8)));
    *((uint16_t *)(B + 6)) = (uint16_t)(((x & 0xff00) | ((x1 & 0xff00) << 8)) >> 8);
    *((uint16_t *)(B + 9)) = (uint16_t)((x & 0xff) | ((x1 & 0xff) << 8));
    *((uint16_t *)(B + 12)) = (uint16_t)(((y & 0xff000000) >> 8 | ((y1 & 0xff000000))) >> 16);
    *((uint16_t *)(B + 15)) = (uint16_t)(((y & 0xff0000) | ((y1 & 0xff0000) << 8)) >> 16);
    *((uint16_t *)(B + 18)) = (uint16_t)(((y & 0xff00) | ((y1 & 0xff00) << 8)) >> 8);
    *((uint16_t *)(B + 21)) = (uint16_t)((y & 0xff) | ((y1 & 0xff) << 8));
}



esp_lcd_panel_io_handle_t led_io_handle = NULL;

#ifdef __cplusplus
extern "C"
{
#endif
void _initled( uint8_t * leds, int * pins, int numstrip,int NUM_LED_PER_STRIP, ColorOrder _colorOrder)
{
    //esp_lcd_panel_io_handle_t init_lcd_driver(unsigned int CLOCKLESS_PIXEL_CLOCK_HZ, size_t _nb_components) {
    esp_lcd_i80_bus_handle_t i80_bus = NULL;

    esp_lcd_i80_bus_config_t bus_config;
          
    bus_config.clk_src = LCD_CLK_SRC_PLL160M;
    bus_config.dc_gpio_num = 0;
    bus_config.wr_gpio_num = 0;
    //bus_config.data_gpio_nums = (int*)malloc(16*sizeof(int));
    for (int i=0;i<numstrip;i++)
    {
        bus_config.data_gpio_nums[i]=pins[i];
    }
    if(numstrip<16)
    {
      for (int i=numstrip;i<16;i++)
      {
          bus_config.data_gpio_nums[i]=0;
      }
    }
    bus_config.bus_width = 16;
    bus_config.max_transfer_bytes = _colorOrder._nb_components*NUM_LED_PER_STRIP*8*3*2+__OFFSET;
    bus_config.psram_trans_align = LCD_DRIVER_PSRAM_DATA_ALIGNMENT;
    bus_config.sram_trans_align = 4;
    

    ESP_ERROR_CHECK(esp_lcd_new_i80_bus(&bus_config, &i80_bus));

    esp_lcd_panel_io_i80_config_t io_config ;
    
    io_config.cs_gpio_num = -1;
    io_config.pclk_hz = CLOCKLESS_PIXEL_CLOCK_HZ;
    io_config.trans_queue_depth = 1;
    io_config.dc_levels = {
    .dc_idle_level = 0,
    .dc_cmd_level = 0,
    .dc_dummy_level = 0,
    .dc_data_level = 1,
    };
    //.on_color_trans_done = flush_ready,
    // .user_ctx = nullptr,
    io_config.lcd_cmd_bits = 0;
    io_config.lcd_param_bits = 0;
                
    io_config.on_color_trans_done = flush_ready;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i80(i80_bus, &io_config, &led_io_handle));

    // reclaim GPIO0 back from the LCD peripheral. esp_lcd_new_i80_bus requires
    // wr and dc to be valid gpio and we used 0, however we don't need those
    // signals for our LED stuff so we can reclaim it here.
    esp_rom_gpio_connect_out_signal(0, SIG_GPIO_OUT_IDX, false, true);

}
#ifdef __cplusplus
}
#endif
class I2SClocklessLedDriveresp32S3
{

    public:
    uint16_t *buffers[2];
    uint16_t * led_output = NULL;
    uint16_t * led_output2 = NULL;
    uint8_t * ledsbuff= NULL;
    int num_leds_per_strip;
    int _numstrips;
    int currentframe;
     
    enum colorarrangment _colorArrangement = ORDER_GRB;
    ColorOrder _colorOrder = colorOrders[ORDER_GRB];

    uint8_t __green_map[256];
    uint8_t __blue_map[256];
    uint8_t __red_map[256];
    uint8_t __white_map[256];
    uint8_t _brightness;
    float _gammar, _gammab, _gammag, _gammaw;
   
    void (* _transposeFunc) (unsigned char *A, uint16_t *B) ;

  void setBrightness(int brightness)
  {
      _brightness = brightness;
      float tmp;
      for (int i = 0; i < 256; i++)
      {
          tmp = powf((float)i / 255, 1 / _gammag);
          __green_map[i] = (uint8_t)(tmp * brightness);
          tmp = powf((float)i / 255, 1 / _gammab);
          __blue_map[i] = (uint8_t)(tmp * brightness);
          tmp = powf((float)i / 255, 1 / _gammar);
          __red_map[i] = (uint8_t)(tmp * brightness);
          tmp = powf((float)i / 255, 1 / _gammaw);
          __white_map[i] = (uint8_t)(tmp * brightness);
      }
  }

  void setGamma(float gammar, float gammab, float gammag, float gammaw)
  {
      _gammag = gammag;
      _gammar = gammar;
      _gammaw = gammaw;
      _gammab = gammab;
      setBrightness(_brightness);
  }

  void setGamma(float gammar, float gammab, float gammag)
  {
      _gammag = gammag;
      _gammar = gammar;
      _gammab = gammab;
      setBrightness(_brightness);
  }

  void initled( uint8_t * leds, int * pins, int numstrip,int NUM_LED_PER_STRIP,enum colorarrangment colorArrangement)
  {   
    _colorArrangement = colorArrangement;
    _colorOrder = colorOrders[colorArrangement];
    if (numstrip  <= 4 ) {
      _transposeFunc = &transpose16x1_noinline2_1to4strips;
    } else if (numstrip <= 8 ) {
      _transposeFunc = &transpose16x1_noinline2_5to8strips;
    } else if (numstrip <= 12 ) {
      _transposeFunc = &transpose16x1_noinline2_9to12strips;
    } else {
      _transposeFunc = &transpose16x1_noinline2_13to16strips;
    }

    currentframe=0;
    _gammab = 1;
    _gammar = 1;
    _gammag = 1;
    _gammaw = 1;
    setBrightness(255);
    if (I2SClocklessLedDriverS3_sem == NULL)
    {
    I2SClocklessLedDriverS3_sem = xSemaphoreCreateBinary();
    }
    //esp_lcd_panel_io_handle_t init_lcd_driver(unsigned int CLOCKLESS_PIXEL_CLOCK_HZ, size_t _nb_components) {
    led_output= (uint16_t*)heap_caps_aligned_alloc(LCD_DRIVER_PSRAM_DATA_ALIGNMENT, 8 * _colorOrder._nb_components * NUM_LED_PER_STRIP * 3 * 2 + __OFFSET, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
      memset(led_output,0,8*_colorOrder._nb_components * NUM_LED_PER_STRIP * 3 * 2 + __OFFSET);
      //led_output+=__OFFSET/2;
      led_output2= (uint16_t*)heap_caps_aligned_alloc(LCD_DRIVER_PSRAM_DATA_ALIGNMENT, 8 * _colorOrder._nb_components * NUM_LED_PER_STRIP * 3 * 2 + __OFFSET, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
      memset(led_output2,0,8*_colorOrder._nb_components * NUM_LED_PER_STRIP * 3 * 2 + __OFFSET);
      //led_output2+=__OFFSET/2;
    for (int i = 0; i < NUM_LED_PER_STRIP * _colorOrder._nb_components * 8 ; i++)
        {
            led_output[3 * i+1] = 0xFFFF; //the +1 because it's like the first value doesnt get pushed do not ask me why for now   
            led_output2[3 * i+1] = 0xFFFF;
            buffers[0]=led_output;
            buffers[1]=led_output2;
        }
    ledsbuff = leds;
    _numstrips=numstrip;
    num_leds_per_strip=NUM_LED_PER_STRIP;
    _initled(leds,  pins, numstrip, NUM_LED_PER_STRIP, _colorOrder);
    
  }

  void transposeAll(uint16_t * ledoutput)
  {
    
      uint16_t ledToDisplay = 0;
      Lines secondPixel[_colorOrder._nb_components];
      uint16_t *buff = ledoutput+2; //+1 pour le premier empty +1 pour le 1 systématique
        uint16_t jump = num_leds_per_strip *_colorOrder._nb_components;
      for (int j = 0; j < num_leds_per_strip; j++)
      {
          uint8_t *poli = ledsbuff + ledToDisplay * _colorOrder._nb_components;
          for (int i = 0; i < _numstrips; i++)
          {

              secondPixel[_colorOrder._p_g].bytes[i] =  __green_map[*(poli + 1)];
              secondPixel[_colorOrder._p_r].bytes[i] =  __red_map[*(poli + 0)];
              secondPixel[_colorOrder._p_b].bytes[i] = __blue_map[*(poli + 2)];
              if (_colorOrder._nb_components > 3)
                  secondPixel[3].bytes[i] = __white_map[*(poli + 3)];
              //#endif
              poli += jump;
          }
          ledToDisplay++;
          _transposeFunc(secondPixel[0].bytes, buff);
          buff += 24;
          _transposeFunc(secondPixel[1].bytes, buff);
          buff += 24;
          _transposeFunc(secondPixel[2].bytes, buff);
          buff += 24;
          if (_colorOrder._nb_components > 3)
          {
              _transposeFunc(secondPixel[3].bytes, buff);
              buff += 24;
          }
      }
  }

  void show()
  {
      transposeAll(buffers[currentframe]);
      if(isDisplaying)
      {
          iswaiting=true;
          if (I2SClocklessLedDriverS3_sem==NULL)
              I2SClocklessLedDriverS3_sem=xSemaphoreCreateBinary();
          xSemaphoreTake(I2SClocklessLedDriverS3_sem, portMAX_DELAY);
      }
      isDisplaying=true;
      led_io_handle->tx_color(led_io_handle, 0x2C, buffers[currentframe], _colorOrder._nb_components*num_leds_per_strip*8*3*2+__OFFSET);
      currentframe=(currentframe+1)%2;
  }
    
   
};

 
