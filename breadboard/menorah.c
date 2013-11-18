/*
menorah.c
LED Menorah Kit program

Written by Windell Oskay, http://www.evilmadscientist.com/

 Copyright 2013 Windell H. Oskay
 Distributed under the terms of the GNU General Public License, please see below.
 
 
 
 
 

 An avr-gcc program for the Atmel ATTiny2313  
 
 
 
 
 Version 1.4   Last modified   11/12/2013
 Breadboard version -- For the Evil Mad Scientist Deluxe Electronics Breadboard Menorah Kit
 http://shop.evilmadscientist.com/productsmenu/tinykitlist/672
 

 
 
 Version 1.3   Last modified   11/19/2011
  Added new LED flicker mode
 
 
 
 Version 1.2   Last Modified:  11/12/2009. 
 Written for Evil Mad Scientist Deluxe LED Menorah Kit, based on the "ix" circuit board. 
 
 
 More information about this project is at 
 http://www.evilmadscientist.com/
 
 
 
 -------------------------------------------------
 USAGE: How to compile and install
 
 
 
 A makefile is provided to compile and install this program using AVR-GCC and avrdude.
 
 To use it, follow these steps:
 1. Update the header of the makefile as needed to reflect the type of AVR programmer that you use.
 2. Open a terminal window and move into the directory with this file and the makefile.  
 3. At the terminal enter
 make clean   <return>
 make all     <return>
 make install <return>
 4. Make sure that avrdude does not report any errors.  If all goes well, the last few lines output by avrdude
 should look something like this:
 
 avrdude: verifying ...
 avrdude: 1036 bytes of flash verified
 
 avrdude: safemode: lfuse reads as 62
 avrdude: safemode: hfuse reads as DF
 avrdude: safemode: efuse reads as FF
 avrdude: safemode: Fuses OK
 
 avrdude done.  Thank you.
 
 If you a different programming environment, make sure that you copy over the fuse settings from the makefile.
 
 
 -------------------------------------------------
 
 This code should be relatively straightforward, so not much documentation is provided.  If you'd like to ask 
 questions, suggest improvements, or report success, please use the evilmadscientist forum:
 http://www.evilmadscientist.com/forum/
 
 
 -------------------------------------------------
 
  
*/

// This delay is trimmed for length; careful if you tweak it. :)
#define shortdelay(); 			asm("nop\n\t" \
"nop\n\t" \
"nop\n\t" \
"nop\n\t");

#define shortdelay2(); 			asm("nop\n\t" \
"nop\n\t" \
"nop\n\t" \
"nop\n\t" \
"nop\n\t" \
"nop\n\t");


// APPROXIMATE cycles per minute, measured. 
// (This represents the time that 64 refresh cycles take up.)
// Approx 9 kHz blinking rate. :)
#define CyclesPerSecond		143    

// 1 hour (approx) timeout, roughly 4 s per unit (576 cycles)
#define TimeMax  900

// Auto-sleep after one hour: disabled by default.
#define AllowSleep 0


#include <avr/io.h>  
#include <avr/eeprom.h> 


uint16_t eepromWord __attribute__((section(".eeprom")));





int main (void)
{
	uint8_t LEDs[9]; // Storage for current LED values
	uint8_t LEDsFlicker[9]; // Flicker-variable Storage for current LED values

	
	uint8_t j, m, bt;
	
	uint8_t loopcount;
	uint8_t night, LighterLoc;

	uint8_t delaytime;

	uint8_t  pt, debounce, brightmax;
	uint8_t  modeswitched, UpdateConfig;
	uint8_t	allowsleep, demoMode;
	uint8_t debounce2, Mode; 
 
	uint8_t LED0, LED1, LED2, LED3, LED4, LED5, LED6, LED7, LED8;

	unsigned int CycleCountLow, CycleCountOverflow;	
	unsigned int flickercounter=0;	
	unsigned int flickerPrescale=0;	
	unsigned int brightStep = 1;
	
	
	
	////////////////////////////////////////////////////
	
	//Timing controlled by 16-bit timer 1, a free running timer at the CPU clock rate
	//
	//
	////////////////////////////////////////////////////
	
	
	TCCR1A = 0;
	
	//TCCR1A:
	// Bits 7-4:  0000, Normal port operation, OC1A/B disconnected
	// Bits 1,0: 00, WGM, normal mode
	 
	// Set counter to zero.
	TCNT1 = 0;
	TCCR1B =  1;		// Start Counter
	
	
//Initialization routine: Clear watchdog timer-- this can prevent several things from going wrong.		
MCUSR &= 0xF7;		//Clear WDRF Flag
WDTCSR	= 0x18;		//Set stupid bits so we can clear timer...
WDTCSR	= 0x00;

	MCUCR &= 207; //Disable sleep mode
	
//Data direction register: DDR's
//Port A: 0, 1 are inputs.	
//Port B: 0-3 are outputs, B4 is an input.	
//Port D: 1-6 are outputs, D0 is an input.
	
	DDRA = 0U;		// A are Inputs
	DDRB = 179U;        // Outputs: 0,1,4,5,7
	DDRD = 59U;        // Outputs: 0,1,3,4,5
	
    // Button input: PA1
    
    
	PORTA = 3;	// Pull-up resistors enabled, PA0, PA1
	PORTB = 0;	// Pull-up resistor enabled, bt4
	PORTD = 0;
	
/* Visualize outputs:
 
 L to R:
 
 D2 D3 D4 D5 D6 B0 B1 B2 B3	
  
*/ 
 
		allowsleep = AllowSleep;	// Compile option required to re-activate this.
	 
	
	CycleCountLow = 0;
	CycleCountOverflow = 0;
	
	debounce = 1; 
	debounce2 = 1;
	loopcount = 254; 
	delaytime = 0;
	
	demoMode = 0;
	
	Mode = 0;
	night = 5;
	brightmax = 0; 
	modeswitched = 0; 

	UpdateConfig = 0;
	LighterLoc = 4; 
	
	
if ((PINA & 1) == 0)		// Check if location PA0 is shorted
{  
	LighterLoc = 8;  // Right-hand side lighter candle.
}

	
if ((PINA & 2) == 0)		// Check if button currently pressed pressed down
{  // If button is pressed at turn-on, enable "demo mode"
	demoMode = 1;
}
	
	
	if ( eeprom_read_word(&eepromWord) == 0xffff){
		//EEPROM byte missing or corrupted.
		night = 0U; 
		Mode = 0;
	}
	else	{
		pt = (uint8_t) (eeprom_read_word(&eepromWord)) ;
		Mode = pt >> 4;
		night = (pt & 15) - 1;
		
		if (Mode > 3)
			Mode = 0;
		if (night > 7) {
			night = 0;
		}
		
 
	}
	 	
	if ((Mode & 1) == 0) // Modes 0, 2 
		brightmax = 16;   
	else	 
		brightmax = 64;  
	
	
	j = 0;
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
		
	
	
for (;;)  // main loop
{
 
	CycleCountLow++;
	if (CycleCountLow > 576) {
		CycleCountOverflow++;
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
		 pt = (Mode << 4) | (night + 1); 
		 eeprom_write_word(&eepromWord, (uint8_t) pt);	
		 // Note: this function causes a momentary brightness glitch while it writes the EEPROM.
		 // We separate out this section to minimize the effect. 
		}
		
	}
	else{
	
		if  (CycleCountOverflow > TimeMax)
		{
			if (allowsleep){
			PORTA = 0;
			PORTB = 0;
			PORTD = 0;
			DDRD = 0;
			DDRB = 0;
			DDRA = 0;
			MCUCR &= 207;	// ensure power-down mode			
			MCUCR |= 48;	// enable sleep & power-down modes
			asm("sleep");		//Go to sleep!
			}
			else
			{
				CycleCountOverflow = 1;
			}
		}
			 
		if ((PINA & 2) == 0)		// Check if button currently pressed pressed down
	{ 
		 
		if (debounce2 > 150) 
		{   
			if (modeswitched == 0)
			{  
				debounce = 0;	// If we're holding, this isn't a momentary press. 
			
				modeswitched = 1;
				Mode++;
				UpdateConfig = 1;
			   
				if (Mode > 3) 
					Mode = 0;   
				
				if ((Mode & 1) == 0) // Modes 0, 2 
					brightmax = 16;   
				else	 
					brightmax = 64;   
				
			}
		}
		else {
			debounce = 1;		// Flag that the button WAS pressed.
			debounce2++;	
		} 
		
	}	
	else{	// If button is NOT pressed
	
		debounce2 = 0; 
		modeswitched = 0;
		
		if (debounce)
		{   debounce = 0;
			
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
				pt = 7 - night + j; 
				 
				if (pt >= LighterLoc)
					pt++; 
				
			if (LEDs[pt] < brightmax)
				LEDs[pt]++;
				
				if (LEDsFlicker[pt] < brightmax)
					LEDsFlicker[pt]++;				
				
				
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
		while (j < 9) {			// Fade down to low power mode
			
			if (LEDs[j] > brightmax)
				LEDs[j]--;  
			if (LEDsFlicker[j] > brightmax)
				LEDsFlicker[j]--;  
			
			
			
			j++;
			
		}  
		  
		
     if (Mode > 1) {		
			
	 // Flicker mode, for Mode 2 and 3.
		  
		 flickerPrescale++;
		 if ((flickerPrescale & 7) == 0) {		//Every N passes through...
			 
			 
			 if ((Mode & 1) == 0) // Modes 0, 2 
			 	 brightStep = 4;   
			 else	 
			 	 brightStep = 16;  
			 
			 
			 
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
		 
	
		 
		 
		 LED0 = LEDsFlicker[0];
		 LED1 = LEDsFlicker[1];
		 LED2 = LEDsFlicker[2];
		 LED3 = LEDsFlicker[3];
		 LED4 = LEDsFlicker[4];
		 LED5 = LEDsFlicker[5];
		 LED6 = LEDsFlicker[6];
		 LED7 = LEDsFlicker[7];
		 LED8 = LEDsFlicker[8];
		 
		 
		 
		 
		 
		 //brightmax
	 
	 }  
	 else{
	  
		 LED0 = LEDs[0];
		 LED1 = LEDs[1];
		 LED2 = LEDs[2];
		 LED3 = LEDs[3];
		 LED4 = LEDs[4];
		 LED5 = LEDs[5];
		 LED6 = LEDs[6];
		 LED7 = LEDs[7];
		 LED8 = LEDs[8];
	 }		   
		
		
		
		
				
				
	 
	}
	
	
	 // PWM routine.  Note that either 4 or 5 LEDs are (possibly) on at a given time. 
	 //  Brightness control: 64 levels of gray.
	 // Interleave passes through brightness to improve overall PWM rate.	 
	   
	  j = 0;
	  while (j < 16)
	  {
		  m = 0; 
			  while (m < 4) { 
			  
                  bt = j + m*16;	//bt: temporary brightness 
                  pt = 0;			//pt: port temp
                          
                  if (LED2 > bt) 
                  pt = 1; 
                  if (LED3 > bt) 
                  pt |= 2;	
                  if (LED4 > bt) 
                  pt |= 16; 
                  if (LED5 > bt) 
                  pt |= 32;		 
                  if (LED6 > bt) 
                  pt |= 128U;
                  
                  PORTD = 0;
                  PORTB = pt; 
                  shortdelay2();
                  shortdelay2();
                  shortdelay2(); 
                               
                  pt = 0;	
                  if (LED8 > bt) 
                  pt |= 1;	
                  if (LED7 > bt) 
                  pt |= 2;	
                  
                      if (LED1 > bt) 
                      pt |= 16;	
                      if (LED0 > bt) 
                      pt |= 32;			
              
                  PORTB = 0;  
                  PORTD = pt;     
                  m++;
			  
		  }
	   
	  j++; 
	  }
	// Delays to give even brightness on left & right halves.
	shortdelay();  	    
	shortdelay();  	    
	shortdelay();  	    
	asm("nop\n\t");
	
	PORTB = 0;
   
	}	//End main loop
	return 0;
}
