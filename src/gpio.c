#include <mruby.h>
#include <mruby/value.h>

#include "driver/gpio.h"
#include "driver/dac.h"
#include "driver/adc.h"

#define GPIO_MODE_DEF_PULLUP (BIT3)
#define GPIO_MODE_DEF_PULLDOWN (BIT3)
#define GPIO_MODE_INPUT_PULLUP ((GPIO_MODE_INPUT)|(GPIO_MODE_DEF_PULLUP))
#define GPIO_MODE_INPUT_PULLDOWN ((GPIO_MODE_INPUT)|(GPIO_MODE_DEF_PULLDOWN))
#define GPIO_MODE_OUTPUT (GPIO_MODE_DEF_OUTPUT) 
#define GPIO_MODE_OUTPUT_OD ((GPIO_MODE_DEF_OUTPUT)|(GPIO_MODE_DEF_OD))
#define GPIO_MODE_INPUT_OUTPUT_OD ((GPIO_MODE_DEF_INPUT)|(GPIO_MODE_DEF_OUTPUT)|(GPIO_MODE_DEF_OD))
#define GPIO_MODE_INPUT_OUTPUT ((GPIO_MODE_DEF_INPUT)|(GPIO_MODE_DEF_OUTPUT))

static mrb_value
mrb_esp32_gpio_pin_mode(mrb_state *mrb, mrb_value self) {
  mrb_value pin, dir;

  mrb_get_args(mrb, "oo", &pin, &dir);

  if (!mrb_fixnum_p(pin) || !mrb_fixnum_p(dir)) {
    return mrb_nil_value();
  }

  gpio_pad_select_gpio(mrb_fixnum(pin));
  gpio_set_direction(mrb_fixnum(pin), mrb_fixnum(dir) & ~GPIO_MODE_DEF_PULLUP);

  if (mrb_fixnum(dir) & GPIO_MODE_DEF_PULLUP) {
    gpio_set_pull_mode(mrb_fixnum(pin), GPIO_PULLUP_ONLY);
  }

  return self;
}

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

static mrb_value
mrb_esp32_gpio_analog_write(mrb_state *mrb, mrb_value self) {
  mrb_value ch, vol;

  mrb_get_args(mrb, "oo", &ch, &vol);

  if (!mrb_fixnum_p(ch) || !mrb_fixnum_p(vol)) {
    return mrb_nil_value();
  }

  dac_output_enable(mrb_fixnum(ch));

  dac_output_voltage(mrb_fixnum(ch), mrb_fixnum(vol));

  return self;
}

static mrb_value
mrb_esp32_gpio_digital_read(mrb_state *mrb, mrb_value self) {
  mrb_value pin;

  mrb_get_args(mrb, "o", &pin);

  if (!mrb_fixnum_p(pin)) {
    return mrb_nil_value();
  }

  return mrb_fixnum_value(gpio_get_level(mrb_fixnum(pin)));
}

static mrb_value
mrb_esp32_gpio_analog_read(mrb_state *mrb, mrb_value self) {
  mrb_value ch;

  mrb_get_args(mrb, "o", &ch);

  if (!mrb_fixnum_p(ch)) {
    return mrb_nil_value();
  }

  adc1_config_channel_atten(mrb_fixnum(ch), ADC_ATTEN_11db);

  return mrb_fixnum_value(adc1_get_voltage(mrb_fixnum(ch)));
}

static mrb_value
mrb_esp32_gpio_hall_read(mrb_state *mrb, mrb_value self) {
  return mrb_fixnum_value(hall_sensor_read());
}

void
mrb_mruby_esp32_gpio_gem_init(mrb_state* mrb)
{
  struct RClass *esp32, *gpio, *constants;

  esp32 = mrb_define_module(mrb, "ESP32");

  gpio = mrb_define_module_under(mrb, esp32, "GPIO");
  mrb_define_module_function(mrb, gpio, "pinMode", mrb_esp32_gpio_pin_mode, MRB_ARGS_REQ(2));
  mrb_define_module_function(mrb, gpio, "digitalWrite", mrb_esp32_gpio_digital_write, MRB_ARGS_REQ(2));
  mrb_define_module_function(mrb, gpio, "digitalRead", mrb_esp32_gpio_digital_read, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, gpio, "analogWrite", mrb_esp32_gpio_analog_write, MRB_ARGS_REQ(2));
  mrb_define_module_function(mrb, gpio, "analogRead", mrb_esp32_gpio_analog_read, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, gpio, "hallRead", mrb_esp32_gpio_hall_read, MRB_ARGS_NONE());

  adc1_config_width(ADC_WIDTH_12Bit);

  constants = mrb_define_module_under(mrb, gpio, "Constants");

#define define_const(SYM) \
  do { \
    mrb_define_const(mrb, constants, #SYM, mrb_fixnum_value(SYM)); \
  } while (0)

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

  define_const(GPIO_NUM_21);
  define_const(GPIO_NUM_22);
  define_const(GPIO_NUM_23);

  define_const(GPIO_NUM_25);
  define_const(GPIO_NUM_26);
  define_const(GPIO_NUM_27);

  define_const(GPIO_NUM_32);
  define_const(GPIO_NUM_33);
  define_const(GPIO_NUM_34);
  define_const(GPIO_NUM_35);
  define_const(GPIO_NUM_36);
  define_const(GPIO_NUM_37);
  define_const(GPIO_NUM_38);
  define_const(GPIO_NUM_39);
  define_const(GPIO_NUM_MAX);

  define_const(DAC_CHANNEL_1);
  define_const(DAC_CHANNEL_2);
  define_const(DAC_CHANNEL_MAX);

  define_const(ADC1_CHANNEL_0);
  define_const(ADC1_CHANNEL_1);
  define_const(ADC1_CHANNEL_2);
  define_const(ADC1_CHANNEL_3);
  define_const(ADC1_CHANNEL_4);
  define_const(ADC1_CHANNEL_5);
  define_const(ADC1_CHANNEL_6);
  define_const(ADC1_CHANNEL_7);
  define_const(ADC1_CHANNEL_MAX);

  mrb_define_const(mrb, constants, "LOW", mrb_fixnum_value(0));
  mrb_define_const(mrb, constants, "HIGH", mrb_fixnum_value(1));

  mrb_define_const(mrb, constants, "INPUT",          mrb_fixnum_value(GPIO_MODE_INPUT));
  mrb_define_const(mrb, constants, "INPUT_OUTPUT",   mrb_fixnum_value(GPIO_MODE_INPUT_OUTPUT));
  mrb_define_const(mrb, constants, "OUTPUT",         mrb_fixnum_value(GPIO_MODE_OUTPUT));
  mrb_define_const(mrb, constants, "INPUT_PULLUP",   mrb_fixnum_value(GPIO_MODE_INPUT_PULLUP));
  mrb_define_const(mrb, constants, "INPUT_PULLDOWN", mrb_fixnum_value(GPIO_MODE_INPUT_PULLDOWN));
    
}

void
mrb_mruby_esp32_gpio_gem_final(mrb_state* mrb)
{
}
