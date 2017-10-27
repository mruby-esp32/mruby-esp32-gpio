def sleep secs
  ESP32::System.delay(secs * 1000)
end
$isrs={}
def isr pin, &b
  $isrs[pin] = b if b
  
  ESP32::GPIO.add_isr_handler(pin) if b
  
  return if b
  
  $isrs[pin].call(pin) unless b
end

def call(pin)
  isr pin
end

puts "INIT:"

isr 18, &(B=Proc.new do |pin|
  print " #{pin} "
end)

while true;end


