// Basic DCC command station sketch using the DCC-EX-API libraray
// Plus Wifi support for WiThrottle devices 

#include <DCC-EX-API.h>

#ifdef ARDUINO_AVR_UNO 
  #include <SoftwareSerial.h>
  SoftwareSerial Serial1(15,16); // YOU must get thee pins correct to use Wifi on a UNO
  #define WIFI_BAUD 9600
#else 
  #define WIFI_BAUD 115200 
#endif 
 



// Create a serial command parser... This is OPTIONAL if you don't need to handle JMRI type commands
// from the Serial port.
DCCEXParser  serialParser;

void setup() {

  // The main sketch has responsibilities during setup()
  
  // Responsibility 1: Start the usb connection for diagnostics and possible JMRI input
  // DIAGSERAL is normally Serial but uses SerialUSB on a SAMD processor
  DIAGSERIAL.begin(115200);
  while(!DIAGSERIAL);
  
   // Responsibility 2: Start the DCC engine.
   // Note: this provides DCC with two motor drivers, main and prog, which handle the motor shield(s)
   // Standard supported devices have pre-configured macros but custome hardware installations require 
   //  detailed pin mappings and may also require modified subclasses of the MotorDriver to implement specialist logic.
     
   DCC::begin(STANDARD_MOTOR_SHIELD);

   // Responsibility 3: Start the WiFi interface if required.
   //   NOTE: On a Uno you will have to provide a SoftwareSerial 
   //         configured for the pins connected to the Wifi card
   //         and a 9600 baud rate. 
   //  setup(serial, F(router name), F(password), F(hostname) , port)
   //   (port 3532 is 0xDCC decimal.)     

      
    Serial1.begin(WIFI_BAUD);
    WifiInterface::setup(Serial1, F("BTHub5-M6PT"), F("49de8d4862"),F("DCCEX"),3532); 
      
}

void loop() {      
  // The main sketch has responsibilities during loop()
  
  // Responsibility 1: Handle DCC background processes
  //                   (loco reminders and power checks)
  DCC::loop(); 

  // Responsibility 2: handle any incoming commands on USB connection
  serialParser.loop(DIAGSERIAL);

  // Responsibility 3: Optionally handle any incoming WiFi traffic
  WifiInterface::loop();

}
