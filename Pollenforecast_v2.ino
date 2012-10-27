///////////////////////////////////////////////////////////
// Get the days forecast (usually a pollen forecast)
// and display it on the 3D printed ball using LED's
//
// Josh Trotter-Wanner - Oct. 2012
//
//
//
//////////////////////////
// Using XML parsing code from http://forums.adafruit.com/viewtopic.php?f=25&t=30824
// Get XML formatted data from the web.
// 1/6/08 Bob S. - Created
//
//  Assumptions: single XML line looks like:
//    <tag>data</tag> or <tag>data
//
///////////////////////////////////////////////////////////
boolean debug = false;

#include <Ethernet.h>
#include <SPI.h>
#include <string.h>
#include <EEPROM.h>

// Based on wiring other side of LED's to ground
#define off LOW
#define on HIGH

// Pins 1 to 9 are avalible for led's (don't use pin zero as an output)
#define One_Green 7
#define One_Yellow 8
#define One_Red 9

#define Two_Green 4
#define Two_Yellow 5
#define Two_Red 6

#define Three_Green 1
#define Three_Yellow 2
#define Three_Red 3

const int blinkTiming = 1500; // blink cycle time in milliseconds 

boolean error = false;
boolean startTest = true;
boolean toggle = true;

// Light being tested
int pX = 0;
int pY = 0;

const byte Pins[3][3] = { //[Display], [Color]
  {One_Green,One_Yellow,One_Red},        // First display
  {Two_Green,Two_Yellow,Two_Red},        // Second display
  {Three_Green,Three_Yellow,Three_Red}   // Third display
};

// "Low", "Medium", "High", and "Very High"
// 0=No Forecast/Off, 1=Low/Green, 2=Medium/Yellow, 3=High/Red, 4=Very High/Flashing Red
byte currentForecast[3] = { 0,0,0 };    // { First, Second, Third }

// Enter a MAC address for your controller below.
// Newer Ethernet shields have a MAC address printed on a sticker on the shield
//90-A2-DA-00-14-4A
byte mac[] = {  0x90, 0xA2, 0xDA, 0x00, 0x14, 0x4A };
//char serverName[] = "welovedata.org";  //switch to this when live
char serverName[] = "192.168.123.103";
char targetFile[] = "/forecast/forecast.php";

///Serial Number storage location
int SN_Start=16;  //Choose location from 0 to 511 - SN_Len
int SN_Len=4;

///Web response parse variables for XML parsing code
// Max string length may have to be adjusted depending on data to be extracted
#define MAX_STRING_LEN  20

// Setup vars
char tagStr[MAX_STRING_LEN] = "";
char dataStr[MAX_STRING_LEN] = "";
char tmpStr[MAX_STRING_LEN] = "";
char endTag[3] = {'<', '/', '\0'};
int len;

// Flags to differentiate XML tags from document elements (ie. data)
boolean tagFlag = false;
boolean dataFlag = false;
///

// Initialize the Ethernet client library
// with the IP address and port of the server 
// that you want to connect to (port 80 is default for HTTP):
EthernetClient client;

void waitAwhile() {
  //need to include overflow handling? ??????
  delay(3600000); //an hour
//  delay(60000); //a minute
//  delay(20000); //20secs
}

void setup() {
  
  // Open serial communications and wait for port to open:
  if (debug) {Serial.begin(9600);}
  
  for (byte x=0;x<3;x++) {
    for (byte y=0;y<3;y++) {
      pinMode (Pins[x][y], OUTPUT);
      digitalWrite (Pins[x][y], off);
    }
  }
  
  cli();  //stop interupts

//set timer1 interrupt at 2Hz
  TCCR1A = 0;// set entire TCCR1A register to 0
  TCCR1B = 0;// same for TCCR1B
  TCNT1  = 0;//initialize counter value to 0
  // set compare match register for 1hz increments
//  OCR1A = 15624;// = (16*10^6) / (1*1024) - 1 (must be <65536) = 1Hz
  if (debug) {
    Serial.print("Timing target=");
    Serial.println( word( (16000000) / ((2000/blinkTiming)*1024) ) - 1 );
  }
  OCR1A = ( word( (16000000) / ((2000/blinkTiming)*1024) ) - 1 );
  
  // turn on CTC mode
  TCCR1B |= (1 << WGM12);
  // Set CS10 and CS12 bits for 1024 prescaler
  TCCR1B |= (1 << CS12) | (1 << CS10);  
  // enable timer compare interrupt
  TIMSK1 |= (1 << OCIE1A);
  
  sei();//allow interrupts

  // start the Ethernet connection:
  if (Ethernet.begin(mac) == 0) {
    if (debug) {Serial.println("Failed to configure Ethernet using DHCP");}
    error = true; //blink One_Yellow
    // no point in carrying on, so do nothing forevermore:
    while(true);
  }

  // give the Ethernet shield a second to initialize:
  delay(1000);
}

void loop() {
  if (client.connect(serverName, 80)) {
    if (debug) {Serial.println("connected");}
    // Make a HTTP request:
    // ********** BUG **********
    // this code needs to be modified to work with a virtual host setup
    // The HOST value needs to be sent
    // ********** 
    client.print("GET ");
    client.print(targetFile);
    client.print("?sn=");
    for(int x = SN_Start; x < ( SN_Start + SN_Len ); x++) {
      client.print(EEPROM.read(x));
      if (x < ( SN_Start + SN_Len -1 ))
        client.print(".");
    }
    client.println(" HTTP/1.0");
    client.println();
    error=false;
  } else {
    // if you didn't get a connection to the server:
    if (debug) {Serial.println("connection failed");}
    error=true;
  }
  
  if (debug) {Serial.println("Requested page");}
  
  while (!client.available()); //wait for a response
    
  // Read serial data in from web:
  while (client.available()) {
    serialEvent();
  }
  
  if (!client.connected()) {
    if (debug) {
      Serial.println();
      Serial.println("disconnecting.");
    }
    client.stop();
    if (debug) {Serial.println("waiting");}
    waitAwhile();
  }
  
  if (debug) {Serial.println("looping");}
  
}

void setSN(char *SNdata) {
  char aNewSNbyte[4];
  byte NewSN[4];
  int j=0;
  // check SN from web against the current serial number
  // if different then update serial number (100,000 max updates)
  if (debug) {
    Serial.println();
    Serial.print("New SN=");
    Serial.println(SNdata);
  }
  for (int x=0;x<4;x++) {
    aNewSNbyte[x] = '\0';
  }
  for (int i=0;i<strlen(SNdata);i++) {
    // Parse the returned serial number
    if (SNdata[i] == '.') {
      NewSN[j] = atoi(aNewSNbyte);
      if (debug) {
        Serial.print(strlen(aNewSNbyte));
        Serial.print("=");
        Serial.print(aNewSNbyte);
        Serial.print("=");
        Serial.print(atoi(aNewSNbyte));
      }
      for (int x=0;x<4;x++) {
        aNewSNbyte[x] = '\0';
      }
      j++;
    } else {
      aNewSNbyte[strlen(aNewSNbyte)] = SNdata[i];
    }
  }
  NewSN[j] = atoi(aNewSNbyte);  //assign the last byte because the SN doesn't end in a period
  if (debug) {Serial.println(aNewSNbyte);}
  j++;

  if (debug) {
    Serial.print("New SN Parsed=");
    for (int x=0;x<j;x++) {
      Serial.print(NewSN[x]);
      Serial.print(" ");
    }
    Serial.println();
  }
  
//  currentSNbyte = SNdata;
  if (debug) {
    Serial.print("NewSN bytes=");
    Serial.println(j);
    Serial.print("Current SN=");
  }
  for(int x = SN_Start; x < ( SN_Start + SN_Len ); x++) {
    if (debug) {Serial.print(EEPROM.read(x));}
      if (EEPROM.read(x) != NewSN[x-SN_Start]){
        if (debug) {Serial.print("->"); Serial.print(NewSN[x-SN_Start]); Serial.print(" ");}
        ////////////////////////////////////////////////////////////
        // This writes the new serial number to the Pollen predictor
        // only 100,000 writes are permitted thru the life of the device
//        EEPROM.write(x,NewSN[x-SN_Start]);
      }
    if (debug) {
      if (x < ( SN_Start + SN_Len -1 ))
        Serial.print(".");
    }
  }
  if (debug) {Serial.println();}
  
}

void setLights(byte numb, char *fc) {
  // Set the lights
  if ( strcmp(fc,"Low") == 0 ) {
    currentForecast[numb - 1] = 1;
  } else if ( strcmp(fc,"Medium") == 0 ){
    currentForecast[numb - 1] = 2;
  } else if ( strcmp(fc,"High") == 0 ){
    currentForecast[numb - 1] = 3;
  } else if ( strcmp(fc,"Very High") == 0 ){
    currentForecast[numb - 1] = 4;
  } else {
    currentForecast[numb - 1] = 0;
  }
}

// Process each char from web
void serialEvent() {

   // Read a char
	char inChar = client.read();
  
   if (inChar == '<') {
      addChar(inChar, tmpStr);
      tagFlag = true;
      dataFlag = false;

   } else if (inChar == '>') {
      addChar(inChar, tmpStr);

      if (tagFlag) {      
         strncpy(tagStr, tmpStr, strlen(tmpStr)+1);
      }

      // Clear tmp
      clearStr(tmpStr);

      tagFlag = false;
      dataFlag = true;      
      
   } else if (inChar != 10) {
      if (tagFlag) {
         // Add tag char to string
         addChar(inChar, tmpStr);

         // Check for </XML> end tag, ignore it
         if ( tagFlag && strcmp(tmpStr, endTag) == 0 ) {
            clearStr(tmpStr);
            tagFlag = false;
            dataFlag = false;
         }
      }
      
      if (dataFlag) {
         // Add data char to string
         addChar(inChar, dataStr);
      }
   }  
  
   // If a LF, process the line
   if (inChar == 10 ) {

/*
      Serial.print("tagStr: ");
      Serial.println(tagStr);
      Serial.print("dataStr: ");
      Serial.println(dataStr);
*/

      // Find specific tags and print data
      if (matchTag("<sn>")) {
        if (debug) {
	  Serial.print("SN: ");
          Serial.print(dataStr);
        }
        setSN(dataStr);
      }
      if (matchTag("<fcOne>")) {
        if (debug) {
	  Serial.print(", Forecast One: ");
          Serial.print(dataStr);
        }
        setLights(1,dataStr);
      }
      if (matchTag("<fcTwo>")) {
        if (debug) {
	  Serial.print(", Forecast Two: ");
          Serial.print(dataStr);
        }
        setLights(2,dataStr);
      }
      if (matchTag("<fcThree>")) {
        if (debug) {
	  Serial.print(", Forecast Three: ");
          Serial.print(dataStr);
          Serial.println("");
        }
        setLights(3,dataStr);
      }

      // Clear all strings
      clearStr(tmpStr);
      clearStr(tagStr);
      clearStr(dataStr);

      // Clear Flags
      tagFlag = false;
      dataFlag = false;
   }
}


//timer1 interrupt to blink/set lights
ISR(TIMER1_COMPA_vect){
 if (error) {  // display a network error
  if (toggle){
    digitalWrite(One_Yellow,on);
    toggle = false;
  } else {
    digitalWrite(One_Yellow,off);
    toggle = true;
  }
 } else if (startTest) {  // test the LED's
  if (toggle){
    digitalWrite(Pins[pX][pY],on);
    toggle = false;
  } else {
    digitalWrite(Pins[pX][pY],off);
    toggle = true;
    pY++;
    if (pY > 2) {
      pX++;
      pY=0;
    }
    if (pX > 2) {
      startTest = false;
    }
  }
 } else {  // set the LED's based on the forecast
  // 
  //[Display], [Color]
  for (byte x=0;x<3;x++) {
    for (byte y=0;y<3;y++) {
      //Set y=2 (Red Pin)
      if ( (currentForecast[x] == 4) &&  ( y == 2 ) ) {
        if (toggle) {
          digitalWrite(Pins[x][2],on);
          toggle = false;
        } else {
          digitalWrite(Pins[x][2],off);
          toggle = true;
        }
      } else {
        if ( ( currentForecast[x] - 1 ) == y ) {
          digitalWrite(Pins[x][y],on);
        } else {
          digitalWrite(Pins[x][y],off);
        }
      }
    }
  }
 }
} //end of ISR

// Function to clear a string
void clearStr (char* str) {
   int len = strlen(str);
   for (int c = 0; c < len; c++) {
      str[c] = 0;
   }
}

//Function to add a char to a string and check its length
void addChar (char ch, char* str) {
   char *tagMsg  = "<TRUNCATED_TAG>";
   char *dataMsg = "-TRUNCATED_DATA-";

   // Check the max size of the string to make sure it doesn't grow too
   // big.  If string is beyond MAX_STRING_LEN assume it is unimportant
   // and replace it with a warning message.
   if (strlen(str) > MAX_STRING_LEN - 2) {
      if (tagFlag) {
         clearStr(tagStr);
         strcpy(tagStr,tagMsg);
      }
      if (dataFlag) {
         clearStr(dataStr);
         strcpy(dataStr,dataMsg);
      }

      // Clear the temp buffer and flags to stop current processing
      clearStr(tmpStr);
      tagFlag = false;
      dataFlag = false;

   } else {
      // Add char to string
      str[strlen(str)] = ch;
   }
}

// Function to check the current tag for a specific string
boolean matchTag (char* searchTag) {
   if ( strcmp(tagStr, searchTag) == 0 ) {
      return true;
   } else {
      return false;
   }
}

