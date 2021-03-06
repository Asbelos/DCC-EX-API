/*
    © 2020, Chris Harlow. All rights reserved.

    This file is part of Asbelos DCC API

    This is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    It is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with CommandStation.  If not, see <https://www.gnu.org/licenses/>.
*/
#include "WifiInterface.h"
#include "DIAG.h"
#include "StringFormatter.h"
#include "WiThrottle.h"
const char  PROGMEM READY_SEARCH[]  = "\r\nready\r\n";
const char  PROGMEM OK_SEARCH[] = "\r\nOK\r\n";
const char  PROGMEM END_DETAIL_SEARCH[] = "@ 1000";
const char  PROGMEM PROMPT_SEARCH[] = ">";
const char  PROGMEM SEND_OK_SEARCH[] = "\r\nSEND OK\r\n";
const char  PROGMEM IPD_SEARCH[] = "+IPD";
const unsigned long LOOP_TIMEOUT = 2000;
bool WifiInterface::connected = false;
bool WifiInterface::closeAfter = false;
DCCEXParser  WifiInterface::parser;
byte WifiInterface::loopstate = 0;
unsigned long WifiInterface::loopTimeoutStart = 0;
int WifiInterface::datalength = 0;
int WifiInterface::connectionId;
byte WifiInterface::buffer[MAX_WIFI_BUFFER+1];
MemStream  WifiInterface::streamer(buffer, MAX_WIFI_BUFFER);
Stream * WifiInterface::wifiStream = NULL;
HTTP_CALLBACK WifiInterface::httpCallback = 0;


void WifiInterface::setup(Stream & setupStream,  const __FlashStringHelper* SSid, const __FlashStringHelper* password,
                          const __FlashStringHelper* hostname,  int port) {

  wifiStream = &setupStream;

  DIAG(F("\n++++++ Wifi Setup In Progress ++++++++\n"));
  connected = setup2( SSid, password, hostname,  port);
 
  if (connected) {
    StringFormatter::send(wifiStream, F("ATE0\r\n")); // turn off the echo 
    checkForOK(200, OK_SEARCH, true);      
  }
 
 DIAG(F("\n++++++ Wifi Setup %S ++++++++\n"), connected ? F("OK") : F("FAILED"));
}

bool WifiInterface::setup2(const __FlashStringHelper* SSid, const __FlashStringHelper* password,
                           const __FlashStringHelper* hostname, int port) {
  int ipOK = 0;

  char macAddress[17];  //  mac address extraction   
     
  // First check... Restarting the Arduino does not restart the ES. 
  //  There may alrerady be a connection with data in the pipeline.
  // If there is, just shortcut the setup and continue to read the data as normal.
  if (checkForOK(200,IPD_SEARCH, true)) {
    DIAG(F("\nPreconfigured Wifi already running with data waiting\n"));
    loopstate=4;  // carry on from correct place 
    return true; 
  }

   
  StringFormatter::send(wifiStream, F("ATE1\r\n")); // Turn on the echo, se we can see what's happening
  checkForOK(2000, OK_SEARCH, true);      // Makes this visible on the console

  // Display the AT version information
  StringFormatter::send(wifiStream, F("AT+GMR\r\n")); 
  checkForOK(2000, OK_SEARCH, true, false);      // Makes this visible on the console

  delay(8000); // give a preconfigured ES8266 a chance to connect to a router  
  
  StringFormatter::send(wifiStream, F("AT+CIFSR\r\n"));

  // looking fpr mac addr eg +CIFSR:APMAC,"be:dd:c2:5c:6b:b7"
  if (checkForOK(5000, (const char*) F("+CIFSR:APMAC,\""), true,false)) {
    // Copy 17 byte mac address  
      for (int i=0; i<17;i++) {
        while(!wifiStream->available());
        macAddress[i]=wifiStream->read();
        StringFormatter::printEscape(&DIAGSERIAL,macAddress[i]);
      }    
  }
  char macTail[]={macAddress[9],macAddress[10],macAddress[12],macAddress[13],macAddress[15],macAddress[16],'\0'};
      
  if (checkForOK(5000, (const char*) F("+CIFSR:STAIP"), true,false))
    if (!checkForOK(1000, (const char*) F("0.0.0.0"), true,false))
      ipOK = 1;

  if (!ipOK) {
    StringFormatter::send(wifiStream, F("AT+CWMODE=3\r\n")); // configure as server or access point
    checkForOK(1000, OK_SEARCH, true); // Not always OK, sometimes "no change"

    // Older ES versions have AT+CWJAP, newer ones have AT+CWJAP_CUR and AT+CWHOSTNAME
    StringFormatter::send(wifiStream, F("AT+CWJAP?\r\n"));
    if (checkForOK(2000, OK_SEARCH, true)) {
      while (wifiStream->available()) StringFormatter::printEscape(&DIAGSERIAL, wifiStream->read()); /// THIS IS A DIAG IN DISGUISE
  
      // AT command early version supports CWJAP/CWSAP
      if (SSid) {
        StringFormatter::send(wifiStream, F("AT+CWJAP=\"%S\",\"%S\"\r\n"), SSid, password);
        checkForOK(16000, OK_SEARCH, true); // can ignore failure as AP mode may still be ok    
      }
      DIAG(F("\n**\n"));
      
      // establish the APname
      StringFormatter::send(wifiStream, F("AT+CWSAP=\"DCCEX_%s\",\"PASS_%s\",1,4\r\n"), macTail, macTail);
      checkForOK(16000, OK_SEARCH, true); // can ignore failure as AP mode may still be ok
      
    }
    else {
      // later version supports CWJAP_CUR
      
      StringFormatter::send(wifiStream, F("AT+CWHOSTNAME=\"%S\"\r\n"), hostname); // Set Host name for Wifi Client
      checkForOK(2000, OK_SEARCH, true); // dont care if not supported

      
      if (SSid) {
        StringFormatter::send(wifiStream, F("AT+CWJAP_CUR=\"%S\",\"%S\"\r\n"), SSid, password);
        checkForOK(20000, OK_SEARCH, true); // can ignore failure as AP mode may still be ok
      }
      
      StringFormatter::send(wifiStream, F("AT+CWSAP_CUR=\"DCCEX_%s\",\"PASS_%s\",1,4\r\n"), macTail, macTail);
      checkForOK(20000, OK_SEARCH, true); // can ignore failure as SSid mode may still be ok
      
      StringFormatter::send(wifiStream, F("AT+CIPRECVMODE=0\r\n"), port); // make sure transfer mode is correct
      checkForOK(2000, OK_SEARCH, true);
    }
  }

   
  StringFormatter::send(wifiStream, F("AT+CIPMUX=1\r\n")); // configure for multiple connections
  if (!checkForOK(10000, OK_SEARCH, true)) return false;

  StringFormatter::send(wifiStream, F("AT+CIPSERVER=1,%d\r\n"), port); // turn on server on port
  if (!checkForOK(10000, OK_SEARCH, true)) return false;
 
  StringFormatter::send(wifiStream, F("AT+CIFSR\r\n")); // Display  ip addresses to the DIAG 
  if (!checkForOK(10000, OK_SEARCH, true, false)) return false;
  DIAG(F("\nPORT=%d\n"),port);
   
  return true;
}


// This function is used to allow users to enter <+ commands> through the DCCEXParser
// Once the user has made whatever changes to the AT commands, a <+X> command can be used
// to force on the connectd flag so that the loop will start picking up wifi traffic.
// If the settings are corrupted <+RST> will clear this and then you must restart the arduino.
 
void WifiInterface::ATCommand(const byte * command) {
  command++;
  if (*command=='X') {
     connected = true;
     DIAG(F("\n++++++ Wifi Connction forced on ++++++++\n"));
  }
  else {
        StringFormatter::  send(wifiStream, F("AT+%s\r\n"), command);
        checkForOK(10000, OK_SEARCH, true);
  }
}

void WifiInterface::setHTTPCallback(HTTP_CALLBACK callback) {
  httpCallback = callback;
}

bool WifiInterface::checkForOK( const unsigned int timeout, const char * waitfor, bool echo, bool escapeEcho) {
  unsigned long  startTime = millis();
  char  const *locator = waitfor;
  DIAG(F("\nWifi Check: [%E]"), waitfor);
  while ( millis() - startTime < timeout) {
    while (wifiStream->available()) {
      int ch = wifiStream->read();
      if (echo) {
        if (escapeEcho) StringFormatter::printEscape(&DIAGSERIAL, ch); /// THIS IS A DIAG IN DISGUISE
        else DIAG(F("%c"), ch); 
      }
      if (ch != pgm_read_byte_near(locator)) locator = waitfor;
      if (ch == pgm_read_byte_near(locator)) {
        locator++;
        if (!pgm_read_byte_near(locator)) {
          DIAG(F("\nFound in %dms"), millis() - startTime);
          return true;
        }
      }
    }
  }
  DIAG(F("\nTIMEOUT after %dms\n"), timeout);
  return false;
}

bool WifiInterface::isHTTP() {

  // POST GET PUT PATCH DELETE
  // You may think a simple strstr() is better... but not when ram & time is in short supply
  switch (buffer[0]) {
    case 'P':
      if (buffer[1] == 'U' && buffer[2] == 'T' && buffer[3] == ' ' ) return true;
      if (buffer[1] == 'O' && buffer[2] == 'S' && buffer[3] == 'T' && buffer[4] == ' ') return true;
      if (buffer[1] == 'A' && buffer[2] == 'T' && buffer[3] == 'C' && buffer[4] == 'H' && buffer[5] == ' ') return true;
      return false;
    case 'G':
      if (buffer[1] == 'E' && buffer[2] == 'T' && buffer[3] == ' ' ) return true;
      return false;
    case 'D':
      if (buffer[1] == 'E' && buffer[2] == 'L' && buffer[3] == 'E' && buffer[4] == 'T' && buffer[5] == 'E' && buffer[6] == ' ') return true;
      return false;
    default:
      return false;
  }
}

void WifiInterface::loop() {
  if (!connected) return;

  WiThrottle::loop();  // check heartbeats

  // read anything into a buffer, collecting info on the way
  while (loopstate != 99 && wifiStream->available()) {
    int ch = wifiStream->read();

    // echo the char to the diagnostic stream in escaped format
    StringFormatter::printEscape(&DIAGSERIAL,ch); // DIAG in disguise

    switch (loopstate) {
      case 0:  // looking for +IPD
        connectionId = 0;
        if (ch == '+') loopstate = 1;
        break;
      case 1:  // Looking for I   in +IPD
        loopstate = (ch == 'I') ? 2 : 0;
        break;
      case 2:  // Looking for P   in +IPD
        loopstate = (ch == 'P') ? 3 : 0;
        break;
      case 3:  // Looking for D   in +IPD
        loopstate = (ch == 'D') ? 4 : 0;
        break;
      case 4:  // Looking for ,   After +IPD
        loopstate = (ch == ',') ? 5 : 0;
        break;
      case 5:  // reading connection id
        if (ch == ',') loopstate = 6;
        else connectionId = 10 * connectionId + (ch - '0');
        break;
      case 6: // reading for length
        if (ch == ':') loopstate = (datalength == 0) ? 99 : 7; // 99 is getout without reading next char
        else datalength = datalength * 10 + (ch - '0');
        streamer.flush();  // basically sets write point at start of buffer
        break;
      case 7: // reading data
        streamer.write(ch); // NOTE: The MemStream will throw away bytes that do not fit in the buffer.
                            // This protects against buffer overflows even with things as innocent
                            // as a browser which send massive, irrlevent HTTP headers.   
        datalength--;
        if (datalength == 0) {
          buffer[streamer.available()]='\0'; // mark end of buffer, so it can be used as a string later
          loopstate = 99;
        }
        break;

      case 10:  // Waiting for > so we can send reply
        if (millis() - loopTimeoutStart > LOOP_TIMEOUT) {
          DIAG(F("\nWifi TIMEOUT on wait for > prompt or ERROR\n"));
          loopstate = 0; // go back to +IPD
          break;
        }
        if (ch == '>') {
          //                  DIAG(F("\n> [%e]\n"),buffer);
          wifiStream->print((char *) buffer);
          loopTimeoutStart = millis();
          loopstate = closeAfter ? 11 : 0;
          break;
        }
        if (ch == '.') { // busy during send, delay and retry  
          loopstate = 12; // look for SEND OK finished 
          break;
        }
        break;
      case 11: // Waiting for SEND OK or ERROR to complete so we can closeAfter
        if (millis() - loopTimeoutStart > LOOP_TIMEOUT) {
          DIAG(F("\nWifi TIMEOUT on wait for SEND OK or ERROR\n"));
          loopstate = 0; // go back to +IPD
          break;
        }
        if (ch == 'K') { // assume its in  SEND OK
          DIAG(F("\n Wifi AT+CIPCLOSE=%d\r\n"), connectionId);
          StringFormatter::send(wifiStream, F("AT+CIPCLOSE=%d\r\n"), connectionId);
          loopstate = 0; // wait for +IPD
        }
        break;

    case 12: // Waiting for OK after send busy 
        if (ch == '+') { // Uh-oh IPD problem
          DIAG(F("\n\n Wifi ASYNC CLASH - LOST REPLY\n"));
          connectionId = 0;
          loopstate = 1;
        }
        if (ch == 'K') { // assume its in  SEND OK
          DIAG(F("\n\n Wifi BUSY RETRYING.. AT+CIPSEND=%d,%d\r\n"), connectionId, streamer.available());
          StringFormatter::send(wifiStream, F("AT+CIPSEND=%d,%d\r\n"), connectionId, streamer.available());
          loopTimeoutStart = millis();
          loopstate = 10; // non-blocking loop waits for > before sending
          break;
        }
        break;
    } // switch
  } // while
  if (loopstate != 99) return;

  // AT this point we have read an incoming message into the buffer
 
  DIAG(F("\n%l Wifi(%d)<-[%e]\n"), millis(),connectionId, buffer);
  streamer.setBufferContentPosition(0, 0); // reset write position to start of buffer
  // SIDE EFFECT WARNING:::
  //  We know that parser will read the entire buffer before starting to write to it.
  //  Otherwise we would have to copy the buffer elsewhere and RAM is in short supply.

  closeAfter = false;

  // Intercept HTTP requests
  if (isHTTP()) {
    if (httpCallback) httpCallback(&streamer, buffer);
    else {
      StringFormatter::send(streamer, F("HTTP/1.1 404 Not Found\nContent-Type: text/html\nConnnection: close\n\n"));
      StringFormatter::send(streamer, F("<html><body>This is <b>not</b> a web server.<br/></body></html>"));
    }
    closeAfter = true;
  }
  else if (buffer[0] == '<')  parser.parse(&streamer, buffer, true); // tell JMRI parser that ACKS are blocking because we can't handle the async

  else WiThrottle::getThrottle(connectionId)->parse(streamer, buffer);

  if (streamer.available() == 0) {
    // No reply
    if (closeAfter) {
         DIAG(F("AT+CIPCLOSE=%d\r\n"), connectionId);
         StringFormatter::send(wifiStream, F("AT+CIPCLOSE=%d\r\n"), connectionId);
    }
    loopstate = 0; // go back to waiting for +IPD
    return;
  }
  // prepare to send reply
  buffer[streamer.available()]='\0'; // mark end of buffer, so it can be used as a string later
  DIAG(F("%l WiFi(%d)->[%e] l(%d)\n"), millis(), connectionId, buffer, streamer.available());
  DIAG(F("AT+CIPSEND=%d,%d\r\n"), connectionId, streamer.available());
  StringFormatter::send(wifiStream, F("AT+CIPSEND=%d,%d\r\n"), connectionId, streamer.available());
  loopTimeoutStart = millis();
  loopstate = 10; // non-blocking loop waits for > before sending
}
