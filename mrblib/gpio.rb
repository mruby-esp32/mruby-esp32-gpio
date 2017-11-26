module ESP32
  module GPIO
    include Constants
    
    class << self
      alias :digital_write :digitalWrite   
      alias :digital_read  :digitalRead
      alias :analog_write  :analogWrite   
      alias :analog_read   :analogRead    
      alias :pin_mode      :pinMode 
      alias :hall_read     :hallRead   
    end  
  
    class Pin
      PIN_MODE = {
        pullup:   ESP32::GPIO::INPUT_PULLUP,
        pulldown: ESP32::GPIO::INPUT_PULLDOWN,
        input:    ESP32::GPIO::INPUT,
        output:   ESP32::GPIO::OUTPUT,
        inout:    ESP32::GPIO::INPUT_OUTPUT
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
