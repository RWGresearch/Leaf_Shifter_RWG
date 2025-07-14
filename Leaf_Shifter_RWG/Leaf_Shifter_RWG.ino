/*
 * i did and no not plan on spel checking this code... :) your welcome... 
 * This code is to add paddle shifter from a mini cooper rewired through the clock spring using 2 pins for analog input to control a shifter on a Nissan Leaf all years up to 2025.
 * this code was build on 7-11-2025, for more info on this setup see this video: https://www.youtube.com/watch?v=SDfXS1Kpxbs
 * this code is to be shared and enjoyed by all, feel free to change and use it as you see fit. if you do somthing cool though... do tell... 
 * RWG 7-11-2025 
 * V1.2
 */

#include "Arduino.h"
#include "PCF8574.h"  // https://github.com/xreef/PCF8574_library

// Instantiate Wire for generic use at 400kHz
TwoWire I2Ctwo = TwoWire(0);

// Set i2c address for GPIO explanders.
PCF8574 pcf8574_1(&I2Ctwo, 0x20); // expander 1 - A0 = low  ,A1 = low ,A2 = low 
PCF8574 pcf8574_2(&I2Ctwo, 0x21); // expander 2 - A0 = high ,A1 = low ,A2 = low

// set up: 
void setup()
{
  // start serial information. 
  Serial.begin(112560);

  // set up I2C (pin numbers and frequincy)
  I2Ctwo.begin(8,9,400000U); // SDA pin 16, SCL pin 17, 400kHz frequency
  //delay(100);

  // Disable Wi-Fi to save power
  //WiFi.mode(WIFI_OFF);
  // Disable Bluetooth to save power
  btStop();
  //Serial.println("Bluetooth and Wi-Fi disabled.");

  // Set pinMode to OUTPUT / input for bank 1. NOTE im using External Pull Up's you can enable internal pull up's.
  pcf8574_1.pinMode(P0, INPUT);
  pcf8574_1.pinMode(P1, INPUT);
  pcf8574_1.pinMode(P2, INPUT);
  pcf8574_1.pinMode(P3, INPUT);
  pcf8574_1.pinMode(P4, OUTPUT);
  pcf8574_1.pinMode(P5, OUTPUT);
  pcf8574_1.pinMode(P6, OUTPUT);
  pcf8574_1.pinMode(P7, OUTPUT);

  // Set pinMode to OUTPUT / input for bank 2. NOTE im using External Pull Up's you can enable internal pull up's.
  pcf8574_2.pinMode(P0, INPUT);
  pcf8574_2.pinMode(P1, INPUT);
  pcf8574_2.pinMode(P2, INPUT);
  pcf8574_2.pinMode(P3, INPUT);
  pcf8574_2.pinMode(P4, OUTPUT);
  pcf8574_2.pinMode(P5, OUTPUT);
  pcf8574_2.pinMode(P6, OUTPUT);
  pcf8574_2.pinMode(P7, OUTPUT);

  // debug print out the expander state on start up. 
  Serial.print("Init pcf8574...\n");
  if (pcf8574_1.begin()){
    Serial.println("bank1_OK\n");
  } else {
    Serial.println("Bank1_KO\n");
  }
  if (pcf8574_2.begin()){
    Serial.println("Bank2_OK\n");
  } else {
    Serial.println("Bank2_KO\n");
  }

  //set the resolution to 12 bits (0-4095) for analog. 
  analogReadResolution(12);

}

  // main loop: 
void loop() 
{

  // read the analog value for pin 0 for paddel shifters:
  int analogValue = analogRead(0);

  // Read the Expander #1
  PCF8574::DigitalInput di_1 = pcf8574_1.digitalReadAll();
  // Read the Expander #2
  PCF8574::DigitalInput di_2 = pcf8574_2.digitalReadAll();

  // left in for debug: 
  //Serial.print("READ VALUE FROM PCF di_1: ");
  //Serial.print(" sen1= "); 
  //Serial.print(di_1.p0);
  //Serial.print(" sen2= "); 
  //Serial.print(di_2.p0);
  //Serial.print(" sen3= "); 
  //Serial.print(di_1.p1);
  //Serial.print(" sen4= "); 
  //Serial.print(di_2.p1);
  //Serial.print(" sen5= "); 
  //Serial.print(di_1.p2);
  //Serial.print(" sen6= "); 
  //Serial.print(di_2.p2);
  //Serial.print(" sen7= "); 
  //Serial.print(di_2.p3);
  //Serial.print(" sen8= "); 
  //Serial.println(di_1.p3);

    // if we detect we need to set "park" VIA shifter push buttion OR paddel shifter (press both paddels at the same time) NOTE car triggers on release automaticaly... 
    if (di_1.p0 == 1 && di_2.p0 == 1 && di_1.p1 == 0 && di_2.p1 == 1 && di_1.p2 == 1 && di_2.p2 == 0 && di_2.p3 == 0 && di_1.p3 == 1 || (analogValue >= 1000 && analogValue <= 1030)) {
      Serial.println("Park"); // debug
      pcf8574_1.digitalWrite(P4, HIGH);
      pcf8574_2.digitalWrite(P4, HIGH);
      pcf8574_1.digitalWrite(P5, LOW);
      pcf8574_2.digitalWrite(P5, HIGH);
      pcf8574_1.digitalWrite(P6, HIGH);
      pcf8574_2.digitalWrite(P6, LOW);
      pcf8574_2.digitalWrite(P7, LOW);
      pcf8574_1.digitalWrite(P7, HIGH);
      //delay(100);
      }

      // if we detect we need to set "Reverse" VIA shifter OR paddle shifter (pull both paddels at the same time)
      else if (di_1.p0 == 0 && di_2.p0 == 0 && di_1.p1 == 1 && di_2.p1 == 1 && di_1.p2 == 1 && di_2.p2 == 1 && di_2.p3 == 1 && di_1.p3 == 0 || (analogValue >= 2300 && analogValue <= 2390)) { 
        Serial.println("Reverse"); // debug
        pcf8574_1.digitalWrite(P4, LOW);
        pcf8574_2.digitalWrite(P4, LOW);
        pcf8574_1.digitalWrite(P5, HIGH);
        pcf8574_2.digitalWrite(P5, HIGH);
        pcf8574_1.digitalWrite(P6, HIGH);
        pcf8574_2.digitalWrite(P6, HIGH);
        pcf8574_2.digitalWrite(P7, HIGH);
        pcf8574_1.digitalWrite(P7, LOW);
        //delay(100);
      }

      // if we detect we need to set "D / B" VIA shifter OR paddle shifter (pull ither the left or right paddel) the car will auto change from D OR B Modes.
      else if (di_1.p0 == 1 && di_2.p0 == 1 && di_1.p1 == 1 && di_2.p1 == 0 && di_1.p2 == 0 && di_2.p2 == 1 && di_2.p3 == 1 && di_1.p3 == 0 || ((analogValue >= 3190 && analogValue <= 3230) || (analogValue >= 2980 && analogValue <= 3020))) {
        Serial.println("D/B"); // debug
        pcf8574_1.digitalWrite(P4, HIGH);
        pcf8574_2.digitalWrite(P4, HIGH);
        pcf8574_1.digitalWrite(P5, HIGH);
        pcf8574_2.digitalWrite(P5, LOW);
        pcf8574_1.digitalWrite(P6, LOW);
        pcf8574_2.digitalWrite(P6, HIGH);
        pcf8574_2.digitalWrite(P7, HIGH);
        pcf8574_1.digitalWrite(P7, LOW);
      }
    
      // If the system is @ rest and no commands are set up to change the shifter, send the fallowing: VIA shifter OR paddle shifter (no paddel triggerd and no input from shifter "home pos")
      else if (di_1.p0 == 1 && di_2.p0 == 1 && di_1.p1 == 0 && di_2.p1 == 1 && di_1.p2 == 1 && di_2.p2 == 0 && di_2.p3 == 1 && di_1.p3 == 0 && analogValue >= 4000); {
        delay(100); // delay here or will not trigger change from other shift triggers...
        Serial.println("home"); // debug
        pcf8574_1.digitalWrite(P4, HIGH);
        pcf8574_2.digitalWrite(P4, HIGH);
        pcf8574_1.digitalWrite(P5, LOW);
        pcf8574_2.digitalWrite(P5, HIGH);
        pcf8574_1.digitalWrite(P6, HIGH);
        pcf8574_2.digitalWrite(P6, LOW);
        pcf8574_2.digitalWrite(P7, HIGH);
        pcf8574_1.digitalWrite(P7, LOW);
      }

      // if we detect we need to set "Netrual" VIA shifter OR paddle shifter (push ither the right OR left paddel)
    if (di_1.p0 == 1 && di_2.p0 == 0 && di_1.p1 == 0 && di_2.p1 == 0 && di_1.p2 == 1 && di_2.p2 == 1 && di_2.p3 == 1 && di_1.p3 == 0 || ((analogValue >= 2000 && analogValue <= 2045) || (analogValue >= 1390 && analogValue <= 1430))) {
      Serial.println("Netrual delay"); // debug
      delay(150);
      // Read the Expander #1
      PCF8574::DigitalInput di_1 = pcf8574_1.digitalReadAll();
      // Read the Expander #2
      PCF8574::DigitalInput di_2 = pcf8574_2.digitalReadAll();
      // check again to reduse false inputs for park
      // read the analog value for pin 0 for paddel shifters:
      int analogValue = analogRead(0);
      Serial.println("Netrual chech again"); // debug
      if (di_1.p0 == 1 && di_2.p0 == 0 && di_1.p1 == 0 && di_2.p1 == 0 && di_1.p2 == 1 && di_2.p2 == 1 && di_2.p3 == 1 && di_1.p3 == 0 || ((analogValue >= 2000 && analogValue <= 2045) || (analogValue >= 1390 && analogValue <= 1430))) {
    
        Serial.println("Netrual"); // debug
        pcf8574_1.digitalWrite(P4, HIGH);
        pcf8574_2.digitalWrite(P4, LOW);
        pcf8574_1.digitalWrite(P5, LOW);
        pcf8574_2.digitalWrite(P5, LOW);
        pcf8574_1.digitalWrite(P6, HIGH);
        pcf8574_2.digitalWrite(P6, HIGH);
        pcf8574_2.digitalWrite(P7, HIGH);
        pcf8574_1.digitalWrite(P7, LOW);
        delay(1000); // delay for 1.1 sec to trigger Netrual in car, its needs 1 second hold before it iwll trigger netrual. the delay in the loop add's an extra .1 sec. 
      }

      }

}
