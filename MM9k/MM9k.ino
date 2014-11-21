/*
 * Mega Menorah 9000 Firmware, Rev A
 *
 * Uses light_ws2812 library.
 *
 * Created: November 3, 2014
 * Author: Windell Oskay (www.evilmadscientist.com)
 *
 */

#include <WS2812.h>
#define F_CPU 16000000
#ifdef __AVR_ATtiny85__ // e.g., Adafruit Trinket
#include <avr/power.h>
#endif

#include <avr/eeprom.h> 

#define outputPin 0  // Digital output pin (default: 7)
#define LEDCount 9   // Number of LEDs to drive (default: 9)

#define BrightnessAbsMax 255  // Maximum LED value per channel
#define BrightnessLowSetpoint 16 // "Dim" LED brightness


#define LighterLoc 4  // Shamash location
#define msInterval 10  // Milliseconds between updates

#define ColorNo 24 // Number of color settings

#define shortdelay(); 	asm("nop\n\t"  "nop\n\t"  "nop\n\t"  "nop\n\t");

uint16_t eepromWord __attribute__((section(".eeprom")));
uint16_t pt;

uint8_t LEDs[9]; // Storage for current LED intensities
uint8_t LEDsFlicker[9]; // Flicker-variable Storage for current LED values

uint8_t night, color, Mode;
uint8_t brightmax, brightmaxCopy; 

uint8_t debounce, debounce2;
uint8_t debounceC, debounceC2;

uint8_t modeswitched, UpdateConfig;
uint8_t dimmed;
uint8_t	demoMode;

uint8_t CycleCountOverflow;
unsigned int CycleCountLow;	
unsigned int flickercounter=0;	
unsigned int flickerPrescale=0;	
unsigned int brightStep = 1;

unsigned long millisNext;

byte FadeCounter;
byte FadeIntensity;

byte sign;
byte phase;
byte buttonPress = 0;

WS2812 LED(LEDCount); 


void setup() {

#ifdef __AVR_ATtiny85__ // Trinket, Gemma, etc.
  if(F_CPU == 16000000) clock_prescale_set(clock_div_1);
  // Seed random number generator from an unused analog input:
  randomSeed(analogRead(2));
#endif

  LED.setOutput(outputPin); // Digital Pin 7

  /* You may uncomment one of the following three lines to switch 
   to a different data transmission sequence for your addressable LEDs. */

  LED.setColorOrderRGB();  // Uncomment for RGB color order
  //LED.setColorOrderBRG();  // Uncomment for BRG color order
  //LED.setColorOrderGRB();  // Uncomment for GRB color order (Default; will be used if none other is defined.)


  // For rainbow fades:
  FadeIntensity = 0;
  sign = 1;
  phase = 0;


  // Button setup  
  digitalWrite(1, LOW);
  digitalWrite(2, HIGH);  
  pinMode(1, INPUT);
  pinMode(2, INPUT); 


  CycleCountLow = 0;
  CycleCountOverflow = 0;

  debounce = 1;   // Advance the night at reset (as though Night button has breen pressed).
  debounce2 = 1;
  debounceC = 0;
  debounceC2 = 0;


  demoMode = 0;
  dimmed = 0;
  Mode = 0;
  night = 5;
  color = 0;
  brightmax = 0; 
  brightmaxCopy = 0;
  modeswitched = 0; 

  UpdateConfig = 0;
  FadeCounter = 0;


  if (PINB & _BV(1)) // If Night button is pressed at turn-on, enable "demo mode"
  { 
    demoMode = 1;
  }


  pt = (uint16_t) (eeprom_read_word(&eepromWord)) ;
  byte setDefaults = 0;

  color = pt >> 8;
  pt &= 0xFF;
  Mode = pt >> 4;
  night = (pt & 15) - 1;


  if (color > ColorNo)
    setDefaults = 1;
  if (Mode > 3)
    setDefaults = 1;
  if (night > 7) 
    setDefaults = 1;

  if (setDefaults)
  {
    night = 0;
    Mode = 0;
    color = 0;
  }




  if ((Mode & 1) == 0) // Modes 0, 2 
    brightmax = BrightnessLowSetpoint;   
  else	 
    brightmax = BrightnessAbsMax;  

  brightmaxCopy = brightmax;

  byte  j = 0;
  while (j < 9) {  
    LEDs[j] = 0;  
    LEDsFlicker[j] = 0;
    j++;

  }  	

  // Delay before turning LEDs on.
  CycleCountLow = 0;
  while (CycleCountLow < 10000) {
    shortdelay();    
    CycleCountLow++; 
  }  	

  CycleCountLow = 0;

  millisNext =  millis() + msInterval;
}

void loop() {

  byte pinBtemp;
  byte i, j;
  byte briteTemp;
  byte temp;


  // Button setup.  (repeated in loop.)
  digitalWrite(1, LOW);
  digitalWrite(2, HIGH);  
  pinMode(1, INPUT);
  pinMode(2, INPUT); 

  CycleCountLow++;
  if (CycleCountLow > 576) {
    CycleCountOverflow = 1;
    CycleCountLow = 0;

    if (demoMode) {
      night = 7;


      CycleCountOverflow = 0; 
      j = 0;
      while (j < 9) {

        LEDs[j] = 0;  
        LEDsFlicker[j] = 0;
        j++; 
      } 
    }
  } 

  if (UpdateConfig){		// Need to save configuration byte to EEPROM
    if (CycleCountLow > 10) // Avoid burning EEPROM in event of flaky power connection resets
    {
      UpdateConfig = 0;
      pt = (Mode << 4) | (night + 1) | (color << 8); 
      eeprom_write_word(&eepromWord, (uint16_t) pt);	
    }
  }
  else
  {

    pinBtemp = PINB;   // Read current input button state

    byte nightButton = ((pinBtemp & _BV(1)) != 0); // Check if Night button currently pressed pressed down
    byte colorButton = ((pinBtemp & _BV(2)) == 0); // Check if Color button currently pressed pressed down

    if (dimmed )
    {
      if (nightButton && (LEDs[LighterLoc] == 0))		
      {  
        debounce = 1;	
      }
      else{	// If button is NOT pressed
        if (debounce)   // If button has just been released from momentary press
        {   
          debounce = 0;
          dimmed = 0;

          if ((Mode & 1) == 0) // Modes 0, 2 
            brightmax = BrightnessLowSetpoint;   
          else	 
            brightmax = BrightnessAbsMax;   

          brightmaxCopy = brightmax;

          CycleCountOverflow = 0;
          CycleCountLow = 0;
        } 
      } 


    }
    else
    {
      // NIGHT button:
      // When pressed: Cycle to next night
      // When held: Turn off LEDs (for graceful shutdown).

      if (nightButton)		
      { 
        if (debounce2 > 100) 
        {   
          if (dimmed == 0)
          {  
            debounce = 0;	// Holding; not a momentary press. 
            dimmed = 1;       
            brightmax = 0;      // Set maximum brightness to zero, dimming LEDs.
          }
        }
        else {
          debounce = 1;		// Flag that the button WAS pressed.
          debounce2++;	
        } 
      }	
      else{	// If button is NOT pressed
        debounce2 = 0;  
        if (debounce)   // If button has just been released from momentary press
        {   
          debounce = 0;
          night++;
          if (night > 7)
            night = 0; 
          UpdateConfig = 1;	// We need to save the new night to EEPROM.
          CycleCountOverflow = 0;
          CycleCountLow = 0;
          j = 0;
          while (j < 9) {
            LEDs[j] = 0;  
            j++;
          }  
        } 
      } 

      // COLOR button:
      // When pressed: Cycle to next color in sequence.
      // When held: Cycle between four modes: Dim/bright & flickering/steady

      if (colorButton)  // Check if Color button currently pressed pressed down
      { 
        if (debounceC2 > 100) 
        {   
          if (modeswitched == 0)
          {  
            debounceC = 0;	// If we're holding, this isn't a momentary press. 
            modeswitched = 1;
            Mode++;
            UpdateConfig = 1;
            if (Mode > 3) 
              Mode = 0;   
            if ((Mode & 1) == 0) // Modes 0, 2 
              brightmax = BrightnessLowSetpoint;   
            else	 
              brightmax = BrightnessAbsMax;   

            brightmaxCopy = brightmax;
          }
        }
        else {
          debounceC = 1;		// Flag that the button WAS pressed.
          debounceC2++;	
        } 
      }	
      else{	// If button is NOT pressed
        debounceC2 = 0; 
        modeswitched = 0;
        if (debounceC)
        {   
          debounceC = 0;

          color++;
          if (color >= ColorNo)
            color = 0;   

          UpdateConfig = 1;	// We need to save the new color setting to EEPROM.
          CycleCountOverflow = 0;
          CycleCountLow = 0;
          //          j = 0;
          //          while (j < 9) {
          //            LEDs[j] = 0;  
          //            j++;
          //          }  


        } 
      }  
    }

    if ((CycleCountOverflow == 0) && (CycleCountLow < 576))	// Turn-on sequence
    {

      if (LEDs[LighterLoc] < brightmax)
        LEDs[LighterLoc]++;
      if (LEDsFlicker[LighterLoc] < brightmax)
        LEDsFlicker[LighterLoc]++;

      j = 0;
      while (j < (night + 1)) { 

        if (CycleCountLow > (64 * ( j + 1 )))	
        {
          i = 7 - night + j; 

          if (i >= LighterLoc)
            i++; 
          if (LEDs[i] < brightmax)
            LEDs[i]++;
          if (LEDsFlicker[i] < brightmax)
            LEDsFlicker[i]++;				
        }
        j++;
      }
    }
    else {
      j = 0;
      while (j < 9) {		// Fade up to high power mode
        if (LEDs[j] < brightmax){
          if (LEDs[j] > 0)		// Only turn up ones that are already on.
            LEDs[j]++;   
        }
        if (LEDsFlicker[j] < brightmax){
          if (LEDsFlicker[j] > 0)		// Only turn up ones that are already on.
            LEDsFlicker[j]++;   
        }			
        j++;
      }  
    }

    j = 0;
    while (j < 9) {			// Fade down to low power mode (also used when dimmed).
      if (LEDs[j] > brightmax)
        LEDs[j]--;  
      if (LEDsFlicker[j] > brightmax)
        LEDsFlicker[j]--;  
      j++;
    }  

    if (Mode > 1) {		
      // Flicker mode, for Mode 2 and 3.

      flickerPrescale++;

      if (brightmax > 64)
      {  
        i = 7; // Bright modes flickering
        brightStep = brightmax >> 3;
      }
      else
      { 
        i = 14;  // Dim mode flickering
        brightStep = brightmax >> 2;  
      }
      if ((flickerPrescale & i) == 0) {		//Every N passes through...

        j = 0;
        while (j < 9) { 
          flickercounter += 23 ; // Rate of counter increase 
          if ((flickercounter & 127) < 64) { // ***Worlds crappiest pseudorandom number generator!*** //

            if (LEDsFlicker[j] < (brightmax - brightStep) ) {
              LEDsFlicker[j] += brightStep;
            } 
          } 
          else { 
            if (LEDsFlicker[j] >= brightStep ) {
              LEDsFlicker[j] -= brightStep;
            }	
          }
          if (LEDs[j] == 0)
            LEDsFlicker[j] = 0;
          j++;	 
        }
      } 
    }  	   
  }

  cRGB value; 

  byte colorPortionA, colorPortionB;
  byte brightHalf, brightQuarter;


  // Manage color fades at rate independent of brightness:

  // 
  byte phaseSpeedHigh = 0;




  i = 9;
  while (i != 0)
  {
    i--;

    if (Mode > 1){
      briteTemp = LEDsFlicker[i];
    }
    else{
      briteTemp = LEDs[i];
    }


    if (color < 12)
    {  // Cycle around the color wheel in even steps.
      j = color & 3U;

      brightHalf = briteTemp >> 1;
      brightQuarter = brightHalf >> 1;

      if (j == 0)
        colorPortionA = 0;
      else if (j == 1)
        colorPortionA = brightQuarter; 
      else if (j == 2)
        colorPortionA = brightHalf;
      else
        colorPortionA = brightHalf + brightQuarter; 

      // Thus, if (color % 4) == 0, then colorPortionA = 0.
      // Thus, if (color % 4) == 1, then colorPortionA = briteTemp / 4
      // Thus, if (color % 4) == 2, then colorPortionA = briteTemp / 2
      // Thus, if (color % 4) == 3, then colorPortionA = briteTemp * 3/4

      colorPortionB = briteTemp - colorPortionA;

      if (color > 7)
      { // Colors 8 - 11: blue, violet, purple, maroon
        value.g = 0;
        value.r = colorPortionA; // Increasing 
        value.b = colorPortionB; // Decreasing
      }
      else if (color > 3)
      {  // Colors 4 - 7: green, aqua, teal, royal blue
        value.g = colorPortionB; // Decreasing
        value.r = 0;
        value.b = colorPortionA; // Increasing 
      }
      else
      {  // Colors 0 - 3: Red, orange, yellow, yellow-green
        value.g = colorPortionA; // Increasing 
        value.r = colorPortionB; // Decreasing
        value.b = 0;
      }
    }

    else if (color == 12)
    { // white
      colorPortionA = briteTemp / 3;
      value.g = colorPortionA; 
      value.r = colorPortionA; 
      value.b = colorPortionA;
    }
    else if (color == 13)
    { // warm white
      colorPortionA = briteTemp / 3;
      colorPortionB = colorPortionA >> 2;

      value.g = colorPortionA + colorPortionB; 
      value.r = value.g; //colorPortionA + colorPortionB;

      colorPortionB += colorPortionB;
      if (colorPortionA > colorPortionB)
        value.b = colorPortionA - colorPortionB;
      else
        value.b = 0;

    } 
    else if (color < 18)
    { 
      // RGB Color cycling
      // Color codes 14 - 17
      // 14: Per-LED color cycling, fast
      // 15: Per-LED color cycling, slow
      // 16: All-together color cycling, fast
      // 17: All-together color cycling, slow

      if (color > 15)
        temp = phase % 6; //color 16 & 17: i Cycle all LEDs together
      else  // color 14 & 15: individual cycles
      temp = (i + phase) % 6;  // Cycle LEDs individually

      if ((color & 1) == 0)
        phaseSpeedHigh = 1;

      if (FadeIntensity > briteTemp)
        colorPortionA = briteTemp; // Limit value, to prevent "negative" brightness.
      else  
        colorPortionA = FadeIntensity;

      if (temp == 0)   // First LED, and every third after that
      {
        value.b = briteTemp - colorPortionA; 
        value.g =  0; 
        value.r = colorPortionA; 
      }
      else if (temp == 1) 
      {
        value.b = 0; 
        value.g = 0; 
        value.r = briteTemp; 
      }
      else if (temp == 2) 
      { 
        value.b = 0; 
        value.g = colorPortionA; 
        value.r = briteTemp - colorPortionA; 
      }
      else if (temp == 3) 
      {
        value.b = 0;
        value.g = briteTemp; 
        value.r = 0; 
      }
      else if (temp == 4) 
      {
        value.b = colorPortionA; 
        value.g = briteTemp - colorPortionA;  
        value.r = 0;
      }
      else if (temp == 5) 
      { 
        value.b = briteTemp; 
        value.g = 0; 
        value.r = 0; 
      }
    }
    else if (color < 21)
    {
      // Red-Green Color cycling.  This is just... so... wrong... that it's hilarious.
      // Color codes 18 - 21
      // 18: Per-LED color cycling, fast
      // 19: Per-LED color cycling, slow
      // 20: Fixed

      if (color < 20)
        temp = ((i + phase) & 1);  // Cycle LEDs individually
      else 
        temp = (i & 1);  // Fixed, phase 1

      if ((color & 1) == 0)
        phaseSpeedHigh = 1;


        if (temp == 0)   // First LED, and every second after that
      {
        value.b = 0; 
        value.g = briteTemp;  
        value.r = 0;
      }
      else 
      {
        value.b = 0; 
        value.g = 0; 
        value.r = briteTemp; 
      }

    }
    else 
    {
      // Blue-White Color cycling. 
      // Color codes 18 - 21
      // 21: Per-LED color cycling, fast
      // 22: Per-LED color cycling, slow
      // 23: Fixed, phase 0


      if (color < 23)
        temp = ((i + phase) & 1);  // Cycle LEDs individually
      else
        temp = (i & 1);  // Fixed, phase 1

      if (color & 1)
        phaseSpeedHigh = 1;


        if (temp == 0)   // First LED, and every third after that
      {
        value.b = briteTemp; 
        value.g = 0;  
        value.r = 0;
      }
      else 
      {
        colorPortionA = briteTemp / 2;
        value.g = colorPortionA; 
        value.r = colorPortionA; 
        value.b = colorPortionA;
      }
    }

    LED.set_crgb_at(i, value); // Set value at LED found at index i
  }



  FadeCounter++;

  uint16_t IntensityTemp = FadeCounter * brightmaxCopy;

  if (phaseSpeedHigh)
    temp = 64;  // Fast mode
  else 
    temp = 255;  

  FadeIntensity = (IntensityTemp / temp);
  if (FadeIntensity >= brightmaxCopy)
  {
    FadeIntensity = 0;
    FadeCounter = 0;
    phase++;
    if (phase > 5)
      phase = 0;  
  }
















  LED.sync(); // Sends the data to the LEDs

  while (millis() < millisNext)
  {
    ;
  }
  millisNext += msInterval;

}























