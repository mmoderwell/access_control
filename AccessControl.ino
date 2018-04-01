/*
--------------------------------------------------------------------------------------------------------------------
An Arduino door lock using RFID, EEPROM, and a servo.
--------------------------------------------------------------------------------------------------------------------
Thanks to the hard work of @miguelbalboa and others with https://github.com/miguelbalboa/rfid

First define a master key which will be used as a programmer.
After scanning that you will then be able to add or remove cards.


Information stored on Arduino's non volatile EEPROM memory.
No Information is lost when the system resets.
EEPROM has unlimited Read cycle but roughly 100,000 limited Write cycle.

*/

/* wiring the MF RC522 to ESP8266 (ESP-12) or NodeMCU v2
RST     = GPIO5 (D1)
SDA(SS) = GPIO4 (D2)
MOSI    = GPIO13 (D7)
MISO    = GPIO12 (D6)
SCK     = GPIO14 (D5)
GND     = GND 
3.3V    = 3.3V
*/

#include "FastLED.h"    //a single rgb led to give user feedback
#include <EEPROM.h>     //to read and write PICC's UIDs from/to EEPROM
#include <SPI.h>        //RC522 module uses the SPI protocol
#include <MFRC522.h>    //for Mifare RC522 Devices
#include <Servo.h> 
 
Servo myservo;  // create servo object to control a servo 


// The data pin for the WS2812B LED
#define DATA_PIN 15 //D8 on NodeMCU v2

// How many LEDS are attached to the Arduino?
#define NUM_LEDS 1

CRGB leds[NUM_LEDS];

bool match = false;           //initialize card match
bool programMode = false;     //initialize programming mode
bool replaceMaster = false;

uint8_t successfulRead;       //Integer to keep to keep track of a successful read or not

byte storedCard[4];           //Stores an ID read from EEPROM
byte readCard[4];             //Stores scanned ID read from RFID module
byte masterCard[4];           //Stores master card's ID read from EEPROM

constexpr uint8_t RST_PIN = 5;  //RST_PIN for RC522 RFID module at GPIO5 (D1 on NodeMCU)
constexpr uint8_t SS_PIN  = 4;  //SDA_PIN for RC522 RFID module at GPIO4 (D2 on NodeMCU)

MFRC522 mfrc522(SS_PIN, RST_PIN);



void setup() {
  
  //Protocol config
  
  Serial.begin(9600);     //Initialize serial communication with computer
  SPI.begin();            //SPI protocol needed to communicate with MFRC522
  mfrc522.PCD_Init();     //Initialize MFRC522 hardware
  EEPROM.begin(4096);

  FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);;
  FastLED.setBrightness(20);  //set brighness to lower power consumption
  myservo.attach(16);  // attaches the servo on GIO2 to the servo object 

  Serial.println(F("Access Control Version 1.0"));    // For debugging purposes
  ShowReaderDetails();                                // Show MFRC522 card reader details
  
  // Check if master card defined, if not let user choose a master card
  // This also useful to just redefine the Master Card
  // You can keep other EEPROM records just write other than 143 to EEPROM address 1
  // EEPROM address 1 should hold magical number which is '143'
  if (EEPROM.read(1) != 143) {
    Serial.println(F("No master key defined"));
    Serial.println(F("Scan a card to define as master key"));
    do {
      successfulRead = getID(); // sets successfulRead to 1 when we get read from reader otherwise 0
      leds[0] = CRGB::Blue;
      FastLED.show(); // This sends the updated pixel color to the hardware.
      delay(200);      
      leds[0] = CRGB::Black;
      FastLED.show(); // This sends the updated pixel color to the hardware.
      delay(200);
    }
    while (!successfulRead);                  // Program will not go further while you not get a successful read
    for ( uint8_t j = 0; j < 4; j++ ) {        // Loop 4 times
      EEPROM.write( 2 + j, readCard[j] );  // Write scanned PICC's UID to EEPROM, start from address 3
    }
    EEPROM.write(1, 143);                  // Write to EEPROM we defined Master Card.
    EEPROM.commit();
    Serial.println(F("Master key defined"));
  }


  Serial.println(F("-------------------"));
  Serial.println(F("Master key's UID"));
  for ( uint8_t i = 0; i < 4; i++ ) {          // Read Master Card's UID from EEPROM
    masterCard[i] = EEPROM.read(2 + i);    // Write it to masterCard
    Serial.print(masterCard[i], HEX);
  }
  Serial.println("");
  Serial.println(F("-------------------"));
  Serial.println(F("Everything is ready"));
  Serial.println(F("Waiting for a card to be scanned"));
  cycleLeds();    // Everything ready lets give user some feedback by cycling leds
}

///////////////////////////////////////// Main Loop ///////////////////////////////////
void loop() {

  do {
      successfulRead = getID();  // sets successfulRead to 1 when we get read from reader otherwise 0

      if (programMode) {
        cycleLeds();              // Program Mode cycles through Red Green Blue waiting to read a new card
      }
      else {
        normalModeOn();     // Normal mode, Blue Power LED is on, all others are off
      }
      delay(100);
  } while (!successfulRead);

  if (programMode){
    if (isMaster(readCard)) { //When in program mode check First If master card scanned again to exit program mode
      Serial.println(F("Master key scanned"));
      Serial.println(F("Exiting Program Mode"));
      Serial.println(F("-----------------------------"));
      programMode = false;
      return;
    }
    else{
      if (findID(readCard)) { // If scanned card is known delete it
        Serial.println(F("Recognized card, removing from memory"));
        deleteID(readCard);
        Serial.println("-----------------------------");
        Serial.println(F("Scan a PICC to add or remove to EEPROM"));
      }
      else {                    // If scanned card is not known add it
        Serial.println(F("Unrecognized card, adding to memory"));
        writeID(readCard);
        Serial.println(F("-----------------------------"));
        Serial.println(F("Scan a PICC to add or remove to EEPROM"));
      }
    }
  }
  else{
    if (isMaster(readCard)) {    // If scanned card's ID matches Master Card's ID - enter program mode
      programMode = true;
      Serial.println(F("Master key recognized - Entered Program Mode"));
      uint8_t count = EEPROM.read(0);   // Read the first Byte of EEPROM that
      Serial.print(F("I have "));     // stores the number of ID's in EEPROM
      Serial.print(count);
      Serial.print(F(" record(s) on EEPROM"));
      Serial.println("");
      Serial.println(F("Scan a PICC to add or remove to EEPROM"));
      Serial.println(F("Scan master key again to exit Program Mode"));
      Serial.println(F("-----------------------------"));
    }
    else{
      if (findID(readCard)) { // If not, see if the card is in the EEPROM
        Serial.println(F("Welcome to la casa."));
        granted(1000);        // Open the door lock for 300 ms
        }
      else {      // If not, show that the ID was not valid
        Serial.println(F("Unrecognized key. Access denied."));
        denied();
      }        
    }   
  }
}

/////////////////////////////////////////  Access Granted    ///////////////////////////////////
void granted ( uint16_t setDelay) {
  leds[0] = CRGB::Green;
  FastLED.show(); // This sends the updated pixel color to the hardware.
  delay(1000); // Delay for a period of time (in milliseconds)
int pos;

  for(pos = 0; pos <= 180; pos += 1) // goes from 0 degrees to 180 degrees 
  {                                  // in steps of 1 degree 
    myservo.write(pos);              // tell servo to go to position in variable 'pos' 
    delay(15);                       // waits 15ms for the servo to reach the position 
  } 
  for(pos = 180; pos>=0; pos-=1)     // goes from 180 degrees to 0 degrees 
  {                                
    myservo.write(pos);              // tell servo to go to position in variable 'pos' 
    delay(15);                       // waits 15ms for the servo to reach the position 
  } 
  FastLED.show();
}

///////////////////////////////////////// Access Denied  ///////////////////////////////////
void denied() {
  leds[0] = CRGB::Red;
  FastLED.show(); // This sends the updated pixel color to the hardware.
  delay(1000);
  leds[0] = CRGB::Black;
  FastLED.show(); // This sends the updated pixel color to the hardware.
}


///////////////////////////////////////// Get PICC's UID ///////////////////////////////////
uint8_t getID() {
  // Getting ready for Reading PICCs
  if (!mfrc522.PICC_IsNewCardPresent()) { //If a new PICC placed to RFID reader continue
    return 0;
  }
  if (!mfrc522.PICC_ReadCardSerial()) {   //Since a PICC placed get Serial and continue
    return 0;
  }
  // There are Mifare PICCs which have 4 byte or 7 byte UID care if you use 7 byte PICC
  // I think we should assume every PICC as they have 4 byte UID
  // Until we support 7 byte PICCs
  Serial.println(F("Scanned PICC's UID:"));
  for ( uint8_t i = 0; i < 4; i++) {  //
    readCard[i] = mfrc522.uid.uidByte[i];
    Serial.print(readCard[i], HEX);
  }
  Serial.println("");
  mfrc522.PICC_HaltA(); // Stop reading
  return 1;
}

void ShowReaderDetails() {
  // Get the MFRC522 software version
  byte v = mfrc522.PCD_ReadRegister(mfrc522.VersionReg);
  Serial.print(F("MFRC522 Software Version: 0x"));
  Serial.print(v, HEX);
  if (v == 0x91)
    Serial.print(F(" = v1.0"));
  else if (v == 0x92)
    Serial.print(F(" = v2.0"));
  else
    Serial.print(F(" (unknown),probably a chinese clone?"));
  Serial.println("");
  // When 0x00 or 0xFF is returned, communication probably failed
  if ((v == 0x00) || (v == 0xFF)) {
    Serial.println(F("WARNING: Communication failure, is the MFRC522 properly connected?"));
    Serial.println(F("SYSTEM HALTED: Check connections."));
    // Visualize system is halted
    leds[0] = CRGB::Red;
    FastLED.show(); // This sends the updated pixel color to the hardware.
      
    while (true); // do not go further
  }
}

///////////////////////////////////////// Cycle Leds (Program Mode) ///////////////////////////////////
void cycleLeds() {
  leds[0] = CRGB::Green;
  FastLED.show(); // This sends the updated pixel color to the hardware.     
  delay(200);
  leds[0] = CRGB::Blue;
  FastLED.show(); // This sends the updated pixel color to the hardware.     
  delay(200);
  leds[0] = CRGB::Red;
  FastLED.show(); // This sends the updated pixel color to the hardware.     
  delay(200);
}

//////////////////////////////////////// Normal Mode Led  ///////////////////////////////////
void normalModeOn () {
 leds[0] = CRGB::Blue;
  FastLED.show(); // This sends the updated pixel color to the hardware.
     
  //digitalWrite(LED_BUILTIN, HIGH);    // Make sure Door is Locked
}

//////////////////////////////////////// Read an ID from EEPROM //////////////////////////////
void readID( uint8_t number ) {
  uint8_t start = (number * 4 ) + 2;    // Figure out starting position
  for ( uint8_t i = 0; i < 4; i++ ) {     // Loop 4 times to get the 4 Bytes
    storedCard[i] = EEPROM.read(start + i);   // Assign values read from EEPROM to array
  }
}

///////////////////////////////////////// Add ID to EEPROM   ///////////////////////////////////
void writeID( byte a[] ) {
  if ( !findID( a ) ) {     // Before we write to the EEPROM, check to see if we have seen this card before!
    uint8_t num = EEPROM.read(0);     // Get the numer of used spaces, position 0 stores the number of ID cards
    uint8_t start = ( num * 4 ) + 6;  // Figure out where the next slot starts
    num++;                // Increment the counter by one
    EEPROM.write( 0, num );     // Write the new count to the counter
    for ( uint8_t j = 0; j < 4; j++ ) {   // Loop 4 times
      EEPROM.write( start + j, a[j] );  // Write the array values to EEPROM in the right position
    }
    EEPROM.commit();
    successWrite();
    Serial.println(F("Succesfully added ID record to EEPROM"));
  }
  else {
    failedWrite();
    Serial.println(F("Failed! There is something wrong with ID or bad EEPROM"));
  }
}

///////////////////////////////////////// Remove ID from EEPROM   ///////////////////////////////////
void deleteID( byte a[] ) {
  if ( !findID( a ) ) {     // Before we delete from the EEPROM, check to see if we have this card!
    failedWrite();      // If not
    Serial.println(F("Failed! There is something wrong with ID or bad EEPROM"));
  }
  else {
    uint8_t num = EEPROM.read(0);   // Get the numer of used spaces, position 0 stores the number of ID cards
    uint8_t slot;       // Figure out the slot number of the card
    uint8_t start;      // = ( num * 4 ) + 6; // Figure out where the next slot starts
    uint8_t looping;    // The number of times the loop repeats
    uint8_t j;
    uint8_t count = EEPROM.read(0); // Read the first Byte of EEPROM that stores number of cards
    slot = findIDSLOT( a );   // Figure out the slot number of the card to delete
    start = (slot * 4) + 2;
    looping = ((num - slot) * 4);
    num--;      // Decrement the counter by one
    EEPROM.write( 0, num );   // Write the new count to the counter
    for ( j = 0; j < looping; j++ ) {         // Loop the card shift times
      EEPROM.write( start + j, EEPROM.read(start + 4 + j));   // Shift the array values to 4 places earlier in the EEPROM
    }
    for ( uint8_t k = 0; k < 4; k++ ) {         // Shifting loop
      EEPROM.write( start + j + k, 0);
    }
    successDelete();
    Serial.println(F("Succesfully removed ID record from EEPROM"));
  }
}

///////////////////////////////////////// Check Bytes   ///////////////////////////////////
bool checkTwo (byte a[], byte b[]) {
  if (a[0] != 0)      // Make sure there is something in the array first
    match = true;       // Assume they match at first
  for (uint8_t k = 0; k < 4; k++) {   // Loop 4 times
    if (a[k] != b[k])     // IF a != b then set match = false, one fails, all fail
      match = false;
  }
  if (match) {      // Check to see if if match is still true
    return true;      // Return true
  }
  else  {
    return false;       // Return false
  }
}

///////////////////////////////////////// Find Slot   ///////////////////////////////////
uint8_t findIDSLOT(byte find[]) {
  uint8_t count = EEPROM.read(0);       // Read the first Byte of EEPROM that
  for (uint8_t i = 1; i <= count; i++) {    // Loop once for each EEPROM entry
    readID(i);                // Read an ID from EEPROM, it is stored in storedCard[4]
    if (checkTwo(find, storedCard)) {   // Check to see if the storedCard read from EEPROM
      // is the same as the find[] ID card passed
      return i;         // The slot number of the card
      break;          // Stop looking we found it
    }
  }
}

///////////////////////////////////////// Find ID From EEPROM   ///////////////////////////////////
bool findID(byte find[]) {
  uint8_t count = EEPROM.read(0);     // Read the first Byte of EEPROM that
  for (uint8_t i = 1; i <= count; i++) {    // Loop once for each EEPROM entry
    readID(i);          // Read an ID from EEPROM, it is stored in storedCard[4]
    if (checkTwo(find, storedCard)) {   // Check to see if the storedCard read from EEPROM
      return true;
      break;  // Stop looking we found it
    }
    else {    // If not, return false
    }
  }
  return false;
}

///////////////////////////////////////// Write Success to EEPROM   ///////////////////////////////////
// Flashes the Green LED 3 times to indicate a successful write to EEPROM
void successWrite() {
  leds[0] = CRGB::Green;
  FastLED.show(); // This sends the updated pixel color to the hardware.    
  delay(200);
  leds[0] = CRGB::Black;
  FastLED.show(); // This sends the updated pixel color to the hardware.     
  delay(200);
  leds[0] = CRGB::Green;
  FastLED.show(); // This sends the updated pixel color to the hardware.    
  delay(200);
  leds[0] = CRGB::Black;
  FastLED.show(); // This sends the updated pixel color to the hardware.     
  delay(200);
  leds[0] = CRGB::Green;
  FastLED.show(); // This sends the updated pixel color to the hardware.     
  delay(200);
  leds[0] = CRGB::Black;
  FastLED.show(); // This sends the updated pixel color to the hardware.      
  delay(200);
}

///////////////////////////////////////// Write Failed to EEPROM   ///////////////////////////////////
// Flashes the red LED 3 times to indicate a failed write to EEPROM
void failedWrite() {
  leds[0] = CRGB::Red;
  FastLED.show(); // This sends the updated pixel color to the hardware.
  delay(200);
  leds[0] = CRGB::Black;
  FastLED.show(); // This sends the updated pixel color to the hardware.
  delay(200);
  leds[0] = CRGB::Red;
  FastLED.show(); // This sends the updated pixel color to the hardware.
  delay(200);
  leds[0] = CRGB::Black;
  FastLED.show(); // This sends the updated pixel color to the hardware.
  delay(200);
  leds[0] = CRGB::Red;
  FastLED.show(); // This sends the updated pixel color to the hardware.
  delay(200);
  leds[0] = CRGB::Black;
  FastLED.show(); // This sends the updated pixel color to the hardware.
  delay(200);
}

///////////////////////////////////////// Success Remove UID From EEPROM  ///////////////////////////////////
// Flashes the Blue LED 3 times to indicate a success delete to EEPROM
void successDelete() {
  leds[0] = CRGB::Black;
  FastLED.show(); // This sends the updated pixel color to the hardware.
  delay(200);
  leds[0] = CRGB::Blue;
  FastLED.show(); // This sends the updated pixel color to the hardware.
  delay(200);
  leds[0] = CRGB::Black;
  FastLED.show(); // This sends the updated pixel color to the hardware.
  delay(200);
  leds[0] = CRGB::Blue;
  FastLED.show(); // This sends the updated pixel color to the hardware.
  delay(200);
  leds[0] = CRGB::Black;
  FastLED.show(); // This sends the updated pixel color to the hardware.
  delay(200);
  leds[0] = CRGB::Blue;
  FastLED.show(); // This sends the updated pixel color to the hardware.
  delay(200);
}

////////////////////// Check readCard IF is masterCard   ///////////////////////////////////
// Check to see if the ID passed is the master programing card
bool isMaster(byte test[]) {
  if (checkTwo(test, masterCard))
    return true;
  else
    return false;
}