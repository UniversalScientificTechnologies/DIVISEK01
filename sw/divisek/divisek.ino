String githash = "$Id$";
/*
  Divisek
 
Compiled with: Arduino 1.8.5

ISP
---
PD0     RX
PD1     TX
RESET#  through 50M capacitor to RST#

SDcard
------
DAT3   SS   4 B4
CMD    MOSI 5 B5
DAT0   MISO 6 B6
CLK    SCK  7 B7

ANALOG
------
+      A0  PA0
-      A1  PA1
RESET  0   PB0

LED
---
LED  23  PC7         // LED for Dasa


                     Mighty 1284p    
                      +---\/---+
           (D 0) PB0 1|        |40 PA0 (AI 0 / D24)
           (D 1) PB1 2|        |39 PA1 (AI 1 / D25)
      INT2 (D 2) PB2 3|        |38 PA2 (AI 2 / D26)
       PWM (D 3) PB3 4|        |37 PA3 (AI 3 / D27)
    PWM/SS (D 4) PB4 5|        |36 PA4 (AI 4 / D28)
      MOSI (D 5) PB5 6|        |35 PA5 (AI 5 / D29)
  PWM/MISO (D 6) PB6 7|        |34 PA6 (AI 6 / D30)
   PWM/SCK (D 7) PB7 8|        |33 PA7 (AI 7 / D31)
                 RST 9|        |32 AREF
                VCC 10|        |31 GND
                GND 11|        |30 AVCC
              XTAL2 12|        |29 PC7 (D 23)
              XTAL1 13|        |28 PC6 (D 22)
      RX0 (D 8) PD0 14|        |27 PC5 (D 21) TDI
      TX0 (D 9) PD1 15|        |26 PC4 (D 20) TDO
RX1/INT0 (D 10) PD2 16|        |25 PC3 (D 19) TMS
TX1/INT1 (D 11) PD3 17|        |24 PC2 (D 18) TCK
     PWM (D 12) PD4 18|        |23 PC1 (D 17) SDA
     PWM (D 13) PD5 19|        |22 PC0 (D 16) SCL
     PWM (D 14) PD6 20|        |21 PD7 (D 15) PWM
                      +--------+
*/

#include <SD.h>             // Tested with version 1.2.2.
#include "wiring_private.h"
#include <Wire.h>           // Tested with version 1.0.0.
#include "RTClib.h"         // Tested with version 1.5.4.

#define LED       23   // PC7
#define RESET     0    // PB0
#define SDpower1  1    // PB1
#define SDpower2  2    // PB2
#define SDpower3  3    // PB3
#define SS        4    // PB4
#define MOSI      5    // PB5
#define MISO      6    // PB6
#define SCK       7    // PB7
#define INTA      24   // PA0
#define INTB      25   // PA1

uint16_t count = 0;
uint32_t serialhash = 0;

uint8_t lightning[32];
String dataString = "";
char buf[10];

RTC_Millis rtc;

void DataOut()
{
  DateTime now = rtc.now();

  uint32_t stroke, stroke_energy;

  dataString += "$STROKE,";

  dataString += String(char('A' + lightning[30] - 1)); 
  dataString += ",";

  dataString += String(count); 
  dataString += ",";

  dataString += String(now.unixtime());  // Time of discharge
  dataString += ",";

  for (int8_t n=0; n<9; n++)
  {
    sprintf(buf, "0x%02X", lightning[n]);
    dataString += buf;
    dataString += ",";
  }  

  dataString += String(lightning[3]);  // Type of discharge
  dataString += ",";

  stroke_energy = lightning[4];
  stroke = lightning[5];
  stroke_energy += stroke << 8;
  stroke = lightning[6] & 0b11111;
  stroke_energy += stroke << 16;

  dataString += String(stroke_energy);  // Energy of single stroke
  dataString += ",";

  dataString += String(lightning[7] & 0b111111);  // Distance from storm
  dataString += ",";

  dataString += "\r\n";
}

void setup()
{

  // Open serial communications and wait for port to open:
  Serial.begin(9600);
  while (!Serial) 
  {
  ; // wait for serial port to connect. Needed for Leonardo only?
  }

  Serial.println("#Cvak...");

  DDRB = 0b10011110;
  PORTB = 0b00000000;  // SDcard Power OFF

  DDRA = 0b11111100;
  PORTA = 0b00000000;  // SDcard Power OFF
  DDRC = 0b11101100;
  PORTC = 0b00000000;  // SDcard Power OFF
  DDRD = 0b11111100;
  PORTD = 0b00000000;  // SDcard Power OFF

  pinMode(LED, OUTPUT);
  digitalWrite(LED, LOW);  
  
  for(int i=0; i<5; i++)  
  {
    delay(50);
    digitalWrite(LED, HIGH);  // Blink for Dasa 
    delay(50);
    digitalWrite(LED, LOW);  
  }

  Serial.println("#Hmmm...");

  // make a string for device identification output
  String dataString = "$DIVISEK,01," + githash.substring(5,44) + ","; // FW version and Git hash
  
  Wire.beginTransmission(0x58);                   // request SN from EEPROM
  Wire.write((int)0x08); // MSB
  Wire.write((int)0x00); // LSB
  Wire.endTransmission();
  Wire.requestFrom((uint8_t)0x58, (uint8_t)16);    
  for (int8_t reg=0; reg<16; reg++)
  { 
    uint8_t serialbyte = Wire.read(); // receive a byte
    if (serialbyte<0x10) dataString += "0";
    dataString += String(serialbyte,HEX);    
    serialhash += serialbyte;
  }

  {
    DDRB = 0b10111110;
    PORTB = 0b00001111;  // SDcard Power ON
  
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
      dataFile.println(dataString);  // write to SDcard (800 ms)     
      dataFile.close();
  
      digitalWrite(LED, HIGH);  // Blink for Dasa
      Serial.println(dataString);  // print to terminal (additional 700 ms in DEBUG mode)
      digitalWrite(LED, LOW);          
    }  
    // if the file isn't open, pop up an error:
    else 
    {
      Serial.println("#error opening datalog.txt");
    }
  
    DDRB = 0b10011110;
    PORTB = 0b00000001;  // SDcard Power OFF          
  }    

}


void loop()
{
  lightning[30] = 0;
  // make a string for assembling the data to log:
  dataString = "";

  if (digitalRead(INTA))
  {
    delay(100); // minimal delay after stroke interrupt

    Wire.requestFrom((uint8_t)3, (uint8_t)8);    // request 9 bytes from slave device #3

    for (int8_t reg=0; reg<8; reg++)
    { 
      lightning[reg] = Wire.read();    // receive a byte
    }
    lightning[30] = 1;
    DataOut();
  }

  if (digitalRead(INTB))
  {
    delay(100); // minimal delay after stroke interrupt

    Wire.requestFrom((uint8_t)2, (uint8_t)8);    // request 9 bytes from slave device #3

    for (int8_t reg=0; reg<8; reg++)
    { 
      lightning[reg] = Wire.read();    // receive a byte
    }
    lightning[30] = 2;
    DataOut();
  }

  // Data out
  

  if (lightning[30])  
  {
    digitalWrite(LED, HIGH);  // Blink for Dasa
    Serial.print(dataString);  // print to terminal (additional 700 ms in DEBUG mode)
    digitalWrite(LED, LOW);          

    DDRB = 0b10111110;
    PORTB = 0b00001111;  // SDcard Power ON

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

    DDRB = 0b10011110;
    PORTB = 0b00000001;  // SDcard Power OFF

    count++;
  }          
}









