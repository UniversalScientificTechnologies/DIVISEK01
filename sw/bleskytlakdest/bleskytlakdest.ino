String githash = "$Id: 4c0c30535b9db4f980f154b70911e5b2a320fb20 $";
/*
 * https://github.com/adafruit/Adafruit_MPL3115A2_Library
 * 
 * Todo:
 *  - include MK time library from AIRDOS_F, here the time will skew
 *  - make it work on rechargeable Li-Ion cells 
 */

#define DEBUG
#define SD_ENABLE
#define DIVISEK_ENABLE
//!!!#define GPS_ENABLE

#include <Adafruit_MPL3115A2.h>
// SD MightyCore 1.0.7
// SD by Arduino, Sparkfun 1.2.2

#include <SD.h>             // Tested with version 1.2.2.
#include "wiring_private.h"
#include <Wire.h>           // Tested with version 1.0.0.
#include "src/RTCx/RTCx.h"  // Modified version

#define LED       23   // PC7
#define LED_OUT   10   // TX1/PD2
#define INTA      24   // PA0
#define INTP      11   // TX1 - Precipitation meter interrupt pin

#define GPSpower  26   // PA2
#define GPSerror 70000 // number of cycles for waiting for GPS in case of GPS error 
#define GPSdelay 50    // number of measurements between obtaining GPS position

unsigned long lastRead;
uint16_t count = 0;
uint16_t countl = 0;      // Lightning counter
uint32_t serialhash = 0;
uint8_t lightning[32];
String dataString = "";
char buf[10];
boolean event = false;
struct RTCx::tm tm;

uint8_t prec_count = 0; // Precipitation meter counter

Adafruit_MPL3115A2 sensor = Adafruit_MPL3115A2();

unsigned long lastRain = 0;

void gpsMessages() {
  // make a string for assembling the data to log:

  #define MSG_NO 10    // number of logged NMEA messages
           
  // flush serial buffer
  while (Serial.available()) Serial.read();
  
  boolean flag = false;
  char incomingByte; 
  int messages = 0;
  uint32_t nomessages = 0;
  uint8_t parsing = 0;

  // make a string for assembling the NMEA to log:
  flag = false;
  messages = 0;
  nomessages = 0;

  while(true)
  {
    if (Serial.available()) 
    {
      // read the incoming byte:
      incomingByte = Serial.read();
      nomessages = 0;
      
      if (incomingByte == '$') 
      {
        flag = true; 
      };
      if (flag && (incomingByte == '\n')) 
      {
        messages++;
      };
      if (messages > MSG_NO) 
      {
        rtc.readClock(tm);
        RTCx::time_t t = RTCx::mktime(&tm);
      
        dataString += "\n$TIME,";
        dataString += String(t-946684800);  // Time of discharge
        dataString += "\r\n";

        break;
      }
      
      // say what you got:
      if (flag && (messages<=MSG_NO)) dataString+=incomingByte;
    }
    else
    {
      nomessages++;  
      if (nomessages > GPSerror) break; // preventing of forever waiting
    }
  }
}

void setup_GPS() {
  digitalWrite(GPSpower, HIGH); // GPS Power ON
  delay(100);
  {
    // Switch off Galileo and GLONASS; 68 configuration bytes
    const char cmd[0x3C + 8]={0xB5, 0x62, 0x06, 0x3E, 0x3C, 0x00, 0x00, 0x20, 0x20, 0x07, 0x00, 0x08, 0x10, 0x00, 0x01, 0x00, 0x01, 0x01, 0x01, 0x01, 0x03, 0x00, 0x01, 0x00, 0x01, 0x01, 0x02, 0x04, 0x08, 0x00, 0x00, 0x00, 0x01, 0x01, 0x03, 0x08, 0x10, 0x00, 0x00, 0x00, 0x01, 0x01, 0x04, 0x00, 0x08, 0x00, 0x00, 0x00, 0x01, 0x03, 0x05, 0x00, 0x03, 0x00, 0x00, 0x00, 0x01, 0x05, 0x06, 0x08, 0x0E, 0x00, 0x00, 0x00, 0x01, 0x01, 0x53, 0x1F};
    for (int n=0;n<(0x3C + 8);n++) Serial.write(cmd[n]); 
  } 
  /*
  {
    // airborne <2g; 44 configuration bytes
    const char cmd[0x24 + 8]={0xB5, 0x62 ,0x06 ,0x24 ,0x24 ,0x00 ,0xFF ,0xFF ,0x07 ,0x03 ,0x00 ,0x00 ,0x00 ,0x00 ,0x10 ,0x27 , 0x00 ,0x00 ,0x05 ,0x00 ,0xFA ,0x00 ,0xFA ,0x00 ,0x64 ,0x00 ,0x5E ,0x01 ,0x00 ,0x3C ,0x00 ,0x00 , 0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x85 ,0x2A};
    for (int n=0;n<(0x24 + 8);n++) Serial.write(cmd[n]); 
  }
  */
}

void disable_GPS() {
    digitalWrite(GPSpower, LOW); // GPS Power OFF
}

void rain() {
  if (millis() - lastRain >= 100)
  {
    prec_count++;
    lastRain = millis();
  }
}

void DataOut()
{
  uint32_t stroke, stroke_energy;
  
  rtc.readClock(tm);
  RTCx::time_t t = RTCx::mktime(&tm);

  dataString += "$STROKE,";

  dataString += String(countl++); 
  dataString += ",";

  dataString += String(t-946684800);  // Time of discharge
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

  // Initiating RTC
  rtc.autoprobe();
  rtc.resetClock();

  DDRB = 0b10011110;
  PORTB = 0b00000000;  // SDcard Power OFF

  DDRA = 0b11111100;
  PORTA = 0b00000000;  // SDcard Power OFF
  DDRC = 0b11101100;
  PORTC = 0b00000000;  // SDcard Power OFF
  DDRD = 0b11111100;
  PORTD = 0b00000000;  // SDcard Power OFF

  pinMode(LED, OUTPUT);
  pinMode(LED_OUT, OUTPUT);
  digitalWrite(LED, LOW);  
  digitalWrite(LED_OUT, LOW);  
  
  for(int i=0; i<5; i++)  
  {
    delay(50);
    digitalWrite(LED, HIGH);  // Blink for Dasa 
    digitalWrite(LED_OUT, HIGH);  // Blink for Marek 
    delay(50);
    digitalWrite(LED, LOW);  
    digitalWrite(LED_OUT, LOW);  
  }

  setup_GPS();

  Serial.println("#Hmmm...");

  // make a string for device identification output
  dataString = "$DIVISEK,MPL-RAIN-GPS," + githash.substring(5,44) + ","; // FW version and Git hash
  
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
  
  digitalWrite(LED, HIGH);  // Blink for Dasa
  digitalWrite(LED_OUT, HIGH);  // Blink for Marek
  Serial.println(dataString);  // print to terminal (additional 700 ms in DEBUG mode)
  digitalWrite(LED, LOW);          
  digitalWrite(LED_OUT, LOW);          

  
#ifdef SD_ENABLE
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
    }  
    // if the file isn't open, pop up an error:
    else 
    {
      Serial.println("#error opening datalog.txt");
    }
  
    DDRB = 0b10011110;
    PORTB = 0b00000001;  // SDcard Power OFF          
  }
#else
  Serial.println("NOT enabling write to SD card.");
#endif

  sensor.begin();
  dataString = "";

  //Wire.setClock(1000000);

  const int8_t CAP = 1; 

#ifdef DIVISEK_ENABLE
  // LCO calibration, Antenna Tuning, see manual page 35
  Wire.beginTransmission(3); // transmit to device #3
  Wire.write(8);             // DISP_LCO [7]  
  Wire.write(0x80 | CAP);       
  Wire.endTransmission();    // stop transmitting
  delay(10000); //**************** Right displayed frequency must be from 30.16 kHz to 32.34 kHz (center 31.25 kHz)
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

  // Indoor/Outdoor environment
  Wire.beginTransmission(3); // transmit to device #3
  Wire.write(0);             // AFE_GB [5:1]  
  //Wire.write(0b00100100);    // Indoor 
  Wire.write(0b00011100);    // Outdoor 
  Wire.endTransmission();    // stop transmitting

  // Threshold
  Wire.beginTransmission(3); // transmit to device #3
  Wire.write(1);             // NF_LEV [6:4], WDTH [3:0]  
  Wire.write(0b00100010);    // Default 
  Wire.endTransmission();    // stop transmitting

#endif

  // RAIN: Add hook for precipitation device interrupt
  lastRain = millis();
  lastRead = millis();
  
  pinMode(INTP, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(INTP), rain, RISING);
  interrupts();
}


int8_t counter = 0;

void loop() 
{
#ifdef DIVISEK_ENABLE
  if (digitalRead(INTA))
  {
    delay(2); // minimal delay after stroke interrupt

    counter++;
    //Serial.print(counter);
    //Serial.println('*');

    Wire.requestFrom((uint8_t)3, (uint8_t)9);    // request 9 bytes from slave device #3

    for (int8_t reg=0; reg<9; reg++)
    { 
      lightning[reg] = Wire.read();    // receive a byte
    }
    DataOut();
  }
#endif

  if(millis() - lastRead >= 10000) 
  {
    rtc.readClock(tm);
    RTCx::time_t t = RTCx::mktime(&tm);
    lastRead = millis();

    // make a string for assembling the data to log:
    dataString += "$MPL,";

    dataString += String(count++); 
    dataString += ",";
    dataString += String(t-946684800);  // Time of measurement
    dataString += ",";

    dataString += String(prec_count);
    prec_count = 0; // Reset precipitation after 10 seconds
    dataString += ",";
    /*!!!!!!!!!!!!!!!!!
    float pressure = sensor.getPressure();
    dataString += String(pressure); 
    dataString += ",";

    float temperature = sensor.getTemperature();
    dataString += String(temperature); 
    */
    dataString += "\r\n";

    
#ifdef GPS_ENABLE
    gpsMessages();
#endif
 
    digitalWrite(LED, HIGH);  // Blink for Dasa
    digitalWrite(LED_OUT, HIGH);  // Blink for Marek
    Serial.print(dataString);  // print to terminal (additional 700 ms in DEBUG mode)
    digitalWrite(LED, LOW);  
    digitalWrite(LED_OUT, LOW);  

#ifdef SD_ENABLE
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
    }
#endif
  dataString = "";
  }
}
