/*-----------------------ADI AD9959 Driver Reference Design Source Code-----------------------------------

Author: 		ADI CAST (China Application Support Team)
Date:			2008-11-26
Rev:			1.0
Description:	Realize AD9959 Driver£¬Use ADuC7026 as MCU£¬Development Tool: KEIL C

---------------------------------------------------------------------------------------------------------*/
#include <ADuC7026.h>
#include <Common.h>

unsigned char RegisterData[4] = {0,0,0,0};

int putchar(unsigned long ch)  {                   /* Write character to Serial Port  */

    while(!(0x020==(COMSTA0 & 0x020)))
    {}
 
 	return (COMTX = ch);	//COMTX is an 8-bit transmit register.
}

void main(void)
{
	ADuC7026_Initiate();		//ADuC7026 Initialization

	RegisterData[0] = 0x10;		//Channel 0 enable
	WriteToAD9959ViaSpi(0x00,1,RegisterData);

	RegisterData[2] = 0xAB;		//REF CLK = 50MHz, Multiplication factor = 10
	RegisterData[1] = 0x00;
	RegisterData[0] = 0x00;
	WriteToAD9959ViaSpi(0x01,3,RegisterData);//VCO gain control = 1, system clock = 500MHz

	RegisterData[3] = 0x05;
	RegisterData[2] = 0x1E;
	RegisterData[1] = 0xB8;
	RegisterData[0] = 0x51;
	WriteToAD9959ViaSpi(0x04,4,RegisterData);//Output frequency = 10MHz 

	IO_Update();

	ReadFromAD9959ViaSpi(0x80,1,RegisterData);
	putchar(RegisterData[0]);
	ReadFromAD9959ViaSpi(0x81,3,RegisterData);
	putchar(RegisterData[2]);
	putchar(RegisterData[1]);
	putchar(RegisterData[0]);
	while(1)
	{;}
}
