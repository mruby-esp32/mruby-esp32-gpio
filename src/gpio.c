#include <mruby.h>
#include <mruby/value.h>
#include "string.h"
#include "driver/gpio.h"
#include "driver/dac.h"
#include "driver/adc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#define GPIO_MODE_DEF_PULLUP (BIT3)
#define GPIO_MODE_INPUT_PULLUP ((GPIO_MODE_INPUT)|(GPIO_MODE_DEF_PULLUP))

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

static mrb_value
mrb_esp32_gpio_set_intr_type(mrb_state* mrb, mrb_value self) {
	return mrb_nil_value();
}

static void IRAM_ATTR mrb_esp32_gpio_isr_cb(void* data) {
  
}

static mrb_value
mrb_esp32_gpio_install_isr_service(mrb_state* mrb, mrb_value self) {
return mrb_nil_value();
}

typedef struct {
  mrb_state* mrb;
  mrb_value obj;
  int pin;
} cb_env;

static cb_env env;

static xQueueHandle gpio_evt_queue = NULL;
static mrb_value mrb_esp32_gpio_call_mrb_isr_handler(void* data) {
	uint32_t gpio_num = (uint32_t) data;
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
    return mrb_nil_value();
}

static void gpio_task_example(void* data)
{
    uint32_t io_num = (int)data;
    for(;;) {
        if(xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY)) {
           printf("GPIO[%d] intr, val: %d\n", io_num, gpio_get_level(io_num));
           mrb_funcall(env.mrb, mrb_top_self(env.mrb), "isr", 1, mrb_fixnum_value(io_num));  
        }
    }
}

static mrb_value
mrb_esp32_gpio_isr_handler_add(mrb_state* mrb, mrb_value self) {
	mrb_int pin;
	mrb_get_args(mrb,"i",&pin);
	
	//
    gpio_set_intr_type((int)pin, GPIO_INTR_ANYEDGE);
    //
	
	//
	gpio_install_isr_service(0);
	//

    //	
    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));
    //start gpio task
    xTaskCreate(gpio_task_example, "gpio_task_example", 2048, NULL, 10, NULL);
	//
	
	
	//cb_env 
	env.mrb = mrb;
	gpio_isr_handler_add((int)pin,mrb_esp32_gpio_call_mrb_isr_handler,(void*) pin);    
    return mrb_nil_value();//mrb_esp32_gpio_call_mrb_isr_handler((void*) &env);
}

static mrb_value
mrb_esp32_gpio_isr_handler_remove(mrb_state* mrb, mrb_value self) {
return mrb_nil_value();
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
  mrb_define_module_function(mrb, gpio, "set_interrupt_type", mrb_esp32_gpio_set_intr_type, MRB_ARGS_NONE());  
  mrb_define_module_function(mrb, gpio, "add_isr_handler", mrb_esp32_gpio_isr_handler_add, MRB_ARGS_NONE());  
  mrb_define_module_function(mrb, gpio, "remove_isr_handler", mrb_esp32_gpio_isr_handler_remove, MRB_ARGS_ANY());  
  mrb_define_module_function(mrb, gpio, "install_isr_service", mrb_esp32_gpio_install_isr_service, MRB_ARGS_NONE());   
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

  mrb_define_const(mrb, constants, "INPUT", mrb_fixnum_value(GPIO_MODE_INPUT));
  mrb_define_const(mrb, constants, "INPUT_PULLUP", mrb_fixnum_value(GPIO_MODE_INPUT_PULLUP));
  mrb_define_const(mrb, constants, "OUTPUT", mrb_fixnum_value(GPIO_MODE_OUTPUT));
}

void
mrb_mruby_esp32_gpio_gem_final(mrb_state* mrb)
{
}
