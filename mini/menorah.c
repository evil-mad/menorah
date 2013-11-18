/*
menorah.c

Written by Windell Oskay, http://www.evilmadscientist.com/

This is free software; please modify it and use it to do cool stuff.



This avr-gcc program for the Atmel ATTiny2313 
runs a very tiny hanukkah menorah that lites from 2 - 9 LED "candles."

 http://www.evilmadscientist.com/2006/how-to-make-high-tech-led-decorations-for-the-holidays/
 
The number of candles displayed is stored in EEPROM and is
incremented each time that the unit is reset.

After finishing the "standard" Hanukkah sequence, it then enters a demo mode,
to show off its program.

Version 2.0   Last Modified:  12/1/2008. 
Now extra spiffy, with better LED uniformity, higher refresh rate, and lower power usage. :D
 
 
 
 
*/

 


#include <avr/io.h>
#include <avr/eeprom.h> 

uint16_t eepromWord __attribute__((section(".eeprom")));

int main (void)
{
uint8_t m,n;		//8-bit unsigned integers
uint8_t PB, PD;
uint8_t PortBData[9] = {8U,9U,11U,15U,31U,63U,127U,255U,0};
uint8_t PortDData[9] = {64U,64U,64U,64U,64U,64U,64U,64U,0};

//uint8_t multiplexed;

unsigned int day, daycopy,  j;				//16-bit unsigned integers
unsigned short i;				

//Initialization routine: Clear watchdog timer-- this can prevent several things from going wrong.		
MCUSR &= 0xF7;		//Clear WDRF Flag
WDTCSR	= 0x18;		//Set stupid bits so we can clear timer...
WDTCSR	= 0x00;

//Data direction register: DDRD
//Set all ports to output *EXCEPT* PA2 (reset pin)
//Only IO lines D6 and B0-B7 will be used for output.
	DDRA = 3U;
	DDRB = 255U;	
	DDRD = 127U;


// Read string number from EEPROM data memory. If good, increment it and save
// the new value to EEPROM.  This allows us to cycle through the set of strings in flash, 
// using an new one each time that the unit is reset.  

if ( eeprom_read_word(&eepromWord) == 0xffff)
	day = 0U;
else	
	day = (uint8_t) (eeprom_read_word(&eepromWord)) ;
	
	
			
day++;
	
if (day > 8)	
	day = 0U;

eeprom_write_word(&eepromWord, (uint8_t) day);

daycopy = day;
	
/* Visualize outputs:

		B7 B6 B5 B4 B3 B2 B1 B0 D6	 B value
Day 0	0  0  0  0  1  0  0  0  1		8
Day 1	0  0  0  0  1  0  0  1  1		9
Day 2	0  0  0  0  1  0  1  1  1		11
Day 3   0  0  0  0  1  1  1  1  1		15
Day 4	0  0  0  1  1  1  1  1  1		31
Day 5   0  0  1  1  1  1  1  1  1		63
Day 6   0  1  1  1  1  1  1  1  1		127
Day 7   1  1  1  1  1  1  1  1  1		255

Day 8: Demo mode, scan day = 0 to 7.


Pin D6 is always high, other D pins are always low. 2^6 = 64, so D = 64U

PORTB: B3 is always high.
Summarize B values in data table;
uint8_t PortBData[9] = {8U,9U,11U,15U,31U,63U,127U,255U,0};

We add a final case-- all lights off-- as well.

*/
	
//multiplexed = LowPower;	
 
	
	PORTA = 0;		
	PORTB = 0;	
	PORTD = 0;



   i = 0;

for (;;)  // main loop
{



if (daycopy == 8)
{
	day++;
	if (day > 8)
		day = 0;
}



PB = PortBData[day];
PD = PortDData[day];

  
 j = 0;		
 while (j < 15000)		// This sets how long it is between changing number of "candles" when (daycopy == 8)	
	{ 

 
#define shortdelay(); 			asm("nop\n\t" \ 
								"nop\n\t");

//Multiplexing routine: Each LED is on (1/9) of the time. 
//  -> Uses much less power.
 
m = 6;
PORTD = PD & (64U);
shortdelay();
PORTD = 0;

PORTB = PB & (1);
shortdelay();
PORTB = 0;

PORTB = PB & (2);
shortdelay();
PORTB = 0;

PORTB = PB & (4);
shortdelay();
PORTB = 0;

PORTB = PB & (8);
shortdelay();
PORTB = 0;

PORTB = PB & (16);
shortdelay();
PORTB = 0;

PORTB = PB & (32);
shortdelay();
PORTB = 0;

PORTB = PB & (64);
shortdelay();
PORTB = 0;

PORTB = PB & (128);
shortdelay();
PORTB = 0;
 							
	 
	
	j++;				
	} 
  
	}	//End main loop
	return 0;
}
