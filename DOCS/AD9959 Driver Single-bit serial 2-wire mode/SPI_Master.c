/***************************************************************************

 Author        : Neil Zhao - CAST

 Date          : Nov 26th, 2008

 File          : SPI_Master.c

 Hardware      : ADuC7026 and AD9959

 Description   : Use the GPIO to simulate the SPI communication of AD9959

		
***************************************************************************/

#include<ADuC7026.h>	
#include<Common.h>	

void delay (signed int length)
{
	while (length >0)
    	length--;
}

//---------------------------------
//WriteToAD9959ViaSpi();
//---------------------------------
//Function that writes to the AD9959 via the SPI port. It sends first the control
//word that includes the start address and then the data to write.
//--------------------------------------------------------------------------------
void WriteToAD9959ViaSpi(unsigned char RegisterAddress, unsigned char NumberofRegisters, unsigned char *RegisterData)
{
	unsigned	char	ControlValue = 0;
	unsigned	char	ValueToWrite = 0;
	signed		char	RegisterIndex = 0;
	unsigned	char	i = 0;

	//Create the 8-bit header
	ControlValue = RegisterAddress;

	CLR_SCL();
	delay(1);
	SET_CS();
	delay(1);
	CLR_CS();	 //bring CS low
	delay(1);

 	SDA_OUT();	//Make SDIO an output

	//Write out the control word
	for(i=0; i<8; i++)
	{
		CLR_SCL();
		if(0x80 == (ControlValue & 0x80))
		{
			SET_SDIO();	  //Send one to SDIO pin
		}
		else
		{
			CLR_SDIO();	  //Send zero to SDIO pin
		}
		delay(1);
		SET_SCL();
		delay(1);
		ControlValue <<= 1;	//Rotate data
	}
	CLR_SCL();
	delay(2);
	//And then the data
	for (RegisterIndex=NumberofRegisters; RegisterIndex>0; RegisterIndex--)
	{
		ValueToWrite = *(RegisterData + RegisterIndex - 1);
		for (i=0; i<8; i++)
		{
			CLR_SCL();
			if(0x80 == (ValueToWrite & 0x80))
			{
				SET_SDIO();	  //Send one to SDIO pin
			}
			else
			{
				CLR_SDIO();	  //Send zero to SDIO pin
			}
			delay(1);
			SET_SCL();
			delay(1);
			ValueToWrite <<= 1;	//Rotate data
		}
	}
	CLR_SCL();
	delay(1);
	SET_CS();	//bring CS high again
}

//---------------------------------
//ReadFromAD9959ViaSpi();
//---------------------------------
//Function that reads from the AD9959 via the SPI port. It first send the control word
//that includes the start address and then 32 clocks for each register to read.
//--------------------------------------------------------------------------------
void ReadFromAD9959ViaSpi(unsigned char RegisterAddress, unsigned char NumberofRegisters, unsigned char *RegisterData)
{
	unsigned	char	ControlValue = 0;
	signed		char	RegisterIndex = 0;
	unsigned	char	ReceiveData = 0;
	unsigned	char	i = 0;
	unsigned	int		iTemp = 0;

	//Create the 8-bit header
	ControlValue = RegisterAddress;

	SET_SCL();
	delay(1);	
	SET_CS();
	delay(1);
	CLR_CS();	 //bring CS low
	delay(1);

 	SDA_OUT();	//Make SDIO an output

	//Write out the control word
	for(i=0; i<8; i++)
	{
		CLR_SCL();
		if(0x80 == (ControlValue & 0x80))
		{
			SET_SDIO();	  //Send one to SDIO pin
		}
		else
		{
			CLR_SDIO();	  //Send zero to SDIO pin
		}
		delay(1);
		SET_SCL();
		delay(1);
		ControlValue <<= 1;	//Rotate data
	}

	SDA_IN();	//Make SDA an input

	//Read data in
	for (RegisterIndex=NumberofRegisters; RegisterIndex>0; RegisterIndex--)
	{
		for(i=0; i<8; i++)
		{
			CLR_SCL();
			ReceiveData <<= 1;		//Rotate data
			iTemp = GP1DAT;			//Read SDIO of AD9959
			if(0x00000008 == (iTemp & 0x00000008))
			{
				ReceiveData |= 1;	
			}
			delay(1);
			SET_SCL();
			delay(1);
		}
		*(RegisterData + RegisterIndex - 1) = ReceiveData;
	}
	delay(1);
	SET_CS();	//bring CS high again
} 
