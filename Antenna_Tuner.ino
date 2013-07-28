
/*
Automatic Antenna Tuner
Date: 28 July 2013
Author: Dell-Ray Sackett
Version: 0.7
Copyright (c) 2012 Dell-Ray Sackett
License: License information can be found in the LICENSE.txt file which
  MUST accompany all versions of this code.
Discription:
  This is an automatic Antenna Tuner. Right now it is designed to work 
  with an ICOM-706. It will read the frequency from the radio and then
  adjust the antenna accordingly.
Required Non-standard Libraries:
LiquidTWI: https://github.com/Stephanie-Maks/Arduino-LiquidTWI
MemoryFree: http://arduino.cc/playground/Code/AvailableMemory
*/

#include <Wire.h>
#include <LiquidTWI.h>
#include <EEPROM.h>
#include <MemoryFree.h>

/*
 *This is the debugging statement. Uncomment this if you would like to debgu
*/
//#define DEBUG


// Declare some constants
/* this is an array that will hold some of the places I will need to
  write too commonly. The order follows:
Ant CountCol, Ant Count Row, FreqCol, FreqRow, SWRCol, SWRRow, MemCol, MemRow

*/
const uint8_t characterPlaces[] = {12, 0, 6, 1, 5, 2, 8, 3};
const float MAXANTENNAHEIGHT = 3474.72;  // calculated in millimeters
const float MINANTENNAHEIGHT = 2895.6;   // see above
const float TURNDISTANCE = 6.35;         // the amount the antenna travels with one turn
const long MAXTURNS = 91;                // the maximum number of turns the antenna can handle
const int BAUD = 9600;
const int RXBUFSIZE = 16;
const int CTRL_UP = 5;
const int CTRL_DOWN = 4;
const int CTRL_TUNE = 8;                // are we tuning??
const int RELAY_A = 7;                  // called UNIT_A in sch
const int RELAY_B = 6;                  // called UNIT_B in sch
const byte XCVR_ADDR = 0x58;
const byte MCU_ADDR = 0xE0;
const byte BACKPACK_ADDR = 0x00;
const byte DEBUG_SCREEN_ADDR = 0x01;

// Memory maping for the EEPROM
const byte EEPROM_TURNS = 0;    // long so writes through 3


// Variables and objects
LiquidTWI lcd(BACKPACK_ADDR);
// Part of a new debugging system.
#ifdef DEBUG
  LiquidTWI dlcd(DEBUG_SCREEN_ADDR);
#endif
boolean screenUpdatesPending = true;
boolean movingUp = false;
boolean movingDown = false;
boolean moving = false;
boolean tuning = false;
boolean freqReady = false;
boolean upPressed = false;
boolean downPressed = false;
byte inBuf[RXBUFSIZE];
byte rcvChar = 0x00;
byte longBuf[4];
long freqOut = 0;
long freq = 0;
long tempFreq = 0;
long turnCount = 0;
long turnTemp = 0;
long targetTurns = 0;
float wavelengthTemp = 0.00;
String frequencyStr = "12.34";
String antCountStr = "1234";
String swrStr = "1234";
String memoryStr = "12.34";
String debugStr = "Debug!";



void setup()
{
    // Setup the interrupt for the turn counter.
    attachInterrupt(0, turnCounter, HIGH);
    
    // Setup the pins
    pinMode(CTRL_UP, INPUT);
    pinMode(CTRL_DOWN, INPUT);
    pinMode(CTRL_TUNE, INPUT);
    pinMode(RELAY_A, OUTPUT);
    pinMode(RELAY_B, OUTPUT);
    pinMode(13, OUTPUT);
    digitalWrite(RELAY_A, LOW);
    digitalWrite(RELAY_B, LOW);
    
    // Start the Serial port and the LCD, Clear the LCD, and get good values for the strings.
    Serial.begin(BAUD);        // Start the Serial port at the appropriate baud rate.
    lcd.begin(20, 4);          // Start the LCD at the correct Size.
    lcd.clear();               // Clear anything off of the LCD. (Mostly for resets).
    updateFrequency();         // Get the current Frequency of the radio.
    #ifdef DEBUG
      dlcd.begin(16, 2);
      lcd.clear();
    #endif
    
    // Restore the current turn count
    turnCount = readTurns();
}

void loop()
{
    // I want as little to happen here as possible unless something is going on.
    // I have done my best to make this even triggered instead of "free-running".
    if (screenUpdatesPending)
    {
        // This means that something has changed on of the strings used to print data to the screen.
        // If those strings have been changed and the information not written the screen is out of date
        // so here we update it.
        updateScreen();
    }
    // Decide if up is pressed. If it isn't set its bool to false
    if(digitalRead(CTRL_UP) == LOW)
    {
      upPressed = true;
    } else
    {
      upPressed = false;
    }
    // same but for down.
    if(digitalRead(CTRL_DOWN) == LOW)
    {
      downPressed = true;
    } else
    {
      downPressed = false;
    }
    
    // Read the control pins and drive the motor if necessary
    if(upPressed && !movingUp)
    {
        moveUp();
    }
    if(downPressed && !movingDown)
    {
        moveDown();
    }
    if(!upPressed && !downPressed && moving == true)
    {
        moveStop();
        writeTurns(turnCount);
    }
    if(digitalRead(CTRL_TUNE) == HIGH && !tuning)
    {
       // Some one has told us to autotune.
       // update the frequency and tell it it's tuning 
       updateFrequency();
       tuning = true;     
    }
    if(tuning == true)
    {
      // time to tune!
        if(freqReady == true)
        {
           // this is the first round through the tuning game so calculate the wavelength and the counts
           targetTurns = calculateTurns(calculateWavelength(freq));
           freqReady = false;
        }
        if(turnCount > targetTurns)
        {
          moveDown();
        } else if(turnCount < targetTurns)
        {
          moveUp();
        } else {
          moveStop();
          tuning = false;
        }
    }
    
    //debugStr = String(freeMemory());
}


// Functions!!!
// ----------------------------------------------------Screen functions------------------------------------------------------------

void screenLayout()
{
    // This function lays out the screen
    lcd.home();
    lcd.print("Ant. Count: ");
    lcd.setCursor(0, characterPlaces[3]);
    lcd.print("Freq: ");
    lcd.setCursor(0, characterPlaces[5]);
    lcd.print("SWR: ");
    lcd.setCursor(0, characterPlaces[7]);
    lcd.print("Memory: ");
}

void updateScreen()
{
  // This function updates the current display
    lcd.clear();
    screenLayout();
    lcd.setCursor(characterPlaces[0], characterPlaces[1]);
    lcd.print(String(turnCount) + "Trns");
    lcd.setCursor(characterPlaces[2], characterPlaces[3]);
    lcd.print(frequencyStr + "MHz");
    lcd.setCursor(characterPlaces[4], characterPlaces[5]);
    lcd.print(swrStr);
    // This formats and prints the debug string. This is so I can debug without the serial port
    // which is being used to talk to the radio. If the string is too long to fit in the space
    // the characters TL will display instead.
    if(debugStr.length() < (20 - (characterPlaces[4] + swrStr.length())))
    {
        lcd.setCursor((20 - debugStr.length()), characterPlaces[5]);
        lcd.print(debugStr);
    } else {
        lcd.setCursor(18, characterPlaces[5]);
        lcd.print("TL");
    }
    lcd.setCursor(characterPlaces[6], characterPlaces[7]);
    lcd.print(memoryStr + "MHz");  
 
    screenUpdatesPending = false;
}

// ----------------------------------------------------Radio functions------------------------------------------------------------
void radioWrite(byte radioAddr, byte mcuAddr, byte command)
{
    // Write a command to the XCVR
    Serial.write(0xFE);
    Serial.write(0xFE);
    Serial.write(radioAddr);
    Serial.write(mcuAddr);
    Serial.write(command);
    Serial.write(0xFD);
}

void radioWrite(byte radioAddr, byte mcuAddr, byte command, byte subCommand)
{
    // Write a command and subcommand to the XCVR
    Serial.write(0xFE);
    Serial.write(0xFE);
    Serial.write(radioAddr);
    Serial.write(mcuAddr);
    Serial.write(command);
    Serial.write(subCommand);
    Serial.write(0xFD);
}

boolean rxFromRadio(byte radioAddr, byte mcuAddr)
{
    while(Serial.available())
    {
        rcvChar = Serial.read();
        if(rcvChar == 0xFE)
        {
            // Might be a message
            rcvChar = Serial.read();
            if(rcvChar == 0xFE)
            {
                // For sure a message
                rcvChar = Serial.read();
                if(rcvChar == mcuAddr)
                {
                    // A message for me
                    rcvChar = Serial.read();
                    if(rcvChar == radioAddr)
                    {
                        // A message for me from the right guy
                        rcvChar = Serial.read();
                        if(rcvChar == 0x03)
                        {
                            // Frequency message.
                            rcvChar = Serial.read();
                            if(rcvChar == 0xFD)
                            {
                                // this is just an ok message don't fill buffer
                            } else {
                                // Actual frequency information
                                for(int count = 0; count < RXBUFSIZE; count++)
                                {
                                    if(rcvChar == 0xFD)
                                    {
                                        inBuf[count] = rcvChar;
                                        return true;
                                    } else {
                                        inBuf[count] = rcvChar;
                                    }
                                    rcvChar = Serial.read();
                                }
                            }
                         // End of frequency message section
                        }
                        if(rcvChar == 0x15)
                        {
                            // Levels and status Message
                            rcvChar = Serial.read();
                            if(rcvChar == 0x12)
                            {
                                //SWR Message
                                rcvChar = Serial.read();
                                for(int count = 0; count < RXBUFSIZE; count++)
                                {
                                    if(rcvChar == 0xFD)
                                    {
                                        inBuf[count] = rcvChar;
                                        return true;    
                                    } else {
                                        inBuf[count] = rcvChar;
                                    }
                                    rcvChar = Serial.read();
                                }
                                //SWR Message                                
                            }
                            // End of Levels and Status Message
                        }
                    }                
                }
            }
        }
    }
    return false;
}

long getFrequency(byte radioAddr, byte mcuAddr)
{
      // get the frequency
      // Clear this out or you are going to have a bad time.
      freqOut = 0;
      radioWrite(radioAddr, mcuAddr, 0x03);
      Serial.flush();
      delay(200);
      if(rxFromRadio(XCVR_ADDR, MCU_ADDR))
      {
          for(int count = 0; count < RXBUFSIZE && inBuf[count] != 0xFD; count++)
          {
              if(count == 0)
              {
                   // one and tens hertz
                   freqOut += (inBuf[count] & B1111);
                   freqOut += (inBuf[count] >> 4) * 10;
                   inBuf[count] = 0;
              }else if (count == 1)
              {
                   // 100 hertz and 1KHz
                   freqOut += (inBuf[count] & B1111) * 100;
                   freqOut += (inBuf[count] >> 4) * 1000;
                   inBuf[count] = 0;
              }else if (count == 2)
              {
                   // 10KHz & 100Khz
                   freqOut += (inBuf[count] & B1111) * 10000L; // HAS TO BE A LONG IDFK WHY
                   freqOut += (inBuf[count] >> 4) * 100000;
                   inBuf[count] = 0; 
              }else if (count == 3)
              {
                   // 1Mhz & 10Mhz
                   freqOut += (inBuf[count] & B1111) * 1000000;
                   freqOut += int(inBuf[count] >> 4) * 10000000;
                   inBuf[count] = 0;
              } else if (count  == 4)
              {
                   // 1ghz & 100mhz
                   freqOut += (inBuf[count] & B1111) * 100000000;
                   freqOut += (inBuf[count] >> 4) * 1000000000;
                   inBuf[count] = 0;
              }
          }
          return freqOut;
       } else {
           // Couldn't read from radio.
           return -1;
       }
   // Shouldn't ever happen but here to be safe.
   return -2;
}

byte getSWR(byte radioAddr, byte mcuAddr)
{
    // Get the SWR value from the radio
    radioWrite(radioAddr, mcuAddr, 0x15, 0x12);
    Serial.flush();
    delay(200);
    if(rxFromRadio(radioAddr, mcuAddr))
    {
         // The buffer should only contain the good SWR data and 0xFD or the No Good command and 0xFD.
         return inBuf[0];
    } else {
        // The radio couldn't be read.
        return -1;
    }
}

//---------------------------------------------Frequency Calculations----------------------------------------------------------------
void updateFrequency()
{
    /*
    This function will get the frequency the radio is operating at in Hertz.
    Then it will set it up to be displayed properly and store the string in
    the frequencyStr variable. I would have used a double here but per the
    Arduino IDE documentation double is actually just a float so I would lose
    a gread deal of precision storing it as GMMM.KKKHHH because everything
    past 2 Dicimal places would be destoyed.
    */
    freq = getFrequency(XCVR_ADDR, MCU_ADDR);                         // Get the frequency in Hertz.
    tempFreq = freq % 1000000;                                        // Temporarily store the Khz and Hertz portion.
    frequencyStr = String(freq/1000000) + "." + String(tempFreq);     // Concatenate the GHz/MHz and KHz/Hz portions into a String.
    screenUpdatesPending = true;                                      // This triggers the screen update in the main loop.
    tempFreq = 0;                                                     // Clear out tempFreq. freq is used else where so it should remain.
    freqReady = true;
}

void updateSWR()
{
    swrStr = String(getSWR(XCVR_ADDR, MCU_ADDR), DEC);                // The SWR comes back as a decimal number in byte form so make it a String.   
       
}

//--------------------------------------------------wavelength and turn calculations-------------------------------------------------

float calculateWavelength(long frequency)
{
  // takes in a frequency in hertz and returns a wavelength in millimeters
  // takes frequency is a long return is a float.
  return 300000000000.0/frequency;
}


long calculateTurns(float waveLength)
{
  // take a float of wavelength and calculate the number of turns from 0 the antenna needs
  // to be to match that.
  wavelengthTemp = waveLength;
  while(wavelengthTemp > MAXANTENNAHEIGHT)
  {
     wavelengthTemp = wavelengthTemp / 2;
     if(wavelengthTemp < MINANTENNAHEIGHT)
     {
       return -1;
     }   
  }
  turnTemp = wavelengthTemp / TURNDISTANCE;
  if (turnTemp > MAXTURNS)
  {
    return -1;
  }
  return turnTemp;
  
}

// ----------------------------------------------------movement functions------------------------------------------------------------
void turnCounter()
{
    // This will be called of the reed switch goes off, if we are driving the motor up then increment the turnCount variable
    // else decrement it.
   noInterrupts();
   if(movingUp)
   {
      turnCount++;
   } else if(movingDown) {
      turnCount--;
   }
   interrupts();
   debugStr = "Sensor!";
}

void moveUp()
{
  // This functions commands the antenna up
  digitalWrite(RELAY_A, HIGH);
  digitalWrite(RELAY_B, LOW);
  movingUp = true;
  movingDown = false;
  moving = true;
}

void moveDown()
{
    // This function commands the antenna down.
    digitalWrite(RELAY_B, HIGH);
    digitalWrite(RELAY_A, LOW);
    movingUp = false;
    movingDown = true;
    moving = true;
}

void moveStop()
{
    // This function commands the antenna to stop
    digitalWrite(RELAY_A, LOW);
    digitalWrite(RELAY_B, LOW);
    movingUp = false;
    movingDown = false;
    moving = false;
}

// ----------------------------------------------------EEPROM functions------------------------------------------------------------
void writeTurns(long turns)
{
  // Break up turns into 4 bytes to be written to the EEPROM
  for (int count = 0; count <= 4; count++)
  {
    longBuf[count] = turns && 11111111;
    turns >> 8;
  }
  
  // Write the data to the EEPROM
  for (int count = 0; count <=4; count++)
  {
    EEPROM.write(count, longBuf[count]);
  }  
}

long readTurns()
{
  long turns = 0;
  for (int count = 0; count <=4; count++)
  {
   turns = turns || EEPROM.read(count);
   if (count < 4)
    {
     turns << 8;
    }
  }
  
  return turns; 
}
