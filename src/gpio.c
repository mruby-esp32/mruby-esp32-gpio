#include <mruby.h>
#include <mruby/value.h>
#include <mruby/variable.h>
#include "string.h"
#include "driver/gpio.h"
#include "driver/dac.h"
#include "driver/adc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#define GPIO_MODE_DEF_PULLUP (BIT3)
#define GPIO_MODE_DEF_PULLDOWN (BIT3)
#define GPIO_MODE_INPUT_PULLUP ((GPIO_MODE_INPUT)|(GPIO_MODE_DEF_PULLUP))
#define GPIO_MODE_INPUT_PULLDOWN ((GPIO_MODE_INPUT)|(GPIO_MODE_DEF_PULLDOWN))
#define GPIO_MODE_OUTPUT (GPIO_MODE_DEF_OUTPUT) 
#define GPIO_MODE_OUTPUT_OD ((GPIO_MODE_DEF_OUTPUT)|(GPIO_MODE_DEF_OD))
#define GPIO_MODE_INPUT_OUTPUT_OD ((GPIO_MODE_DEF_INPUT)|(GPIO_MODE_DEF_OUTPUT)|(GPIO_MODE_DEF_OD))
#define GPIO_MODE_INPUT_OUTPUT ((GPIO_MODE_DEF_INPUT)|(GPIO_MODE_DEF_OUTPUT))


typedef struct {
  mrb_state* mrb;
  int debug;
  int isr_init;
} mrb_esp32_gpio_env_t;

mrb_esp32_gpio_env_t mrb_esp32_gpio_env;


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

static xQueueHandle mrb_esp32_gpio_isr_evt_queue = NULL;

static void IRAM_ATTR mrb_esp32_gpio_call_isr_handler(void* data) {
	uint32_t gpio_num = (uint32_t) data;
    
    xQueueSendFromISR(mrb_esp32_gpio_isr_evt_queue, &gpio_num, NULL);
}

static void mrb_esp32_gpio_isr_task(void* data)
{
    uint32_t io_num = (int)data;
    for(;;) {
        if(xQueueReceive(mrb_esp32_gpio_isr_evt_queue, &io_num, portMAX_DELAY)) {
           if (mrb_esp32_gpio_env.debug == 1) {
			   printf("GPIO[%d] intr, val: %d\n", io_num, gpio_get_level(io_num));
           }
           
           int ai = mrb_gc_arena_save(mrb_esp32_gpio_env.mrb);
           mrb_value esp32 = mrb_const_get(mrb_esp32_gpio_env.mrb,mrb_obj_value(mrb_esp32_gpio_env.mrb->object_class), mrb_intern_cstr(mrb_esp32_gpio_env.mrb, "ESP32"));
           mrb_funcall(mrb_esp32_gpio_env.mrb, mrb_const_get(mrb_esp32_gpio_env.mrb, esp32, mrb_intern_cstr(mrb_esp32_gpio_env.mrb, "GPIO")), "dispatch_isr", 1, mrb_fixnum_value(io_num));  
           mrb_gc_arena_restore(mrb_esp32_gpio_env.mrb, ai);
        }
    }
}

static mrb_value
mrb_esp32_gpio_init_isr(mrb_state* mrb, mrb_value self) {
	if (mrb_esp32_gpio_env.isr_init == 1) {
		return mrb_nil_value();
	}
	
	mrb_esp32_gpio_env.mrb = mrb;	
	
    mrb_int task_size;
    mrb_get_args(mrb,"i", &task_size);
    
    gpio_install_isr_service(0);

    if (mrb_esp32_gpio_env.debug == 1) {
      printf("ISR: Init.");
    }
        
    mrb_esp32_gpio_isr_evt_queue = xQueueCreate(10, sizeof(uint32_t));

    xTaskCreate(mrb_esp32_gpio_isr_task, "mrb_esp32_gpio_isr_task", (uint32_t)task_size, NULL, 10, NULL);
	
	mrb_esp32_gpio_env.isr_init = 1;
	
    if (mrb_esp32_gpio_env.debug == 1) {
      printf("ISR: Init END.");
    }
	
	return self;
}

static mrb_value
mrb_esp32_gpio_isr_handler_add(mrb_state* mrb, mrb_value self) {
	mrb_int pin, edge;
	mrb_get_args(mrb,"ii",&pin, &edge);
	
    gpio_set_intr_type((int)pin, (uint32_t)edge);
	gpio_isr_handler_add((int)pin,mrb_esp32_gpio_call_isr_handler,(void*) pin);    
    
    return self;
}

static mrb_value
mrb_esp32_gpio_isr_handler_remove(mrb_state* mrb, mrb_value self) {
  return mrb_nil_value();
}

static mrb_value
mrb_esp32_gpio_get_debug_level(mrb_state* mrb, mrb_value self) {
	return mrb_fixnum_value(mrb_esp32_gpio_env.debug);
}

static mrb_value
mrb_esp32_gpio_set_debug_level(mrb_state* mrb, mrb_value self) {
	mrb_int level;
	mrb_get_args(mrb, "i", &level);
	
	mrb_esp32_gpio_env.debug = (int)level;
	
	return mrb_fixnum_value(level);
}

static mrb_value
mrb_esp32_gpio_get_isr_init(mrb_state* mrb, mrb_value self) {
	if (mrb_esp32_gpio_env.isr_init == 1) {
		return self;
	}
	
	return mrb_nil_value();
}

void
mrb_mruby_esp32_gpio_gem_init(mrb_state* mrb)
{
  mrb_esp32_gpio_env_t env = mrb_esp32_gpio_env;
  env.isr_init  = 0;
  env.debug     = 1;
  	
  struct RClass *esp32, *gpio, *constants;

  esp32 = mrb_define_module(mrb, "ESP32");

  gpio = mrb_define_module_under(mrb, esp32, "GPIO");
  mrb_define_module_function(mrb, gpio, "pin_mode", mrb_esp32_gpio_pin_mode, MRB_ARGS_REQ(2));
  mrb_define_module_function(mrb, gpio, "digital_write", mrb_esp32_gpio_digital_write, MRB_ARGS_REQ(2));
  mrb_define_module_function(mrb, gpio, "digital_read", mrb_esp32_gpio_digital_read, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, gpio, "analog_write", mrb_esp32_gpio_analog_write, MRB_ARGS_REQ(2));
  mrb_define_module_function(mrb, gpio, "analog_read", mrb_esp32_gpio_analog_read, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, gpio, "hall_read", mrb_esp32_gpio_hall_read, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, gpio, "add_isr_handler", mrb_esp32_gpio_isr_handler_add, MRB_ARGS_REQ(2));  
  mrb_define_module_function(mrb, gpio, "remove_isr_handler", mrb_esp32_gpio_isr_handler_remove, MRB_ARGS_REQ(1));  
  mrb_define_module_function(mrb, gpio, "init_isr", mrb_esp32_gpio_init_isr, MRB_ARGS_REQ(1));   
  
  mrb_define_module_function(mrb, gpio, "set_debug_level", mrb_esp32_gpio_set_debug_level, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, gpio, "get_debug_level", mrb_esp32_gpio_get_debug_level, MRB_ARGS_NONE());    

  mrb_define_module_function(mrb, gpio, "isr_init?", mrb_esp32_gpio_get_isr_init, MRB_ARGS_NONE());  

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
  mrb_define_const(mrb, constants, "OUTPUT", mrb_fixnum_value(GPIO_MODE_OUTPUT));
  mrb_define_const(mrb, constants, "INPUT_PULLUP", mrb_fixnum_value(GPIO_MODE_INPUT_PULLUP));
  mrb_define_const(mrb, constants, "INPUT_PULLDOWN", mrb_fixnum_value(GPIO_MODE_INPUT_PULLDOWN));
    
  mrb_define_const(mrb, constants, "INTR_DISABLE", mrb_fixnum_value(GPIO_INTR_DISABLE));
  mrb_define_const(mrb, constants, "INTR_ANY", mrb_fixnum_value(GPIO_INTR_ANYEDGE));
  mrb_define_const(mrb, constants, "INTR_NEG", mrb_fixnum_value(GPIO_INTR_NEGEDGE));    
  mrb_define_const(mrb, constants, "INTR_POS", mrb_fixnum_value(GPIO_INTR_POSEDGE));   
  mrb_define_const(mrb, constants, "INTR_LOW", mrb_fixnum_value(GPIO_INTR_LOW_LEVEL));       
  mrb_define_const(mrb, constants, "INTR_HIGH", mrb_fixnum_value(GPIO_INTR_HIGH_LEVEL));   
  mrb_define_const(mrb, constants, "INTR_MAX", mrb_fixnum_value(GPIO_INTR_MAX));  
}

void
mrb_mruby_esp32_gpio_gem_final(mrb_state* mrb)
{
}
