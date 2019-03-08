//Libraries
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "SparkFunMPL3115A2.h"
#include <Adafruit_MAX31856.h>
#include <SD.h>
//#include <Relay_XBee.h> /1111
#include <XBee.h>
#include <RelayXBee.h>

//======================================================================
//               MURI Thermal-Vac Computer
//               Written by Jacob Meyer - meye2497 Winter Break 2018-19, Spring 2019              
//======================================================================

/* Mega ADK pin connections:
   //Write down pin connections when layout is compeltely soldered and finalized
   //Will be helpful for future reference
   -------------------------------------------------------------------------------------------------------------------------
     Component                    | Pins used             | Notes
     
     xBee serial                  |
     Honeywell Pressure           |
     Adafruit Thermocouple #1     |
     Adafruit Thermocouple #2     |
     Adafruit Thermocouple #3     |
     Adafruit Thermocouple #4     |
     Adafruit OLED screen         |
     Battery heater #1            |
     Battery heater #2            |
     Fan #1                       |
     Fan #2                       |
   -------------------------------------------------------------------------------------------------------------------------
*/

//XBee serial connection
#define XBeeSerial Serial1

//XBee network ID
//-------COORDINATE WITH OTHER TEAMS IF NECESSARY-------
const String ID = "MEYER1"; //Choose a unique 4-digit hexadecimal network ID
//----THIS MUST MATCH YOUR THERMAL-VAC COMPUTER CODE-----

//XBee object
XBee xBee = XBee(&XBeeSerial);

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

Adafruit_MAX31856 maxthermo1 = Adafruit_MAX31856(5, 6, 7, 8);
Adafruit_MAX31856 maxthermo2 = Adafruit_MAX31856(22, 24, 26, 28);
Adafruit_MAX31856 maxthermo3 = Adafruit_MAX31856(23, 25, 27, 29);
Adafruit_MAX31856 maxthermo4 = Adafruit_MAX31856(30, 31, 32, 33);

// Declaration for SSD1306 display connected using software SPI (default case): (NOT I2C)
#define OLED_MOSI  9
#define OLED_CLK   38
#define OLED_DC    34
#define OLED_CS    35
#define OLED_RESET 36
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT,OLED_MOSI, OLED_CLK, OLED_DC, OLED_RESET, OLED_CS);

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
bool SecS = false; // Boolean to start or stop data collection

// Active Heating
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

// End of active heating

 Relay R = Relay(42, 43);

void setup() {
// Heater initialization
  R.init();
   R.openRelay();
   delay(500);
   R.closeRelay();
  Serial.begin(9600);
  //start XBee communication
  XBeeSerial.begin(9600);
  //configure radio with ID; designate as "flight" (thermal-vac test) unit
  Serial.println(xBee.enterATmode());
  Serial.println(xBee.atCommand("ATMY0"));
  Serial.println(xBee.atCommand("ATDL1"));
  Serial.println(xBee.atCommand("ATID" + ID));
  Serial.println(xBee.exitATmode());

  //SD-card Setup
  pinMode(10, OUTPUT);      //Needed for SD library, regardless of shield used
  pinMode(chipSelect, OUTPUT);
  Serial.print("Initializing SD card...");
  if (!SD.begin(chipSelect)) {                            //attempt to start SD communication
    Serial.println(" Card failed, or not present");          //print out error if failed; remind user to check card
  }
  else {                                                    //if successful, attempt to create file
    Serial.println("Card initialized.\nCreating File...");
    for (byte i = 0; i < 100; i++) {                        //can create up to 100 files with similar names, but numbered differently
      filename[4] = '0' + i / 10;
      filename[5] = '0' + i % 10;
      if (!SD.exists(filename)) {                           //if a given filename doesn't exist, it's available
        datalog = SD.open(filename, FILE_WRITE);            //create file with that name
        SDactive = true;                                    //activate SD logging since file creation was successful
        Serial.println("Logging to: " + String(filename));  //Tell user which file contains the data for this run of the program
        break;                                              //Exit the for loop now that we have a file
      }
    }
    if (!SDactive) Serial.println("No available file names; clear SD card to enable logging");
  }
  String header = "PR,    T1, T2, T3, T4, DP, Sec";  //setup data format, and print it to the monitor and SD card
  Serial.println(header);
  if (SDactive) {
    datalog.println(header);
    datalog.close();
  }

  /*
    // XBee Setup (ALTERNATIVE METHOD)
    //Optional: set a specific cooldown time (in seconds) during which the XBee will ignore repeats of the same command
    //Default is 10 seconds if function isn't called
    // xBee.setCooldown(4000); // I'll set it to 4 seconds for the Thermal-Vac

    //Begin XBee communication at specified baud rate. Should be 9600 unless previously set otherwise.
    xBee.initialize();
    /1111 */
  // Setup Adafruit MAX_31856 Thermocouples #1-4
  maxthermo1.begin();
  maxthermo2.begin();
  maxthermo3.begin();
  maxthermo4.begin();
  maxthermo1.setThermocoupleType(MAX31856_TCTYPE_K);
  maxthermo2.setThermocoupleType(MAX31856_TCTYPE_K);
  maxthermo3.setThermocoupleType(MAX31856_TCTYPE_K);
  maxthermo4.setThermocoupleType(MAX31856_TCTYPE_K);
  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3D)) { // Address 0x3D for 128x64
    Serial.println(F("SSD1306 allocation failed"));
    for (;;); // Don't proceed, loop forever
  }
  // Show initial display buffer contents on the screen --
  // the library initializes this with an Adafruit splash screen.
  display.display();
  delay(2000); // Pause for 2 seconds

  // Clear the buffer
  display.clearDisplay();

  // Draw a single pixel in white
  display.drawPixel(10, 10, WHITE);

  // Show the display buffer on the screen. You MUST call display() after
  // drawing commands to make them visible on screen!
  display.display();
  delay(2000);
  // display.display() is NOT necessary after every single drawing command,
  // unless that's what you want...rather, you can batch up a bunch of
  // drawing operations and then update the screen all at once by calling
  // display.display(). These examples demonstrate both approaches...
  display.clearDisplay();

  // Initialize some things
  display.setTextSize(1);            // Normal 1:1 pixel scale
  display.setTextColor(WHITE);        // Draw white text
  display.setCursor(0, 0);            // Start at top-left corner


  // Project Title and Author
  Serial.println("Thermal-Vac Chamber");
  Serial.println("By Jacob Meyer");
  Serial.println("12/31/2018 12:31PM");
  display.println("Thermal-Vac Chamber");
  display.println("By Jacob Meyer");
  display.println("12/31/2018 12:31PM");

  display.display();
  delay(20);
  display.clearDisplay();
}
// Decide whether to use the thermocouples with SPI or I2C
void loop() {
  if (millis() - prevTime >= 1000) // Almost precisely makes a one second interval (as opposed to using "delay()")
  {
    prevTime = millis();
    // Always check for incoming commands
    String command = Serial.readStringUntil('\n');
    // Pressure Sensor
    pressureSensor = analogRead(A0);                // Read the analog pin
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

    // XBee always checks for commands
    CheckXBee(atmSTR, TakeData, command);

    // Functions to relay sensor info to certain devices
    OLED(atmSTR, T1int, T2int, T3int, T4int, TakeData);
    serial(atmSTR, T1int, T2int, T3int, T4int, TakeData);
    SDLogging(atmSTR, T1int, T2int, T3int, T4int, TakeData);

    if (SecS == true)
    {
      count++;
    }
    count2++;
    Misc(atm, T4);
  }

}

void OLED(String P, int T1, int T2, int T3, int T4, bool &TakeData) {

  if (0 <= count2 && count2 % 7 == 0) // Method 1 of Showing Data (Show 7, Erase, Repeat forever)
  {
    //Header
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("PR    T1 T2 T3 T4 Sec");
    display.display();
  }
  if (TakeData == true)
  {
    display.println(P + " " + T1 + " " + T2 + " " + T3 + " " + T4 + " " + String(count)); // Use for Methods 1 and 2 not 3
    display.display(); // show initial text
  }
  else if (TakeData == false)
  {
    display.println(P + " " + T1 + " " + T2 + " " + T3 + " " + T4 + " "); // Use for Methods 1 and 2 not 3
    display.display(); // show initial text
  }
}

void serial(String P, double T1, double T2, double T3, double T4, bool &TakeData) {
  if (TakeData == true && count == 0)
  {
    Serial.println("PRESSURE (ATM)    T1   T2   T3   T4   Sec");
    SecS = true;
  }
  else if (TakeData == true && count > 0)
  {
    Serial.println(P + " " + T1 + " " + T2 + " " + T3 + " " + T4 + " " + String(count));
  }
  else if (TakeData == false)
  {
    Serial.println(P + " " + T1 + " " + T2 + " " + T3 + " " + T4);
  }
}

void SDLogging(String P, int T1, int T2, int T3, int T4, bool &TakeData) {
  if (SDactive/* && TakeData==true*/) {
    datalog = SD.open(filename, FILE_WRITE);
    datalog.println(P + ", " + T1 + ", " + T2 + ", " + T3 + ", " + T4 + ", " + String(count) + ", " + String(millis()*1000));          //...and to SD card if available // CHange Format Later
    datalog.close();                //close file afterward to ensure data is saved properly
  }
}

void CheckXBee(String P, bool &TakeData, String command) { // Can just make "TakeData" global if it doesn't work this way
  //all possible commands go in a series of if-else statements, where the correct action is taken in each case
  if (command == "")
  {
    return; // Try without this later
  }
  else if (command == "HI") {
    Serial.println("Hello");
  }
  else if (command == "NAME") {
    Serial.println(ID);
  }
  else if (command == "BEGIN") {
    TakeData = true;
    Serial.println(" Beginning data collection");
  }
  else if (command == "STOP") {
    TakeData = false;
    Serial.println(" Data collection stopped.");
  }
  else if (command == "HEATON")
  {
    // Turn the heaters on
  }
  else if (command == "HEATOFF")
  {
    // Turn the heaters off
  }
  /*else if(command=="")
    else if(command=="")*/
}

void Misc(float atm, double T4) {
  // Start with code for battery heater; assign thermocouple # 4 to battery
  if (T4 < 10)
  {
    R.openRelay();
    // Turn the heater on
  }
  else if (T4 > 25)
  {
    R.closeRelay();
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
