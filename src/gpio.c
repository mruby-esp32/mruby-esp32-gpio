#include <mruby.h>
#include <mruby/value.h>

#include "driver/gpio.h"
#include "driver/dac_oneshot.h"
#include "esp_adc/adc_oneshot.h"

// Defined by ESP-IDF:
// GPIO_MODE_DEF_INPUT  (BIT0)
// GPIO_MODE_DEF_OUTPUT (BIT1)
// GPIO_MODE_DEF_OD     (BIT2)
#define GPIO_MODE_DEF_PULLUP    (BIT3)
#define GPIO_MODE_DEF_PULLDOWN  (BIT4)

// Defined by ESP-IDF:
// GPIO_MODE_INPUT
// GPIO_MODE_OUTPUT
#define GPIO_MODE_INPUT_PULLUP           ((GPIO_MODE_DEF_INPUT)|(GPIO_MODE_DEF_PULLUP))
#define GPIO_MODE_INPUT_PULLDOWN         ((GPIO_MODE_DEF_INPUT)|(GPIO_MODE_DEF_PULLDOWN))
#define GPIO_MODE_INPUT_PULLUP_PULLDOWN  ((GPIO_MODE_DEF_INPUT)|(GPIO_MODE_DEF_PULLUP)|(GPIO_MODE_DEF_PULLDOWN))
#define GPIO_MODE_INPUT_OUTPUT           ((GPIO_MODE_DEF_INPUT) |(GPIO_MODE_DEF_OUTPUT))
#define GPIO_MODE_INPUT_OUTPUT_OD        ((GPIO_MODE_DEF_INPUT) |(GPIO_MODE_DEF_OUTPUT)|(GPIO_MODE_DEF_OD))
#define GPIO_MODE_OUTPUT_OD              ((GPIO_MODE_DEF_OUTPUT)|(GPIO_MODE_DEF_OD))

// Pin Mode
static mrb_value
mrb_esp32_gpio_pin_mode(mrb_state *mrb, mrb_value self) {
  mrb_value pin, dir;

  mrb_get_args(mrb, "oo", &pin, &dir);

  if (!mrb_fixnum_p(pin) || !mrb_fixnum_p(dir)) {
    return mrb_nil_value();
  }

  esp_rom_gpio_pad_select_gpio(mrb_fixnum(pin));
  
  // Clear pullup and pulldown bits (BIT3 and BIT4) when setting direction.
  gpio_set_direction(mrb_fixnum(pin), mrb_fixnum(dir) & ~(GPIO_MODE_DEF_PULLUP | GPIO_MODE_DEF_PULLDOWN));

  // Set correct pull mode based on bits 3 and 4.
  uint32_t pull = mrb_fixnum(dir) >> 3;
  switch(pull) {
    case 0: gpio_set_pull_mode(mrb_fixnum(pin), GPIO_FLOATING);        break;
    case 1: gpio_set_pull_mode(mrb_fixnum(pin), GPIO_PULLUP_ONLY);     break;
    case 2: gpio_set_pull_mode(mrb_fixnum(pin), GPIO_PULLDOWN_ONLY);   break;
    case 3: gpio_set_pull_mode(mrb_fixnum(pin), GPIO_PULLUP_PULLDOWN); break;
  }

  return self;
}

// Digital Read
static mrb_value
mrb_esp32_gpio_digital_read(mrb_state *mrb, mrb_value self) {
  mrb_value pin;

  mrb_get_args(mrb, "o", &pin);

  if (!mrb_fixnum_p(pin)) {
    return mrb_nil_value();
  }

  return mrb_fixnum_value(gpio_get_level(mrb_fixnum(pin)));
}

// Digital Write
static mrb_value
mrb_esp32_gpio_digital_write(mrb_state *mrb, mrb_value self) {
  mrb_value pin, level;

  mrb_get_args(mrb, "oo", &pin, &level);

  if (!mrb_fixnum_p(pin) || !mrb_fixnum_p(level)) {
    return mrb_nil_value();
  }

  gpio_set_level(mrb_fixnum(pin), mrb_fixnum(level));

  return self;
}

// Analog Read
static mrb_value
mrb_esp32_gpio_analog_read(mrb_state *mrb, mrb_value self) {
  mrb_value ch;

  mrb_get_args(mrb, "o", &ch);

  if (!mrb_fixnum_p(ch)) {
    return mrb_nil_value();
  }
  
  // Handle
  adc_oneshot_unit_handle_t adc1_handle;
  adc_oneshot_unit_init_cfg_t init_config1 = {
      .unit_id = ADC_UNIT_1,
      .ulp_mode = ADC_ULP_MODE_DISABLE,
  };
  ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));
  
  // Always use maximum resolution and attenuation.
  // Should make this configurable.
  adc_oneshot_chan_cfg_t config = {
      .bitwidth = ADC_BITWIDTH_DEFAULT,
      .atten = ADC_ATTEN_DB_11,
  };
  ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, mrb_fixnum(ch), &config));
  
  // Read and Delete
  int adc_result;
  ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, mrb_fixnum(ch), &adc_result));
  ESP_ERROR_CHECK(adc_oneshot_del_unit(adc1_handle));

  return mrb_fixnum_value(adc_result);
}

// Analog Write (DACs not available on some chips)
#ifdef SOC_DAC_SUPPORTED
static mrb_value
mrb_esp32_gpio_analog_write(mrb_state *mrb, mrb_value self) {
  mrb_value ch, vol;

  mrb_get_args(mrb, "oo", &ch, &vol);

  if (!mrb_fixnum_p(ch) || !mrb_fixnum_p(vol)) {
    return mrb_nil_value();
  }
  
  // Handle
  dac_oneshot_handle_t chan_handle;
  
  // Configuration
  dac_oneshot_config_t chan_cfg = {
      .chan_id = mrb_fixnum(ch),
  };
  ESP_ERROR_CHECK(dac_oneshot_new_channel(&chan_cfg, &chan_handle));
  
  // Write
  ESP_ERROR_CHECK(dac_oneshot_output_voltage(chan_handle, mrb_fixnum(vol)));
  
  return self;
}
#endif

void
mrb_mruby_esp32_gpio_gem_init(mrb_state* mrb) {
  struct RClass *esp32, *gpio, *constants;

  // ESP32 module
  esp32 = mrb_define_module(mrb, "ESP32");

  // ESP32::GPIO
  gpio = mrb_define_module_under(mrb, esp32, "GPIO");
  
  // ESP32::Constants
  constants = mrb_define_module_under(mrb, esp32, "Constants");

  // Ruby-style snake-case methods.
  mrb_define_module_function(mrb, gpio, "pin_mode", mrb_esp32_gpio_pin_mode, MRB_ARGS_REQ(2));
  mrb_define_module_function(mrb, gpio, "digital_write", mrb_esp32_gpio_digital_write, MRB_ARGS_REQ(2));
  mrb_define_module_function(mrb, gpio, "digital_read", mrb_esp32_gpio_digital_read, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, gpio, "analog_read", mrb_esp32_gpio_analog_read, MRB_ARGS_REQ(1));

  // Arduino-style camel-case methods.
  mrb_define_module_function(mrb, gpio, "pinMode", mrb_esp32_gpio_pin_mode, MRB_ARGS_REQ(2));
  mrb_define_module_function(mrb, gpio, "digitalWrite", mrb_esp32_gpio_digital_write, MRB_ARGS_REQ(2));
  mrb_define_module_function(mrb, gpio, "digitalRead", mrb_esp32_gpio_digital_read, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, gpio, "analogRead", mrb_esp32_gpio_analog_read, MRB_ARGS_REQ(1));
  
  // DAC available only on some chips.
  #ifdef SOC_DAC_SUPPORTED
    mrb_define_const(mrb, constants, "SOC_DAC_SUPPORTED", mrb_true_value());
    mrb_define_module_function(mrb, gpio, "analogWrite", mrb_esp32_gpio_analog_write, MRB_ARGS_REQ(2));
    mrb_define_module_function(mrb, gpio, "analog_write", mrb_esp32_gpio_analog_write, MRB_ARGS_REQ(2));
  #else
    mrb_define_const(mrb, constants, "SOC_DAC_SUPPORTED", mrb_false_value());
  #endif
  
  // Pass a C constant through to mruby, defined inside ESP32::Constants.
  #define define_const(SYM) \
  do { \
    mrb_define_const(mrb, constants, #SYM, mrb_fixnum_value(SYM)); \
  } while (0)

  //
  // GPIO numbers available on each variant found here:
  // https://github.com/espressif/esp-idf/blob/67552c31da/components/hal/include/hal/gpio_types.h
  //
  // All chips define GPIO_NUM_MAX and GPIO_NUM_0..GPIO_NUM_20.  
  define_const(GPIO_NUM_MAX);
  define_const(GPIO_NUM_0);
  define_const(GPIO_NUM_1);
  define_const(GPIO_NUM_2);
  define_const(GPIO_NUM_3);
  define_const(GPIO_NUM_4);
  define_const(GPIO_NUM_5);
  define_const(GPIO_NUM_6);
  define_const(GPIO_NUM_7);
  define_const(GPIO_NUM_8);
  define_const(GPIO_NUM_9);
  define_const(GPIO_NUM_10);
  define_const(GPIO_NUM_11);
  define_const(GPIO_NUM_12);
  define_const(GPIO_NUM_13);
  define_const(GPIO_NUM_14);
  define_const(GPIO_NUM_15);
  define_const(GPIO_NUM_16);
  define_const(GPIO_NUM_17);
  define_const(GPIO_NUM_18);
  define_const(GPIO_NUM_19);
  define_const(GPIO_NUM_20);

  // Original, S2, S3, C3, C6 and H2 (all except C2) have 21.
  #if defined(CONFIG_IDF_TARGET_ESP32)  || defined(CONFIG_IDF_TARGET_ESP32S2) || \
      defined(CONFIG_IDF_TARGET_ESP32S3)|| defined(CONFIG_IDF_TARGET_ESP32C3) || \
      defined(CONFIG_IDF_TARGET_ESP32C6)|| defined(CONFIG_IDF_TARGET_ESP32H2)      
    define_const(GPIO_NUM_21);
  #endif
  
  // Original, C6 and H2 have 22,23,25.
  #if defined(CONFIG_IDF_TARGET_ESP32) || defined(CONFIG_IDF_TARGET_ESP32C6) || \
      defined(CONFIG_IDF_TARGET_ESP32H2)
    define_const(GPIO_NUM_22);
    define_const(GPIO_NUM_23);
    define_const(GPIO_NUM_25);
  #endif
    
  // C6 and H2 have 24.
  #if defined(CONFIG_IDF_TARGET_ESP32C6) || defined(CONFIG_IDF_TARGET_ESP32H2)
    define_const(GPIO_NUM_24);
  #endif
    
  // Original, S2, S3, C6 ad H2 have 26,27.
  #if defined(CONFIG_IDF_TARGET_ESP32)   || defined(CONFIG_IDF_TARGET_ESP32S2) || \
      defined(CONFIG_IDF_TARGET_ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32C6) || \
      defined(CONFIG_IDF_TARGET_ESP32H2)
    define_const(GPIO_NUM_26);
    define_const(GPIO_NUM_27);
  #endif
    
  // Original, S2, S3 and C6 have 28..30.
  #if defined(CONFIG_IDF_TARGET_ESP32)   || defined(CONFIG_IDF_TARGET_ESP32S2) || \
      defined(CONFIG_IDF_TARGET_ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32C6)
    define_const(GPIO_NUM_28);
    define_const(GPIO_NUM_29);
    define_const(GPIO_NUM_30);
  #endif
    
  // Original, S2 and S3 have 31..39.
  #if defined(CONFIG_IDF_TARGET_ESP32)   || defined(CONFIG_IDF_TARGET_ESP32S2) || \
      defined(CONFIG_IDF_TARGET_ESP32S3)
    define_const(GPIO_NUM_31);    
    define_const(GPIO_NUM_32);
    define_const(GPIO_NUM_33);
    define_const(GPIO_NUM_34);
    define_const(GPIO_NUM_35);
    define_const(GPIO_NUM_36);
    define_const(GPIO_NUM_37);
    define_const(GPIO_NUM_38);
    define_const(GPIO_NUM_39);
  #endif

  // S2 and S3 have 40..46.
  #if defined(CONFIG_IDF_TARGET_ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32S3)
    define_const(GPIO_NUM_40);    
    define_const(GPIO_NUM_41);
    define_const(GPIO_NUM_42);
    define_const(GPIO_NUM_43);
    define_const(GPIO_NUM_44);
    define_const(GPIO_NUM_45);
    define_const(GPIO_NUM_46);
  #endif

  // S3 alone has 47,48.
  #if defined(CONFIG_IDF_TARGET_ESP32S3)
    define_const(GPIO_NUM_47);
    define_const(GPIO_NUM_48);
  #endif

  //
  // All chips have ADC_CHANNEL_0..ADC_CHANNEL_9 defined, but limit them instead
  // to the channels which are actually connected to GPIOs.
  //
  // All chips connect ADC_CHANNEL_0..ADC_CHANNEL_4 to a GPIO.
  define_const(ADC_CHANNEL_0);
  define_const(ADC_CHANNEL_1);
  define_const(ADC_CHANNEL_2);
  define_const(ADC_CHANNEL_3);
  define_const(ADC_CHANNEL_4);

  // Original, S2, S3 and C6 have 5,6.
  #if defined(CONFIG_IDF_TARGET_ESP32)   || defined(CONFIG_IDF_TARGET_ESP32S2) || \
      defined(CONFIG_IDF_TARGET_ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32C6)
    define_const(ADC_CHANNEL_5);
    define_const(ADC_CHANNEL_6);
  #endif

  // Original, S2 and S3 have 7,8,9.
  // Note: Original ESP32 has 8,9 only on ADC2 which isn't implemented yet.
  #if defined(CONFIG_IDF_TARGET_ESP32)   || defined(CONFIG_IDF_TARGET_ESP32S2) || \
      defined(CONFIG_IDF_TARGET_ESP32S3)
    define_const(ADC_CHANNEL_7);
    define_const(ADC_CHANNEL_8);
    define_const(ADC_CHANNEL_9);
  #endif

  // Original and S2 have DACs.
  #ifdef SOC_DAC_SUPPORTED
    define_const(DAC_CHAN_0);
    define_const(DAC_CHAN_1);
    // Old versions of above. Deprecated.
    define_const(DAC_CHANNEL_1);
    define_const(DAC_CHANNEL_2);
  #endif

  mrb_define_const(mrb, constants, "LOW", mrb_fixnum_value(0));
  mrb_define_const(mrb, constants, "HIGH", mrb_fixnum_value(1));

  define_const(GPIO_MODE_INPUT);
  define_const(GPIO_MODE_OUTPUT);
  define_const(GPIO_MODE_INPUT_PULLUP);
  define_const(GPIO_MODE_INPUT_PULLDOWN);
  define_const(GPIO_MODE_INPUT_PULLUP_PULLDOWN);
  define_const(GPIO_MODE_INPUT_OUTPUT);
  define_const(GPIO_MODE_INPUT_OUTPUT_OD);
  define_const(GPIO_MODE_OUTPUT_OD);
}

void
mrb_mruby_esp32_gpio_gem_final(mrb_state* mrb)
{
}
