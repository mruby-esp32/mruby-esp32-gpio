ESP32::GPIO.debug_level=1

pin = ESP32::GPIO::Pin.new(18, :pullup)

pin.on_interrupt do |gpio_num|
  puts "INTR: on pin: #{gpio_num}."
end 

puts "Running!"

while true;
end    


