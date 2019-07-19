/*
* Readout data from electric field mill
*/
 
#include <SD.h>             // Tested with version 1.2.2.
#include "wiring_private.h"
#include <Wire.h>           // Tested with version 1.0.0.
#include "src/RTCx/RTCx.h"  // Modified version

#define LED  23   // PC7
#define PHASE1  21  
#define PHASE2  22
#define SS          4    // PB4

const int analogInPin = A0;  // Analog input pin 
uint16_t sensorValue = 0;    // value read from the pot
int16_t sign;
int16_t value;
String dataString = "";
uint16_t count = 0;
struct RTCx::tm tm;

void setup() {
  // initialize serial communications at 9600 bps:
  Serial.begin(9600);
  pinMode(PHASE1, INPUT);
  pinMode(PHASE2, INPUT);
  pinMode(LED, OUTPUT);

  // Initiating RTC
  rtc.autoprobe();
  rtc.resetClock();
}


void loop() 
{
  dataString = "";

  for(uint8_t n; n<100; n++)
  {
    dataString += "$M,";
    dataString += String(count++); 
    dataString += ",";
    while (!digitalRead(PHASE1));
    while (digitalRead(PHASE1));
    if (digitalRead(PHASE2))
    {
      sign = 1;
    }
    else
    {
      sign = -1;
    }
    
    // read the analog in value:
    sensorValue = analogRead(analogInPin);
  
    value = sensorValue * sign;
    
    // print the results to the Serial Monitor:
    Serial.println(value);
    dataString += String(value); 
    dataString += "\r\n";
  
    // wait before the next loop for the analog-to-digital
    // converter to settle after the last reading:
    delay(100);
  }

  // Write to SDcard
  {
    rtc.readClock(tm);
    RTCx::time_t t = RTCx::mktime(&tm);

    // make a string for assembling the data to log:
    dataString += "$TIME,";

    dataString += String(t-946684800);  // Time of measurement
    dataString += "\r\n";


    DDRB = 0b10111110;
    PORTB = 0b00001111;  // SDcard Power ON
    digitalWrite(LED, HIGH);  // Blink for Dasa

    // make sure that the default chip select pin is set to output
    // see if the card is present and can be initialized:
    if (!SD.begin(SS)) 
    {
      Serial.println("#Card failed, or not present");
      // don't do anything more:
      return;
    }

    // open the file. note that only one file can be open at a time,
    // so you have to close this one before opening another.
    File dataFile = SD.open("datalog.txt", FILE_WRITE);
  
    // if the file is available, write to it:
    if (dataFile) 
    {
      dataFile.print(dataString);  // write to SDcard (800 ms)     
      dataFile.close();
    }  
    // if the file isn't open, pop up an error:
    else 
    {
      Serial.println("#error opening datalog.txt");
    }
    digitalWrite(LED, LOW);          
    DDRB = 0b10011110;
    PORTB = 0b00000001;  // SDcard Power OFF  
    
  }
}
