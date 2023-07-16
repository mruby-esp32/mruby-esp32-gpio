module ESP32
  include Constants
  
  module GPIO
    INPUT                 = ESP32::GPIO_MODE_INPUT
    OUTPUT                = ESP32::GPIO_MODE_OUTPUT
    INPUT_PULLUP          = ESP32::GPIO_MODE_INPUT_PULLUP
    INPUT_PULLDOWN        = ESP32::GPIO_MODE_INPUT_PULLDOWN
    INPUT_PULLUP_PULLDOWN = ESP32::GPIO_MODE_INPUT_PULLUP_PULLDOWN
    INPUT_OUTPUT          = ESP32::GPIO_MODE_INPUT_OUTPUT
    INPUT_OUTPUT_OD       = ESP32::GPIO_MODE_INPUT_OUTPUT_OD
    OUTPUT_OD             = ESP32::GPIO_MODE_OUTPUT_OD

    class Pin
      PIN_MODE = {
        input:                  ESP32::GPIO_MODE_INPUT,
        output:                 ESP32::GPIO_MODE_OUTPUT,
        input_pullup:           ESP32::GPIO_MODE_INPUT_PULLUP,
        input_pulldown:         ESP32::GPIO_MODE_INPUT_PULLDOWN,
        input_pullup_pulldown:  ESP32::GPIO_MODE_INPUT_PULLUP_PULLDOWN,
        input_output:           ESP32::GPIO_MODE_INPUT_OUTPUT,
        input_output_od:        ESP32::GPIO_MODE_INPUT_OUTPUT_OD,
        output_od:              ESP32::GPIO_MODE_OUTPUT_OD,
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
        val
      end 
      
      def high!
        write HIGH
      end
      
      def low!
        write LOW
      end
      
      def off
        low!
      end
      
      def on
        high!
      end
    
      def mode= mode
        GPIO.pin_mode pin, mode
      end
    
      alias :digital_write :write   
      alias :digital_read  :read
    
      def high?
        read == LOW
      end
      
      def low?
        read == HIGH
      end
    
      # the following only work if GPIO_MODE_INPUT_OUTPUT ie, Pin.new(io_num, :inout)
      def toggle
        write((read==HIGH) ? LOW : HIGH)
      end
    end
  end
end
