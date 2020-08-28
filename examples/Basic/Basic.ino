// Basic DCC command station sketch using the DCC-EX-API libraray

#include <DCC-EX-API.h>
 

// Create a serial command parser... to handle JMRI type commands
// from the Serial port.
DCCEXParser  serialParser;

void setup() {

  // The main sketch has responsibilities during setup()
  
  // Responsibility 1: Start the usb connection for diagnostics and possible JMRI input
  // DIAGSERAL is normally Serial but uses SerialUSB on a SAMD processor
  DIAGSERIAL.begin(115200);
  while(!DIAGSERIAL);
  
  // Responsibility 2: Start the DCC engine.
  // Note: refer to MotorDrivers.h in the libraray for names of
  // devices supported "out of the box".

   DCC::begin(STANDARD_MOTOR_SHIELD);
  }

void loop() {      
  // Responsibility 1: Handle DCC background processes
  DCC::loop(); 

  // Responsibility 2: Handle any incoming commands on USB connection
  serialParser.loop(DIAGSERIAL);
}
