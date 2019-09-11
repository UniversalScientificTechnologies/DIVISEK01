String githash = "$Id: 4c0c30535b9db4f980f154b70911e5b2a320fb20 $";
/*
 * https://github.com/adafruit/Adafruit_MPL3115A2_Library
 */


#include <Adafruit_MPL3115A2.h>
#include <SD.h>             // Tested with version 1.2.2.
#include "wiring_private.h"
#include <Wire.h>           // Tested with version 1.0.0.
#include "RTClib.h"         // Tested with version 1.5.4.

#define LED       23   // PC7
#define INTA      24   // PA0

unsigned long lastRead = 0;
uint16_t count = 0;
uint16_t countl = 0;
uint32_t serialhash = 0;
uint8_t lightning[32];
String dataString = "";
char buf[10];
boolean event = false;

Adafruit_MPL3115A2 sensor = Adafruit_MPL3115A2();
RTC_Millis rtc;

void DataOut()
{
  uint32_t stroke, stroke_energy;
  
  DateTime now = rtc.now();

  dataString += "$STROKE,";

  dataString += String(countl++); 
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
  dataString = "$DIVISEK,MPL," + githash.substring(5,44) + ","; // FW version and Git hash
  
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
  sensor.begin();
  dataString = "";

  //Wire.setClock(1000000);

  const int8_t CAP = 1; 

  // LCO calibration, Antenna Tuning, see manual page 35
  Wire.beginTransmission(3); // transmit to device #3
  Wire.write(8);             // DISP_LCO [7]  
  Wire.write(0x80 | CAP);       
  Wire.endTransmission();    // stop transmitting
  delay(10000);
  Wire.beginTransmission(3); // transmit to device #3
  Wire.write(8);             // Stop DISP_LCO  
  Wire.write(CAP);      
  Wire.endTransmission();    // stop transmitting

  // SRCO calibration, see manual page 36
  Wire.beginTransmission(3); // transmit to device #3
  Wire.write(0x3D);          // CALIB_RCO
  Wire.endTransmission();    // stop transmitting
  Wire.beginTransmission(3); // transmit to device #3
  Wire.write(8);             // DISP_SRCO [6] 
  Wire.write(0x7 | CAP);       
  Wire.endTransmission();    // stop transmitting
  delay(2);
  Wire.beginTransmission(3); // transmit to device #3
  Wire.write(8);             // Stop DISP_SRCO  
  Wire.write(CAP);      
  Wire.endTransmission();    // stop transmitting
}


int8_t counter = 0;

void loop() 
{
  if (digitalRead(INTA))
  {
    delay(2); // minimal delay after stroke interrupt

    Serial.print(counter++);
    Serial.println('*');

    Wire.requestFrom((uint8_t)3, (uint8_t)9);    // request 9 bytes from slave device #3

    for (int8_t reg=0; reg<9; reg++)
    { 
      lightning[reg] = Wire.read();    // receive a byte
    }
    //event = true;
    DataOut();
  }

  if(millis() - lastRead >= 10000) 
  {
    DateTime now = rtc.now();

    // make a string for assembling the data to log:
    dataString += "$MPL,";

    dataString += String(count++); 
    dataString += ",";
    dataString += String(now.unixtime());  // Time of measurement
    dataString += ",";

    float pressure = sensor.getPressure();
    dataString += String(pressure); 
    dataString += ",";

    float temperature = sensor.getTemperature();
    dataString += String(temperature); 
    dataString += "\r\n";
 
    event = true;    
    lastRead = millis();
  }

  if(event)
  {
    digitalWrite(LED, HIGH);  // Blink for Dasa
    Serial.print(dataString);  // print to terminal (additional 700 ms in DEBUG mode)
    digitalWrite(LED, LOW);  
/*
    dataString = "";
    event = false;
  }        

  if(false)
  {
//*/
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

    dataString = "";
    event = false;

  
  }

}
