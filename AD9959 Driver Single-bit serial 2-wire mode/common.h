#define SET_CS()		GP4DAT = (GP4DAT | 0x00020000)	//P4.1->/CS
#define CLR_CS()		GP4DAT = (GP4DAT & 0xFFFDFFFF)

#define	SET_SCL()		GP1DAT = (GP1DAT | 0x00040000)	//P1.2->SCLK
#define	CLR_SCL()		GP1DAT = (GP1DAT & 0xfffbffff)

#define SET_SDIO()		GP1DAT = (GP1DAT | 0x00080000)	//P1.3->SDIO_0
#define CLR_SDIO()		GP1DAT = (GP1DAT & 0xfff7ffff)

#define SDA_OUT()		GP1DAT = (GP1DAT | 0x08000000)
#define SDA_IN()		GP1DAT = (GP1DAT & 0xf7ffffff)

void ADuC7026_Initiate(void);
void main(void);
void delay (int length);
void IO_Update(void);

void WriteToAD9959ViaSpi(unsigned char RegisterAddress, unsigned char NumberofRegisters, unsigned char *RegisterData);
void ReadFromAD9959ViaSpi(unsigned char RegisterAddress, unsigned char NumberofRegisters, unsigned char *RegisterData);

int putchar(unsigned long ch);