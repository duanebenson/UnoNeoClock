//*********************************************************************    
// UnoNeoClock code
//
// For the  UnoNeoClock
// by Duane Benson, 2016 - 2023
//
// UnoNeoClock is Arduino UNO compatible. It uses a Maxim DS3231M RTC
// and a 60 pixel NeoPixel ring from Adafruit (four of Adafruit product ID#1768)
// It can be programmed with any library that is compatible with
// the Arduino Uno. It uses the standard Arduino IDE (V1.05 or newer) in
//
// The hour indicator is yellow
// The minute indicator is green
// The second indicator is blue
// Each hour marker is indicated by orange in the am, and by pink in the pm
//
// Press the right button to increase brightness
// Press the left button to decrease brightness
//
// To set time, press and release both buttons at the same time.
// The on-board LED (Arduino digital out 13) will light up to indicate time set mode
// Press the right button to increase the hour
// Press the left button to decrease the hour
// Then press both buttons at the same time to set the minutes
// Press the right button to increase the minute
// Press the left button to decrease the minute
// When done, press both buttons at the same time again.
// The seconds will be reset to zero and the on-board LED will turn off
//
// Note that when setting the time, moving the hour past am/pm in either 
// direction will change clock accordingly. Moving the minute past the hour
// in either direction will change the hour accordingly. Moving the hour
// past midnight in either direction will change the internal date. The
// date isn't used in this incarnation, but could be used with additional
// software (written by you) to set alarms.
//
// First revision 9/23/16
// Supports UnoNeoClock V2.0
//
// Most recently compiled with Arduino IDE V2.2.1
// Now works with RTClib.h instead of the older RTC_DS3231.h
//
//*********************************************************************    


//*********************************************************************
// Include files
//*********************************************************************    
    #include <Wire.h>               // Arduino I2C library
    #include <SPI.h>                // Arduino SPI library - needed by RTC_DS3231.h
    #include <Adafruit_NeoPixel.h>  // Adafruit NeoPixel library
    #include <RTClib.h>             // Base RTC (real time clock) library
    //#include <RTC_DS3231.h>         // Library for the DS3231 RTC

//*********************************************************************     
// Defines
//*********************************************************************    
    #define neoPin 9               // Neo Pixel signal pin is D9
    #define Usr1 3                 // Switch Usr1 is D3
    #define Usr2 2                  // Switch Usr2 is D2
    #define ambientSense A3         // Analog input used to read the amient light value
    #define setInd 13               // Clock-being-set idicator is a yellow LED on pin D13
    #define minuteMarks 20          // Base brightness value for dim white pixels marking the minutes
    #define handBrightness 127      // Base brightness value used for the hour, minute and second "hand" pixels
//*********************************************************************
// Objects
//*********************************************************************    
    RTC_DS3231 RTCDS3231M;          // Create clock object "RTCDS3231M"
    DateTime ClockCal;              // Create object containing time and date
     
    Adafruit_NeoPixel clockRing = Adafruit_NeoPixel(72, neoPin, NEO_GRB + NEO_KHZ800); // Create clock ring object

//*********************************************************************     
// Global variables
//*********************************************************************    
    byte am_pm;                      // AM/PM state indicator. 0 = am, 1 = pm
    
    // Working time variables
    uint16_t Myear;
    uint8_t Mmonth;
    uint8_t Mday;
    uint8_t Mhour;
    uint8_t Mmin;
    uint8_t Msec;
    uint8_t Mhour12;
    
    byte redPixels, greenPixels, bluePixels; // Holds pixel color values
    uint8_t  ambient = 32;           // Default brightness level. Will be adjusted by the ambient light sensor
    int UsrSw1, UsrSw2;
    int setTimeMode = 0;
    uint8_t startBright;
    int state;                       // State machine variable for processing buttons
 
     // variables for light reading
    int sensorValue = 0;             // variable to store the value coming from the sensor
    int stripBright = 0;             // variablw to store adjusted brightness value
   
//*********************************************************************
// Arduino startup code section    
//*********************************************************************    
    void setup () {
      Wire.begin();                  // Start up I2C - needs to come before the RTC begin
      RTCDS3231M.begin();            // Start talking to RTC clock over I2C
     
      // Pin direction assignments
      pinMode(neoPin, OUTPUT);       // NeoPixel data line
      pinMode(setInd, OUTPUT);       // Clock being set indicator
      pinMode(Usr1, INPUT_PULLUP);          // Clock setting pin (S1 / Usr1 on LeoNeoClock)
      pinMode(Usr2, INPUT_PULLUP);          // Clock setting pin (S3 / Usr2 on LeoNeoClock)
      
      //if (! RTCDS3231M.isrunning() || digitalRead(Usr1)) { // Set clock if need be
      if (RTCDS3231M.lostPower() || !digitalRead(Usr1)) { // Set clock if need be
        // If the RTC has been reset, or is switch Usr1 is held on startup, read the computer time and set RTC
        // LeoNeoClock must be connected to computer for this to work. Only do this at compile time. It's just
        // reading the time and date that this software was last compiled, not the current PC system time.
        setTime();
      }

      Serial.begin(9600);
      blinkInd(3);                   // Show that the clock is alive
      Serial.println("Set up");

      autoBrightness();              // Check ambient light sensor and adjust the brightness value
      clockRing.begin();             // Start talking to the NeoPixel strip
    }

//*********************************************************************     
// Arduino main code loop
//*********************************************************************    
    void loop () {
      //Serial.println("Before read switches");
      state = readSwitches();        // State machine to read and react to switches     
      //Serial.println("Before state machine");
      switch (state) {
        case 1:
          brightnessUp();
          break;
        case 2:
          brightnessDown();
          break;
        case 3:
          adjustTime();
          break;
        default:
          break;
      }

      autoBrightness();
      readTime();                    // Read from RTC
      showTime();                    // Show on NewPixel ring
      //displayTime();                 // Display time in serial monitor
      
      delay(50);                     // A little delay. This number isn't criticle. Jsut needs to be small enough so buttons feel responsive
    }

//*********************************************************************    
// Utility functions
//*********************************************************************

//*********************************************************************
  void blinkInd(int i) {
    int j;
    for (j = 0; j < i; j++) {
      digitalWrite(setInd, HIGH);
      delay(500);
      digitalWrite(setInd, LOW);
      delay(500);
    }
  }  
//*********************************************************************
  void autoBrightness() {
      sensorValue = analogRead(ambientSense);
      stripBright = map(sensorValue, 0, 1023, 0, 127);
      stripBright = stripBright + ambient;
      clockRing.setBrightness(stripBright); // set brightness
  }
  
//*********************************************************************
  void readTime() {
    ClockCal = RTCDS3231M.now();     // Read the DS3231M RTC
    Myear = ClockCal.year();
    Mmonth = ClockCal.month();
    Mday = ClockCal.day();
    Mhour = ClockCal.hour();
    Mmin = ClockCal.minute();
    Msec = ClockCal.second();
  }
    
//*********************************************************************
  int readSwitches() {
    int swState = 0;
    int sw1State = 0;
    int sw2State = 0;
      
    UsrSw1 = !digitalRead(Usr1);
    UsrSw2 = !digitalRead(Usr2);
      
    while (UsrSw1 || UsrSw2) {
      UsrSw1 = !digitalRead(Usr1);
      UsrSw2 = !digitalRead(Usr2);
      if (UsrSw1) sw1State = 1;
      if (UsrSw2) sw2State = 2;
    }

    swState = sw1State + sw2State;
     
    UsrSw1 = 0;
    UsrSw2 = 0;
    delay(50);
    return (swState);
  }
    
//*********************************************************************
  void brightnessUp() {    
    digitalWrite(setInd, HIGH);
    ambient = ambient + 4;
    if (ambient == 128) ambient = 124;
    clockRing.setBrightness(ambient); // set brightness
    while (!setTimeMode && state == 1) {
      state = readSwitches();
    }
    digitalWrite(setInd, LOW);
  }
    
//*********************************************************************
  void brightnessDown() {     
    digitalWrite(setInd, HIGH);
    ambient = ambient - 4;
    if (ambient < 4) ambient = 4;
    clockRing.setBrightness(ambient); // set brightness

    while (!setTimeMode && state == 2) {
      state = readSwitches();
    }
    digitalWrite(setInd, LOW);
  }
    
    
//*********************************************************************
  void adjustTime() {
    setTimeMode = 1;
    digitalWrite(setInd, HIGH);   // LED 13 is on when time is being set
    startBright = ambient;        // Store current brightness value

    while (state != 0) {          // Wait until neither are pressed anymore
      state = readSwitches();
    }
    state = 0;
      
    while (state != 3) {          // Stay in the loop until both buttons are pressed again
      // Hours first
      while (setTimeMode) {
        readTime();
        state = readSwitches();
        switch (state) {
          case 1:
            hourUp();
            break;
          case 2:
            hourDown();
            break;
          case 3:
            setTimeMode = 0;
            break;
          default:
            break;
        }
        saveTime();            // Save modified time to the RTC
        showTime();            // Show time on NeoPixel Ring
      }
    }    
    while (state != 0) {    // Wait until neither are pressed anymore
      state = readSwitches();
    }
    state = 0;

    setTimeMode = 1;

    while (state != 3) { // Stay in the loop until both buttons are pressed again
      // Adjust minutes
      while (setTimeMode) {
        readTime();
        state = readSwitches();
        switch (state) {
          case 1:
            minuteUp();
            break;
          case 2:
            minuteDown();
            break;
          case 3:
            setTimeMode = 0;
            break;
          default:
            break;
        }
        saveTime();            // Save modified time to the RTC
        showTime();            // Show time on NeoPixel Ring
      }
    }     
    while (state != 0) {    // Wait until neither are pressed anymore
      state = readSwitches();
    }
    
    state = 0;
    Msec = 0;    
    saveTime();            // Save modified time to the RTC
    ambient = startBright;    // Reset brightness to pre-adjustment value
    showTime();            // Show time on NeoPixel Ring
    
    digitalWrite(setInd, LOW);  // LED 13 is off when not setting time
  }
    
//*********************************************************************
  void hourUp() {
    Mhour = Mhour + 1;
    if (Mhour > 23) {
      Mhour -= 24;
       Mday = Mday + 1;
    }
  }
    
//*********************************************************************
  void hourDown() {
    Mhour = Mhour - 1;
    if (Mhour == 255) {
      Mhour += 24;
      Mday = Mday - 1;
    }
  }

//*********************************************************************
  void minuteUp() {
    Mmin = Mmin + 1;
    if (Mmin > 59) {
      Mmin -= 60;
       Mhour = Mhour + 1;
    }
  }
    
//*********************************************************************
  void minuteDown() {
    Mmin = Mmin - 1;
    if (Mmin == 255) {
      Mmin += 60;
      Mhour = Mhour - 1;
    }
  }

//*********************************************************************
  void saveTime() {    
    RTCDS3231M.adjust(DateTime(Myear, Mmonth, Mday, Mhour, Mmin, Msec));
  }
        
//*********************************************************************
  void showTime() {    
    uint8_t i;
    // Convert the 24 hour timestored in the RTC chip to 12 hours
    if(Mhour > 11) {  // PM
      Mhour12 = Mhour - 12;               
      am_pm = 1;
    } else {          // AM   
      Mhour12 = Mhour;
      am_pm = 0;
    }

//  displayTime(); // Will display time on the serial monitor. If you want to use this
//  you also need to uncomment the function at the end of this file.
     
    // Set colors
    // Hour is yellow, minute is green and second is blue
    // Hour markers are orange in am and pink in pm
    // All other pixels will be a dim white
   for (int k=0; k<12; k++) {
      redPixels = 0;
      greenPixels = 0;
      bluePixels = 0;

      if (k == Mhour12){
          redPixels = handBrightness;
          greenPixels = handBrightness;
          bluePixels = 0;
      }
      
      clockRing.setPixelColor(k, clockRing.Color(redPixels, greenPixels, bluePixels)); // Send pixel value to clockRing
    }
 
   for(int j=12; j<clockRing.numPixels(); j++) {
      i = j - 12;
      // Reset pixels to dim white
      redPixels = minuteMarks;
      greenPixels = minuteMarks;
      bluePixels = minuteMarks;

      
      // Hour markers - Set every fifth pixel to orange/pink
      if (i % 5 == 0) {
        redPixels = 127;
        if (!am_pm) {
          greenPixels = 32; 
          bluePixels = 0;
        } else {
          greenPixels = 0;
          bluePixels = 32;
        }
      }
        
      // Second hand - gets precidence over hour markers only
      if (i == Msec) {
        redPixels = 0;
        greenPixels = 0;
        bluePixels = handBrightness;
      }
        
      // Minute hand - gets precidence over hour markers and seconds
      if (i == Mmin) {
        redPixels = 0;
        greenPixels = handBrightness;
        bluePixels = 0;  
      }
        
      clockRing.setPixelColor(i+12, clockRing.Color(redPixels, greenPixels, bluePixels)); // Send pixel value to clockRing
    }
    clockRing.show();                    // Display clock pixels on clockRing
  }
    
//*********************************************************************
  void setTime() {    
    RTCDS3231M.adjust(DateTime(__DATE__, __TIME__)); // Set RTC to the time and date this software was last compiled
  }
    
//*********************************************************************
// Uncomment this to use to display the time on the serial monitor

  void displayTime() {
    Serial.print("Current time is: ");
    Serial.print(Mhour, DEC);
    Serial.print(':');
    Serial.print(Mmin, DEC);
    Serial.print(':');
    Serial.print(Msec, DEC);
    Serial.print(", Light level: ");
    Serial.println(stripBright, DEC);
  }
   

