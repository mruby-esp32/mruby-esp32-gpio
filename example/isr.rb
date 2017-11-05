#include <stdio.h>
#include <string.h>

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/timers.h>
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "mruby.h"
#include "mruby/irep.h"
#include "mruby/compile.h"
#include "mruby/error.h"
#include "mruby/string.h"

#include "example_mrb.h"

#define TAG "mruby_task"

typedef struct {
  mrb_value cb;
  bool free;	
  mrb_value data;
} mesp_event_t;

static mesp_event_t mesp_idle_event;

static int id=0;
static QueueHandle_t mesp_event_queue;

static void mesp_send_event(mesp_event_t* event, bool isr) {
	if(isr) {
		xQueueSendToBackFromISR(mesp_event_queue, event, NULL);
	} else {
		xQueueSendToBack(mesp_event_queue, event, portMAX_DELAY);
	}
}

void mesp_timer_cb(TimerHandle_t timer) {	
  mesp_event_t* event = (mesp_event_t*)pvTimerGetTimerID(timer);
  
  mesp_send_event(event, FALSE);

  if (event->free) {
	  xTimerStop(timer, 0);
	  xTimerDelete(timer, 0);
  }
}

static void mesp_attach_timer(int interval, bool repeat, mrb_value cb) {
	static mesp_event_t event;

	event.cb   = cb;
	event.data = mrb_nil_value();
	event.free = TRUE;
	
	if (repeat) {
		event.free = FALSE;
	}

	if (xTimerStart(xTimerCreate("t", interval, repeat, &event, mesp_timer_cb), 0) != pdPASS) {
	  printf("Timer Failed.");	
	}
}

static void mesp_timeout(int delay, mrb_value cb) {
  mesp_attach_timer(delay, FALSE, cb);
};

static void mesp_idle(mrb_value cb) {
	mesp_idle_event.data = mrb_nil_value();
	mesp_idle_event.cb   = cb;
	mesp_idle_event.free = false;
}

void mesp_process_event(mrb_state* mrb, mesp_event_t* event) {
 	if (!mrb_nil_p(event->cb)) {
	  int ai = mrb_gc_arena_save(mrb);
      mrb_funcall(mrb, event->cb, "call", 1, event->data); 
      mrb_gc_arena_restore(mrb, ai);	
    }	
}
	

int mesp_poll_event(mrb_state* mrb) {
	mesp_event_t event;
	if(xQueueReceive(mesp_event_queue, &event, 0)) { 	
		mesp_process_event(mrb,&event);
		return 1;
	}	
	
	return 0;
}

void mesp_main_loop(mrb_state* mrb) {
  while(1) {    
	if (mesp_poll_event(mrb) == 1){
    } else {
	  int ai = mrb_gc_arena_save(mrb);
      mrb_funcall(mrb, mesp_idle_event.cb, "call", 1, mrb_nil_value()); 
      mrb_gc_arena_restore(mrb, ai);
   	}
   	
    vTaskDelay(1);
  }
}

static mrb_value mesp_timeout_add(mrb_state* mrb, mrb_value self) {
	mrb_int delay;
	mrb_value block;
	mrb_get_args(mrb,"i&",&delay, &block);
	
	mesp_timeout(delay, block);
	
	return self;
}

static mrb_value mesp_idle_add(mrb_state* mrb, mrb_value self) {
	mrb_value block;
	mrb_get_args(mrb,"&", &block);
	
	mesp_idle(block);
	
	return self;
}

void mesp_init(mrb_state* mrb) {
	mrb_define_method(mrb, mrb->object_class, "timeout", mesp_timeout_add, MRB_ARGS_REQ(1));
    mrb_define_method(mrb, mrb->object_class, "idle", mesp_idle_add, MRB_ARGS_NONE());
}


static void mesp_task(void* pvParameter) {
  mesp_event_queue = xQueueCreate( 20, sizeof(mesp_event_t));
  
  mrb_state *mrb = mrb_open();
  mrbc_context *context = mrbc_context_new(mrb);
  int ai = mrb_gc_arena_save(mrb);
  ESP_LOGI(TAG, "%s", "Loading binary...");

  mesp_init(mrb);
  mrb_load_irep_cxt(mrb, example_mrb, context);
  if (mrb->exc) {
    ESP_LOGE(TAG, "Exception occurred: %s", mrb_str_to_cstr(mrb, mrb_inspect(mrb, mrb_obj_value(mrb->exc))));
    mrb->exc = 0;
  } else {
    ESP_LOGI(TAG, "%s", "Success");
  }
  mrb_gc_arena_restore(mrb, ai);
  mesp_main_loop(mrb);
  mrbc_context_free(mrb, context);
  mrb_close(mrb);  
}

void app_main()
{
  nvs_flash_init();
  xTaskCreatePinnedToCore(&mesp_task, "duktape_task", 16*1024, NULL, 5, NULL, tskNO_AFFINITY);
}



def interval d, &b
  timeout d do
    b.call
    interval d, &b
  end
end

pin = ESP32::GPIO::Pin.new(23, :output)

t   = 0
cnt = 0

interval 11 do
  print "\rIDLE: #{t} times."
  pin.write cnt += 1
  cnt = -1 if cnt > 1
end

idle do
  t+=1
end
