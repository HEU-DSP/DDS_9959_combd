#include <ADuC7026.h>
#include <Common.h>

void ADuC7026_Initiate(void)
{
    //Clock Initial
	POWKEY1 = 0x01;				//Start PLL Setting
    POWCON = 0x00;				//Set PLL Active Mode With CD = 0  CPU CLOCK DIVIDER = 41.78MHz
    POWKEY2 = 0xF4;				//Finish PLL Setting

	GP1CON = 0x00000011;         //PIN set up for UART and make I2C pin as GPIO

	GP2DAT = GP2DAT | 0x04040000; //Disable LCD;
	GP0DAT = GP0DAT | 0x02020000; //Disable LED;

	GP1DAT = GP1DAT | 0x04000000; //Configure the P1.2 pin as output for CLK of AD9959

	GP4DAT = GP4DAT | 0x22020000; //Configure the P4.1 pin as output for CS of AD9959
								  //Configure the P4.5 pin as output for IO_UPDATE of AD9959

   	//UART Initial£¬Baud Rate = 9600
	COMCON0 = 0x080;  
	COMDIV0 = 0x088;    		
	COMDIV1 = 0x000;
	COMCON0 = 0x007;    		
}
void IO_Update(void)
{
	GP4DAT = GP4DAT & 0xFFDFFFFF;
	delay(2);
	GP4DAT = GP4DAT | 0x00200000;
	delay(8);
	GP4DAT = GP4DAT & 0xFFDFFFFF;
}
