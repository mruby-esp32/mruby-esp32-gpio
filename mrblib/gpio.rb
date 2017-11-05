def sleep secs
  ESP32::System.delay(secs * 1000)
end

module ESP32
  module GPIO
    include Constants
    
    # Less than seems to crash
    DEFAULT_ISR_TASK_RATE = 8192

    # Store the ISR Handlers
	@isrs = {}
	
	# add interrupt handler for a pin
	def self.on_interrupt pin, edge = ESP32::GPIO::INTR_ANY, &b
	  if b
	    if !self.isr_init?
	      debug "Set ISR task-rate #{DEFAULT_ISR_TASK_RATE}"
	      init_isr(DEFAULT_ISR_TASK_RATE) 
	    end
	    
	    @isrs[pin] = b 
	    
	    debug "add ISR Handle for: #{pin}"
	    ESP32::GPIO.add_isr_handler(pin, edge)
	  else 
	    raise "No Block Given"
      end
	end
	
	# Called from ISR xTask
	def self.dispatch_isr pin
	  return unless cb = @isrs[pin]
	  
	  if block_given?
	    debug "Dispatch GPIO ISR: pin - #{pin}"
	    yield pin
	  else
	    dispatch_isr pin, &cb
	  end
	end
  
    # puts msg if debug_level >= lvl
    def self.debug msg, lvl=1
      puts msg if debug_level >= lvl
    end
  
    def self.debug_level
      get_debug_level
    end
  
    def self.debug_level= lvl
      set_debug_level lvl
    end
  
    class << self
      # Why camel case?
      alias :digitalWrite :digital_write   
      alias :digitalRead :digital_read
      alias :analogWrite :analog_write   
      alias :analogRead :analog_read    
      alias :pinMode :pin_mode 
      alias :hallRead :hall_read   
    end  
  
    class Pin
      PIN_MODE = {
        pullup:   ESP32::GPIO::INPUT_PULLUP,
        pulldown: ESP32::GPIO::INPUT_PULLDOWN,
        input:    ESP32::GPIO::INPUT,
        output:   ESP32::GPIO::OUTPUT
      }
    
      EDGE_MAP = {
        any:      ESP32::GPIO::INTR_ANY,
        high:     ESP32::GPIO::INTR_HIGH,
        low:      ESP32::GPIO::INTR_LOW,
        positive: ESP32::GPIO::INTR_POS,
        negative: ESP32::GPIO::INTR_NEG,
        negative: ESP32::GPIO::INTR_DISABLE,      
      }
    
      
      attr_reader :pin
      def initialize pin, mode = :input
        mode = PIN_MODE[mode] unless mode.is_a?(Integer)
        @pin = pin
        self.mode= mode
      end
  
      def analog_read
        GPIO.analog_read pin
      end
    
      def read
        GPIO.digital_read pin
      end 
     
      def analog_write val
        GPIO.analog_write pin, val
      end
    
      def write val
        GPIO.digital_write pin, val
      end 
    
      def mode= mode
        GPIO.pin_mode pin, mode
      end
    
      def on_interrupt edge = :any, &b
        edge = EDGE_MAP[edge] unless edge.is_a?(Integer)
        GPIO.on_interrupt pin, edge, &b
      end
    
      alias :digital_write :write   
      alias :digital_read :read
    end
  end
end
