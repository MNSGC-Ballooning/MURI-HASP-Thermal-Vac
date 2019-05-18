//Libraries
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "SparkFunMPL3115A2.h"
#include <Adafruit_MAX31856.h>
#include <SD.h>
//#include <Relay_XBee.h>
#include <XBee.h>
//#include <RelayXBee.h>
#include <SoftwareSerial.h>
#include <ADC.h>

const int readPin = A0; // ADC0
const int readPin2 = A0; // ADC1

ADC *adc = new ADC(); // adc object;

// Define SoftSerial TX/RX pins
// Connect Arduino pin 8 to TX of usb-serial device
uint8_t ssRX = 7;
// Connect Arduino pin 9 to RX of usb-serial device
uint8_t ssTX = 8;
// Remember to connect all devices to a common Ground: XBee, Arduino and USB-Serial device
SoftwareSerial nss(ssRX, ssTX);
//==================================================================================
//               MURI Thermal-Vac Computer
//               Written by Jacob Meyer - meye2497 Winter Break 2018-19, Spring 2019              
//==================================================================================

/* Mega ADK pin connections:
   //Write down pin connections when layout is compeltely soldered and finalized
   //Will be helpful for future reference
   -------------------------------------------------------------------------------------------------------------------------
     Component                    | Pins used               | Notes
     
     xBee serial                  | 31, 32 (TXRX4)
     Honeywell Pressure           | // Probably Defunct, but uses same pin as a mega - A0
     MS5607 Pressure/Altimiter    | // Decide which pins are best, code still needs to be written for this new sensor
     Adafruit Thermocouple #1     | 6, 7, 8, 9
     Adafruit Thermocouple #2     | 10, 7, 8, 9
     Adafruit Thermocouple #3     | 11, 7, 8, 9
     Adafruit Thermocouple #4     | 12, 7, 8, 9
     Adafruit OLED screen         | // Not used in teensy version, but easily installable (code is still here anyway)
     Fan #1                       | // Optional due to more external power feed-throughs (but still a good option)
     Fan #2                       | // Optional due to more external power feed-throughs (but still a good option)
   -------------------------------------------------------------------------------------------------------------------------
*/

//XBee serial connection
#define XBeeSerial Serial1

//XBee network ID
//-------COORDINATE WITH OTHER TEAMS IF NECESSARY-------
const String ID = "MEYER1"; //Choose a unique 4-digit hexadecimal network ID
//----THIS MUST MATCH YOUR THERMAL-VAC COMPUTER CODE-----

//XBee object
XBee xBee = XBee();
/* //OLDER METHOD
  //Give the XBee a unique ID. Best practice is to keep it short: 2-4 characters, capital A-Z and 0-9 only
  //This can be done directly in the constructor, but making it global allows it to be used elsewhere if needed
  const String ID = "R1";
  //XBee object needs a serial line to read
  SoftwareSerial ss = SoftwareSerial(2,3);
  //Pass a reference to this serial line and chosen ID to the XBee constructor
  XBee xBee = XBee(&ss, ID);
  //Alternately, use a hard serial line if the microcontroller supports it
  //XBee xBee = XBee(&Serial1, ID);
  /1111  */
#define SCREEN_WIDTH 128 // Thermal-vac OLED display width, in pixels
#define SCREEN_HEIGHT 64 // Thermal-vac OLED display height, in pixels

#define chipSelect 4 // Adafruit Wireless SD shield only
File datalog;                     //File object for datalogging
char filename[] = "TVac00.csv"; //Template for file name to save data
bool SDactive = false;            //used to check for SD card before attempting to log

/*
Adafruit_MAX31856 maxthermo1 = Adafruit_MAX31856(5, 6, 7, 8);
Adafruit_MAX31856 maxthermo2 = Adafruit_MAX31856(22, 24, 26, 28);
Adafruit_MAX31856 maxthermo3 = Adafruit_MAX31856(23, 25, 27, 29);
Adafruit_MAX31856 maxthermo4 = Adafruit_MAX31856(30, 31, 32, 33);
*/

//Template 1: Adafruit_MAX31856::Adafruit_MAX31856(int8_t spi_cs)
//Template 2: Adafruit_MAX31856::Adafruit_MAX31856(int8_t spi_cs, int8_t spi_mosi, int8_t spi_miso, int8_t spi_clk) 
//2: or Constructor(CS, SDI, SDO, SCK)
Adafruit_MAX31856 maxthermo1 = Adafruit_MAX31856(6, 7, 8, 9);
Adafruit_MAX31856 maxthermo2 = Adafruit_MAX31856(10, 7, 8, 9);
Adafruit_MAX31856 maxthermo3 = Adafruit_MAX31856(11, 7, 8, 9);
Adafruit_MAX31856 maxthermo4 = Adafruit_MAX31856(12, 7, 8, 9);

// Global Variables
// For Honeywell Analog Pressure Sensor:
int pressureSensor = 0;
float pressureSensorV = 0;
float psi = 0;
float atm = 0;
// For data collection:
long count = 0; // Test OR Counter Variable
long count2 = 0; // Second Counter Variable
String L0 = "Pressure Temp Hum.  T", L1 = "", L2 = "", L3 = "", L4 = "", L5 = "", L6 = "", L7 = "";
unsigned long prevTime = 0;
bool TakeData = false; // Boolean to start or stop data collection
bool SecS = false; // Second boolean to start or stop data collection

class Relay {
  protected:
    bool isOpen;
    int onPin;
    int offPin;
  public:
    Relay(int on, int off);
    const char* getRelayStatus();
    void init();
    void openRelay();
    void closeRelay();
};

//Relay class functions
Relay::Relay(int on,int off)
  : onPin(on)
  , offPin(off)
  , isOpen(false)
  {}
const char* Relay::getRelayStatus(){
  const char _open[] = "ON";
  const char _closed[] = "CLOSED";
  if (isOpen){
    return (_open);
  }
  else {
    return (_closed);
  }
}
void Relay::init(){
  pinMode(onPin,OUTPUT);
  pinMode(offPin,OUTPUT);
}
void Relay::openRelay(){
  isOpen = true;
  digitalWrite(onPin,HIGH);
  delay(10);
  digitalWrite(onPin,LOW);
}
void Relay::closeRelay(){
  isOpen = false;
  digitalWrite(offPin,HIGH);
  delay(10);
  digitalWrite(offPin,LOW);
}

//Fans

Relay F1 = Relay(23, 22);
Relay F2 = Relay(21, 20);


void setup() {
// Fan Initializations
   //Fan 1
   F1.init();
   F1.openRelay();
   delay(2000);
   F1.closeRelay();
   //Fan 2
   F2.init();
   F2.openRelay();
   delay(2000);
   F2.closeRelay();
   Serial.begin(9600);
   Serial1.begin(9600);
  Serial4.begin(9600);
  //XBee setup
  xBee.begin(Serial4);
  // start soft serial
  nss.begin(9600);
  
 // When powered on, XBee radios require a few seconds to start up
 // and join the network.
 // During this time, any packets sent to the radio are ignored.
 // Series 2 radios send a modem status packet on startup.
 
 // it took about 4 seconds for mine to return modem status.
 // In my experience, series 1 radios take a bit longer to associate.
 // Of course if the radio has been powered on for some time before the sketch runs,
 // you can safely remove this delay.
 // Or if you both commands are not successful, try increasing the delay.
 
 delay(5000);

  //SD-card Setup
  pinMode(10, OUTPUT);      //Needed for SD library, regardless of shield used
  pinMode(chipSelect, OUTPUT);
  Serial4.print("Initializing SD card...");
  if (!SD.begin(chipSelect)) {                            //attempt to start SD communication
    Serial4.println(" Card failed, or not present");          //print out error if failed; remind user to check card
  }
  else {                                                    //if successful, attempt to create file
    Serial4.println("Card initialized.\nCreating File...");
    for (byte i = 0; i < 100; i++) {                        //can create up to 100 files with similar names, but numbered differently
      filename[4] = '0' + i / 10;
      filename[5] = '0' + i % 10;
      if (!SD.exists(filename)) {                           //if a given filename doesn't exist, it's available
        datalog = SD.open(filename, FILE_WRITE);            //create file with that name
        SDactive = true;                                    //activate SD logging since file creation was successful
        Serial4.println("Logging to: " + String(filename));  //Tell user which file contains the data for this run of the program
        break;                                              //Exit the for loop now that we have a file
      }
    }
    if (!SDactive) Serial1.println("No available file names; clear SD card to enable logging");
  }
  String header = "PR,    T1, T2, T3, T4, DP, Sec";  //setup data format, and print it to the monitor and SD card
  Serial4.println(header);
  if (SDactive) {
    datalog.println(header);
    datalog.close();
  }
  
  // Setup Adafruit MAX_31856 Thermocouples #1-4
  SPI.begin();
  maxthermo1.begin();
  maxthermo2.begin();
  maxthermo3.begin();
  maxthermo4.begin();
  maxthermo1.setThermocoupleType(MAX31856_TCTYPE_K);
  maxthermo2.setThermocoupleType(MAX31856_TCTYPE_K);
  maxthermo3.setThermocoupleType(MAX31856_TCTYPE_K);
  maxthermo4.setThermocoupleType(MAX31856_TCTYPE_K);

      pinMode(LED_BUILTIN, OUTPUT);
    pinMode(readPin, INPUT);
    pinMode(readPin2, INPUT);

    pinMode(A10, INPUT); //Diff Channel 0 Positive
    pinMode(A11, INPUT); //Diff Channel 0 Negative
    #if ADC_NUM_ADCS>1
    pinMode(A12, INPUT); //Diff Channel 3 Positive
    pinMode(A13, INPUT); //Diff Channel 3 Negative
    #endif

    ///// ADC0 ////
    // reference can be ADC_REFERENCE::REF_3V3, ADC_REFERENCE::REF_1V2 (not for Teensy LC) or ADC_REFERENCE::REF_EXT.
    //adc->setReference(ADC_REFERENCE::REF_1V2, ADC_0); // change all 3.3 to 1.2 if you change the reference to 1V2

    adc->setAveraging(16); // set number of averages
    adc->setResolution(16); // set bits of resolution

    // it can be any of the ADC_CONVERSION_SPEED enum: VERY_LOW_SPEED, LOW_SPEED, MED_SPEED, HIGH_SPEED_16BITS, HIGH_SPEED or VERY_HIGH_SPEED
    // see the documentation for more information
    // additionally the conversion speed can also be ADACK_2_4, ADACK_4_0, ADACK_5_2 and ADACK_6_2,
    // where the numbers are the frequency of the ADC clock in MHz and are independent on the bus speed.
    adc->setConversionSpeed(ADC_CONVERSION_SPEED::VERY_LOW_SPEED); // change the conversion speed
    // it can be any of the ADC_MED_SPEED enum: VERY_LOW_SPEED, LOW_SPEED, MED_SPEED, HIGH_SPEED or VERY_HIGH_SPEED
    adc->setSamplingSpeed(ADC_SAMPLING_SPEED::MED_SPEED); // change the sampling speed

    // always call the compare functions after changing the resolution!
    //adc->enableCompare(1.0/3.3*adc->getMaxValue(ADC_0), 0, ADC_0); // measurement will be ready if value < 1.0V
    //adc->enableCompareRange(1.0*adc->getMaxValue(ADC_0)/3.3, 2.0*adc->getMaxValue(ADC_0)/3.3, 0, 1, ADC_0); // ready if value lies out of [1.0,2.0] V

    // If you enable interrupts, notice that the isr will read the result, so that isComplete() will return false (most of the time)
    //adc->enableInterrupts(ADC_0);


    ////// ADC1 /////
    #if ADC_NUM_ADCS>1
    adc->setAveraging(16, ADC_1); // set number of averages
    adc->setResolution(16, ADC_1); // set bits of resolution
    adc->setConversionSpeed(ADC_CONVERSION_SPEED::MED_SPEED, ADC_1); // change the conversion speed
    adc->setSamplingSpeed(ADC_SAMPLING_SPEED::MED_SPEED, ADC_1); // change the sampling speed

    //adc->setReference(ADC_REFERENCE::REF_1V2, ADC_1);

    // always call the compare functions after changing the resolution!
    //adc->enableCompare(1.0/3.3*adc->getMaxValue(ADC_1), 0, ADC_1); // measurement will be ready if value < 1.0V
    //adc->enableCompareRange(1.0*adc->getMaxValue(ADC_1)/3.3, 2.0*adc->getMaxValue(ADC_1)/3.3, 0, 1, ADC_1); // ready if value lies out of [1.0,2.0] V


    // If you enable interrupts, note that the isr will read the result, so that isComplete() will return false (most of the time)
    //adc->enableInterrupts(ADC_1);

    #endif

  // Project Title and Author
  Serial4.println("Thermal-Vac Chamber");
  Serial4.println("By Jacob Meyer");
  Serial4.println("12/31/2018 12:31PM");
}
// Decide whether to use the thermocouples with SPI or I2C
void loop() {
  Serial.println("Hey");
  if (millis() - prevTime >= 1000) // Almost precisely makes a one second interval (as opposed to using "delay()")
  {
    prevTime = millis();
//    print(millis()): Debugging
    // Always check for incoming commands
    String command = Serial4.readStringUntil('\n');
    // Pressure Sensor
    pressureSensor = adc->analogRead(A0);                // Read the analog pin
    pressureSensorV = pressureSensor * (5.0 / 1024); // Convert the digital number to voltage
    psi = (pressureSensorV - (0.1 * 5.0)) / (4.0 / 15.0); // Convert the voltage to proper units
    atm = psi / 14.696; // Convert psi to atm
    String atmSTR = String(atm, 3); // Converts Pressure to a string, Fixes to 3 decimal places so it takes up less space on the OLED screen and gives us the desired sig figs
    // Adafruit MAX_31856 Thermocouples 1-4 temperatures
    double T1 = maxthermo1.readThermocoupleTemperature();
    double T2 = maxthermo2.readThermocoupleTemperature();
    double T3 = maxthermo3.readThermocoupleTemperature();
    double T4 = maxthermo4.readThermocoupleTemperature();
    //Converting the temperature doubles to ints takes up less space on the OLED screen
    int T1int = T1;
    int T2int = T2;
    int T3int = T3;
    int T4int = T4;
 //   print(millis()): Debugging
    // XBee always checks for commands
    CheckXBee(atmSTR, TakeData, command);

    // Functions to relay sensor info to certain devices
 //   print(millis()) Debugging
    serial(atmSTR, T1int, T2int, T3int, T4int, TakeData, command);
    SDLogging(atmSTR, T1int, T2int, T3int, T4int, TakeData, command);
    
    if (SecS == true)
    {
      count++;
    }
    count2++; 
   // Misc(atm, T4); //Probably a waste of runtime - only include if something autonomous is specifically needed
  }

}

void serial(String P, double T1, double T2, double T3, double T4, bool &TakeData, String command) {
  if (TakeData == true && count == 0)
  {
    // Header of the data collection, one onboard Honeywell pressure sensor, four Adafruit MAX 31856 Thermocouples, a timestamp,
    // manual (user-imput) pressure from a non-logging pressure sensor, and an area for user-input notes
    Serial4.println("PRESSURE (ATM)    T1   T2   T3   T4   Sec   INPUT-PRESSURE   NOTES");
    SecS = true;
  }
  if(command[0] == '<' && command[1] == 'P' && command[2] == '>')
  {
    String temp = command.substring(3); // Returns everything after index 2
    if (TakeData == true && count > 0)
    {
      Serial4.println(P + " " + T1 + " " + T2 + " " + T3 + " " + T4 + " " + String(count) + " " + temp);
    }
    else if (TakeData == false)
    {
      Serial4.println(P + " " + T1 + " " + T2 + " " + T3 + " " + T4 + " " + temp);
    }
  }
  else if(command[0] == '<' && command[1] == 'N' && command[2] == '>')
  {
    String temp = command.substring(3); // Returns everything after index 2
    if (TakeData == true && count > 0)
    {
      Serial4.println(P + " " + T1 + " " + T2 + " " + T3 + " " + T4 + " " + String(count) + " " + temp);
    }
    else if (TakeData == false)
    {
      Serial4.println(P + " " + T1 + " " + T2 + " " + T3 + " " + T4 + " " + temp);
    }
  }
  else
  {
    if (TakeData == true && count > 0)
    {
      Serial4.println(P + " " + T1 + " " + T2 + " " + T3 + " " + T4 + " " + String(count));
    }
    else if (TakeData == false)
    {
      Serial4.println(P + " " + T1 + " " + T2 + " " + T3 + " " + T4);
    }
  }
}

void SDLogging(String P, int T1, int T2, int T3, int T4, bool &TakeData, String command) {
  String tempIP = "";
  String tempNO = "";
  if(command[0] == '<' && command[2] == '>')
  {
    if(command[1] == 'P')
    {
      tempIP = command.substring(3);
    }
    else if(command[1] == 'N')
    {
      tempNO = command.substring(3);
    }
    else
    {
      Serial4.println("Command does not exist");
    }
  }
  if (SDactive) {
    datalog = SD.open(filename, FILE_WRITE);
    datalog.println(P + ", " + T1 + ", " + T2 + ", " + T3 + ", " + T4 + ", " + String(count) + ", " + String(millis()/1000) + ", " + tempIP + ", " + tempNO);
    datalog.close();                //close file afterward to ensure data is saved properly
  }
}

void CheckXBee(String P, bool &TakeData, String command) { 
  // Can just make "TakeData" global if it doesn't work this way
  //all possible commands go in a series of if-else statements, where the correct action is taken in each case
  if (command == "")
  {
    return; // Try without this later
  }
  else if (command == "HI") {
    Serial4.println("Hello");
  }
  else if (command == "NAME") {
    Serial4.println(ID);
  }
  else if (command == "BEGIN") {
    TakeData = true;
    Serial4.println(" Beginning data collection");
  }
  else if (command == "STOP") {
    TakeData = false;
    Serial4.println(" Data collection stopped.");
  }
  else if (command == "HEATON")
  {
    // Turn the heaters on
  }
  else if (command == "HEATOFF")
  {
    // Turn the heaters off
  }
  else if (command == "FAN1ON")
  {
    // Turn Fan 1 on
    F1.openRelay();
   // XBeeSerial.println("ON");
  }
  else if (command == "FAN1OFF")
  {
    // Turn Fan 1 off
    F1.closeRelay();
    XBeeSerial.println("OFF");
  }
  else if (command == "FAN2ON")
  {
    // Turn Fan 2 on
    F2.openRelay();
    Serial4.println(command);
  }
  else if (command == "FAN2OFF")
  {
    // Turn Fan 2 off
    F2.closeRelay();
    Serial4.println(command);
  }
}

void Misc(float atm, double T4) {
  // Start with code for battery heater; assign thermocouple # 4 to battery
  if (T4 < 10)
  {
//    R.openRelay(); !!Discontinued!!
    // Turn the heater on
  }
  else if (T4 > 25)
  {
//    R.closeRelay();  !!Discontinued!!
    // Shut the heater off 
  }
  // Code for the fans
  if (atm < 0.90)
  {
    // Turn the fans on
  }
  else
  {
    // Keep the fans off
  }
}
