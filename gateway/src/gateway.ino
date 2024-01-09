// **********************************************************************************
//            !!!!     ATTENTION:    !!!!
// This is just a simple receiving sketch that will work with most examples
// in the RFM69 library.
//
// If you're looking for the Gateway sketch to use with your RaspberryPi,
// as part of the PiGateway software interface (lowpowerlab.com/gateway),
// this is the wrong sketch. Use this sketch instead: PiGateway:
// https://github.com/LowPowerLab/RFM69/blob/master/Examples/PiGateway/PiGateway.ino
// **********************************************************************************
// Sample RFM69 receiver/gateway sketch, with ACK and optional encryption, and Automatic Transmission Control
// Passes through any wireless received messages to the serial port & responds to ACKs
// It also looks for an onboard FLASH chip, if present
// **********************************************************************************
// Copyright Felix Rusu 2016, http://www.LowPowerLab.com/contact
// **********************************************************************************
// License
// **********************************************************************************
// This program is free software; you can redistribute it 
// and/or modify it under the terms of the GNU General    
// Public License as published by the Free Software       
// Foundation; either version 3 of the License, or        
// (at your option) any later version.                    
//                                                        
// This program is distributed in the hope that it will   
// be useful, but WITHOUT ANY WARRANTY; without even the  
// implied warranty of MERCHANTABILITY or FITNESS FOR A   
// PARTICULAR PURPOSE. See the GNU General Public        
// License for more details.                              
//                                                        
// Licence can be viewed at                               
// http://www.gnu.org/licenses/gpl-3.0.txt
//
// Please maintain this license information along with authorship
// and copyright notices in any redistribution of this code
// **********************************************************************************
#include <RFM69.h>         //get it here: https://www.github.com/lowpowerlab/rfm69
#include <RFM69_ATC.h>     //get it here: https://www.github.com/lowpowerlab/rfm69
#include <string.h>        //included with Arduino IDE (www.arduino.cc)
// #include <SPIFlash.h>      //get it here: https://www.github.com/lowpowerlab/spiflash
// #include <SPI.h>           //included with Arduino IDE (www.arduino.cc)

//*********************************************************************************************
//************ IMPORTANT SETTINGS - YOU MUST CHANGE/CONFIGURE TO FIT YOUR HARDWARE *************
//*********************************************************************************************
#define NODEID        1    //should always be 1 for a Gateway
#define NETWORKID     100  //the same on all nodes that talk to each other
//Match frequency to the hardware version of the radio on your Moteino (uncomment one):
#define FREQUENCY     RF69_433MHZ
// #define FREQUENCY     RF69_868MHZ
// #define FREQUENCY     RF69_915MHZ
// #define FREQUENCY_EXACT 916000000 // you may define an exact frequency/channel in Hz

// FCC Section §97.113(a)(4) No amateur station shall transmit: messages encoded for the purpose of obscuring their meaning
// https://www.ecfr.gov/current/title-47/chapter-I/subchapter-D/part-97
// FCC Section §1.925 allows us to apply for a temporary waiver to test encryption before our satellite is in space
// https://www.ecfr.gov/current/title-47/chapter-I/subchapter-A/part-1/subpart-F/subject-group-ECFR8fff3365c42ee11/section-1.925
// FCC Section §97.211(b) A telecommand station may transmit special codes intended to obscure the meaning of telecommand messages to the station in space operation.
// https://www.ecfr.gov/current/title-47/chapter-I/subchapter-D/part-97
// Apply for waiver and uncomment before launch
// #define ENCRYPTKEY    "sampleEncryptKey" //exactly the same 16 characters/bytes on all nodes!

// FCC Section §97.119(a) Each amateur station, except a space station or telecommand station, must transmit its assigned call sign on its transmitting channel at the end of each communication, and at least every 10 minutes during a communication, for the purpose of clearly making the source of the transmissions from the station known to those receiving the transmissions.
// https://www.ecfr.gov/current/title-47/chapter-I/subchapter-D/part-97/subpart-B/section-97.119
#define CALLSIGN "KK4PDM" // Replace with your callsign

//*********************************************************************************************
//Auto Transmission Control - dials down transmit power to save battery
//Usually you do not need to always transmit at max output power
//By reducing TX power even a little you save a significant amount of battery power
//This setting enables this gateway to work with remote nodes that have ATC enabled to
//dial their power down to only the required level
// #define ENABLE_ATC    //comment out this line to disable AUTO TRANSMISSION CONTROL
//*********************************************************************************************
#define SERIAL_BAUD   9600

#ifdef ENABLE_ATC
  RFM69_ATC radio;
#else
  RFM69 radio;
#endif

// SPIFlash flash(SS_FLASHMEM, 0xEF30); //EF30 for 4mbit  Windbond chip (W25X40CL)
bool spy = false; //set to 'true' to sniff all packets on the same network

void setup() {
  Serial.begin(SERIAL_BAUD);
  Serial.println("Starting up");

  delay(10);
  uint32_t timeout = 3000;
  uint32_t start = millis();
  while (!radio.initialize(FREQUENCY,NODEID,NETWORKID) && millis()-start < timeout); // wait for radio to initialize
  if (millis()-start >= timeout)
  {
    Serial.println("RFM69 radio init failed");
    radio.readAllRegs();
    delay(20000);
    exit(1);
  }
  radio.readAllRegs();
  // if (!radio.initialize(FREQUENCY,NODEID,NETWORKID)){
  //   Serial.println("RFM69 radio init failed");
  //   radio.readAllRegs();
  //   delay(20000);
  //   exit(1);
  // }

  radio.setHighPower(); //must include this only for RFM69HW/HCW!

#ifdef ENCRYPTKEY
  radio.encrypt(ENCRYPTKEY);
#endif

  radio.spyMode(spy);

#ifdef FREQUENCY_EXACT
  radio.setFrequency(FREQUENCY_EXACT); //set frequency to some custom frequency
#endif

  char buff[50];
  sprintf(buff, "\nListening at %d Mhz...", FREQUENCY==RF69_433MHZ ? 433 : FREQUENCY==RF69_868MHZ ? 868 : 915);
  Serial.println(buff);
  
#ifdef ENABLE_ATC
  Serial.println("RFM69_ATC Enabled (Auto Transmission Control)");
#endif

  Serial.print("Gateway ");
  Serial.print(NODEID,DEC);
  Serial.println(" ready");
}

byte ackCount=0;
uint32_t packetCount = 0;
void loop() {
  //process any serial input
  if (Serial.available() > 0)
  {
    char input = Serial.read();
    if (input == 'r') //d=dump all register values
      radio.readAllRegs();

    if (input == 'p')
    {
      spy = !spy;
      radio.spyMode(spy);
      Serial.print("SpyMode mode ");Serial.println(spy ? "on" : "off");
    }
    
    if (input == 't')
    {
      byte temperature =  radio.readTemperature(-1); // -1 = user cal factor, adjust for correct ambient
      byte fTemp = 1.8 * temperature + 32; // 9/5=1.8
      Serial.print( "Radio Temp is ");
      Serial.print(temperature);
      Serial.print("C, ");
      Serial.print(fTemp); //converting to F loses some resolution, obvious when C is on edge between 2 values (ie 26C=78F, 27C=80F)
      Serial.println('F');
    }
  }

  if (radio.receiveDone())
  {
    Serial.print("#[");
    Serial.print(++packetCount);
    Serial.print(']');
    Serial.print('[');Serial.print(radio.SENDERID, DEC);Serial.print("] ");
    if (spy) Serial.print("to [");Serial.print(radio.TARGETID, DEC);Serial.print("] ");
    for (byte i = 0; i < radio.DATALEN; i++)
      Serial.print((char)radio.DATA[i]);
    Serial.print("   [RX_RSSI:");Serial.print(radio.RSSI);Serial.print("]");
    
    if (radio.ACKRequested())
    {
      byte theNodeID = radio.SENDERID;
      radio.sendACK();
      Serial.print(" - ACK sent.");

      // When a node requests an ACK, respond to the ACK
      // and also send a packet requesting an ACK (every 3rd one only)
      // This way both TX/RX NODE functions are tested on 1 end at the GATEWAY
      if (ackCount++%3==0)
      {
        Serial.print(" Pinging node ");
        Serial.print(theNodeID);
        Serial.print(" - ACK...");
        delay(3); //need this when sending right after reception .. ?
        
        String msg1 = "ACK TEST ";
        String msg = msg1 + CALLSIGN;
        if (radio.sendWithRetry(theNodeID, msg.c_str(), 8, 0))  // 0 = only 1 attempt, no retries
          Serial.print("ok!");
        else Serial.print("nothing");
      }
    }
    Serial.println();
    Blink(LED_BUILTIN,3);
  }
}

void Blink(byte PIN, int DELAY_MS)
{
  pinMode(PIN, OUTPUT);
  digitalWrite(PIN,HIGH);
  delay(DELAY_MS);
  digitalWrite(PIN,LOW);
}
