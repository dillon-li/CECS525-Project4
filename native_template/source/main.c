// main.c - main for the CECS 525 Raspberry PI kernel.img
// by Eugene Rockey Copyright 2013 All Rights Reserved
// debug everything that needs debugging
// Add, remove, modify, preserve in order to fulfill project 4 requirements.

#include <stdint.h>
#include <stddef.h>
#include "uart.h"
#include "mmio.h"
#include "bcm2835.h"
//#include "math.h"

#define SECS 0x00
#define MINS 0x01
#define HRS	 0x02
#define DOM	 0x04
#define MONTH 0x05
#define YEAR 0x06
#define ASECS 0x07
#define TEMP 0x11
#define CR 0x0D


const char MS1[] = "\r\n\nCECS-525 RPI Berry OS";
const char MS2[] = "\r\nby Eugene Rockey Copyright 2013 All Rights Reserved";
const char MS3[] = "\r\nReady: ";
const char MS4[] = "\r\nInvalid Command Try Again...";

//PWM Data for Alarm Tone
uint32_t N[200] = {0,1,2,3,4,5,6,7,8,9,10,11,12,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,
				36,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,60,61,62,63,64,65,66,67,68,69,
				70,71,72,73,74,75,76,77,78,79,80,81,82,83,84,84,85,86,87,88,89,90,91,92,93,94,95,96,95,94,93,92,91,90,
				89,88,87,86,85,84,84,83,82,81,80,79,78,77,76,75,74,73,72,71,70,69,68,67,66,65,64,63,62,61,60,60,59,58,57,
				56,55,54,53,52,51,50,49,48,47,46,45,44,43,42,41,40,39,38,37,36,36,35,34,33,32,31,30,29,28,27,26,25,24,23,
				   22,21,20,19,18,17,16,15,14,13,12,12,11,10,9,8,7,6,5,4,3,2,1};
char* buffer[1];
char* ISCN[1];
char alarm[1];
uint8_t ones;
uint8_t tens;
char* tbuf;
char* rbuf;
char cbuf = '\0';
void kernel_main();             //prototypes
void enable_arm_irq();
void disable_arm_irq();
void enable_arm_fiq();
void disable_arm_fiq();
char read_char_buffer();
void reboot();
void enable_irq_57();
void disable_irq_57();
void enable_irq_52();
void enable_1hz();
void falling_edge(uint8_t);
extern int invar;               //assembly variables
extern int outvar;
void displaycommandline();
void ALARM();



//Pointers to some of the BCM2835 peripheral register bases
volatile uint32_t* bcm2835_gpio = (uint32_t*)BCM2835_GPIO_BASE;
volatile uint32_t* bcm2835_clk = (uint32_t*)BCM2835_CLOCK_BASE;
volatile uint32_t* bcm2835_pads = (uint32_t*)BCM2835_GPIO_PADS;//for later updates to program
volatile uint32_t* bcm2835_spi0 = (uint32_t*)BCM2835_SPI0_BASE;
volatile uint32_t* bcm2835_bsc0 = (uint32_t*)BCM2835_BSC0_BASE;//for later updates to program
volatile uint32_t* bcm2835_bsc1 = (uint32_t*)BCM2835_BSC1_BASE;
volatile uint32_t* bcm2835_st = (uint32_t*)BCM2835_ST_BASE;


void falling_edge(uint8_t pin)
{
    volatile uint32_t* paddr = bcm2835_gpio + BCM2835_GPFEN0/4 + pin/32;
    uint8_t shift = pin % 32;
    uint32_t value = 1 << shift;
    bcm2835_peri_set_bits(paddr, value, value);
}

void enable_irq_57(void)
{
	mmio_write(0x2000B214, 0x02000000);
}

void disable_irq_57(void)
{
	mmio_write(0x2000B220, 0x02000000);
}

void enable_irq_52(void)
{
	mmio_write(0x2000B214,0x00020000);
	bcm2835_gpio_fsel(23,BCM2835_GPIO_FSEL_INPT);
	bcm2835_gpio_fsel(24,BCM2835_GPIO_FSEL_INPT);
	bcm2835_gpio_set_pud(23,2);
	bcm2835_gpio_set_pud(24,2);
	falling_edge(23);
	falling_edge(24);
}

void enable_1hz(void)
{
	//uart_puts("\r\nbegin enable_1hz\r\n");
	bcm2835_i2c_begin();
	bcm2835_i2c_setClockDivider(BCM2835_I2C_CLOCK_DIVIDER_2500);
	bcm2835_i2c_setSlaveAddress(0x68);
	*ISCN[0] = 0x00;
	// I2C address 0x68 is the DS3231
	//mmio_write(0x68, 0x00000000);
	mmio_write(0x0E, 0x00000000);
	//Psuedo Code Here: clear the INTCN (TY ROCKEY) bit in the DS3231M Control register
	bcm2835_delayMicroseconds(1000);
	bcm2835_i2c_end();
}

char read_char_buffer(void)
{
	uart_puts("");
	volatile uint32_t* control = bcm2835_bsc1 + BCM2835_BSC_C/4;
	uint8_t c = '\0';
	/*if(cbuf != '\0'){
	    uart_puts("alarm");
	    //call alarm
	    ALARM();
	}*/
	while (c == '\0') {c = cbuf;}
	uart_puts("");
	//Psuedo Code Here: make cbuf = null
	cbuf = '\0'; //prolly not gonna work but who knows //it didn't work
	bcm2835_peri_set_bits(control, BCM2835_BSC_C_CLEAR_1 , BCM2835_BSC_C_CLEAR_1 );
	return c;
}

void banner(void)
{
	uart_puts(MS1);
	uart_puts(MS2);
}

uint8_t BCDtoUint8(uint8_t BCD)
{
	return (BCD & 0x0F) + ((BCD >> 4) * 10);
}

void DATE(void)
{
	uart_puts("\r\nEnter DATE (S)et or (D)isplay\r\n");
	uint8_t c = '\0';
	while (c == '\0') 
	{
		c = read_char_buffer();
	}
	switch (c) {
		case 'S' | 's':
			bcm2835_i2c_begin();
			bcm2835_i2c_setClockDivider(BCM2835_I2C_CLOCK_DIVIDER_2500);
			bcm2835_i2c_setSlaveAddress(0x68);
			uart_puts("\r\nSet Date\r\n");
			uart_puts("\r\nType Day of Month (two digits 01-31): \r\n");
			tens = ((read_char_buffer()-48) << 4) | 0x0F;
			uart_putc((tens >> 4)+48);
			ones = (read_char_buffer()-48) | 0xF0;
			uart_putc((ones & 0x0F)+48);
			if (BCDtoUint8(tens & ones) < 1 || BCDtoUint8(tens & ones) > 31)
			{
				*buffer[0] = 0x01;
				uart_puts("\r\nInvalid Day of the Month Value!\r\n");
				break;
			}
			else
			{
				*buffer[0] = tens & ones;
			}			
			bcm2835_i2c_write(DOM,*buffer);
			uart_puts("\r\nType Month (two digits 01-12): \r\n");
			tens = ((read_char_buffer()-48) << 4) | 0x0F;
			uart_putc((tens >> 4)+48);
			ones = (read_char_buffer()-48) | 0xF0;
			uart_putc((ones & 0x0F)+48);
			if (BCDtoUint8(tens & ones) < 1 || BCDtoUint8(tens & ones) > 12) 
			{
				*buffer[0] = 0x01;
				uart_puts("\r\nInvalid Month Value!\r\n");
				break;
			}
			else
			{
				*buffer[0] = tens & ones;
			}			
			bcm2835_i2c_write(MONTH,*buffer);			
			uart_puts("\r\nType Year (two digits 00-99): \r\n");
			tens = ((read_char_buffer()-48) << 4) | 0x0F;
			uart_putc((tens >> 4)+48);
			ones = (read_char_buffer()-48) | 0xF0;
			uart_putc((ones & 0x0F)+48);
			if (BCDtoUint8(tens & ones) < 0 || BCDtoUint8(tens & ones) > 99) 
			{
				*buffer[0] = 0x00;
				uart_puts("\r\nInvalid Year Value!\r\n");
				break;
			}
			else
			{
				*buffer[0] = tens & ones;
			}			
			bcm2835_i2c_write(YEAR,*buffer);			
			uart_puts("\r\nDate is now set.\r\n");
			bcm2835_i2c_end();
            break;
		case 'D' | 'd':
			bcm2835_i2c_begin();
			bcm2835_i2c_setClockDivider(BCM2835_I2C_CLOCK_DIVIDER_2500);
			//Psuedo Code Here: set I2C slave address to 0x68 (?)
			bcm2835_i2c_setSlaveAddress(0x68);
			bcm2835_i2c_read(DOM,*buffer);
			char ones = *buffer[0] & 0x0F;
			char tens = (*buffer[0] >> 4);
			uart_putc(tens+48);
			uart_putc(ones+48);
			bcm2835_i2c_read(MONTH,*buffer);
			ones = *buffer[0] & 0x0F;
			tens = (*buffer[0] >> 4);
			uart_putc('/');
			uart_putc(tens+48);
			uart_putc(ones+48);
			bcm2835_i2c_read(YEAR,*buffer);
			ones = *buffer[0] & 0x0F;
			tens = (*buffer[0] >> 4);
			uart_putc('/');
			uart_putc(tens+48);
			uart_putc(ones+48);	
			bcm2835_i2c_end();
			break;
		default:
			uart_puts(MS4);
			DATE();
			break;
	}
}


void tx(void) {
    	uart_puts("\r\nIn tx()\r\n");
}

void TIME(void)
{
	uart_puts("\r\nType TIME (S)et or (D)isplay\r\n");
	uint8_t c = '\0';
	while (c == '\0') 
	{
		c = read_char_buffer();
	}
	switch (c) {
		case 'S' | 's':
			bcm2835_i2c_begin();
			bcm2835_i2c_setClockDivider(BCM2835_I2C_CLOCK_DIVIDER_2500);
			bcm2835_i2c_setSlaveAddress(0x68);
			uart_puts("\r\nSet 24 Hour Time\r\n");
			uart_puts("\r\nType Hours (two digits 00-23): \r\n");
			tens = ((read_char_buffer()-48) << 4) | 0x0F;
			uart_putc((tens >> 4)+48);
			ones = (read_char_buffer()-48) | 0xF0;
			uart_putc((ones & 0x0F)+48);
			if (BCDtoUint8(tens & ones) < 0 || BCDtoUint8(tens & ones) > 23) 
			{
				*buffer[0] = 0x00;
				uart_puts("\r\nInvalid Hours Value!\r\n");
				break;
			}
			else
			{
				*buffer[0] = tens & ones;
			}			
			bcm2835_i2c_write(HRS,*buffer);
			uart_puts("\r\nType Minutes (two digits 00-59): \r\n");
			tens = ((read_char_buffer()-48) << 4) | 0x0F;
			uart_putc((tens >> 4)+48);
			ones = (read_char_buffer()-48) | 0xF0;
			uart_putc((ones & 0x0F)+48);
			if (BCDtoUint8(tens & ones) < 0 || BCDtoUint8(tens & ones) > 59) 
			{
				*buffer[0] = 0x00;
				uart_puts("\r\nInvalid Minutes Value!\r\n");
				break;
			}
			else
			{
				*buffer[0] = tens & ones;
			}			
			bcm2835_i2c_write(MINS,*buffer);			
			uart_puts("\r\nType Seconds (two digits 00-59): \r\n");
			tens = ((read_char_buffer()-48) << 4) | 0x0F;
			uart_putc((tens >> 4)+48);
			ones = (read_char_buffer()-48) | 0xF0;
			uart_putc((ones & 0x0F)+48);
			if (BCDtoUint8(tens & ones) < 0 || BCDtoUint8(tens & ones) > 59) 
			{
				*buffer[0] = 0x00;
				uart_puts("\r\nInvalid Seconds Value\r\n!");
				break;
			}
			else
			{
				*buffer[0] = tens & ones;
			}			
			bcm2835_i2c_write(SECS,*buffer);			
			uart_puts("\r\nTime is now set.\r\n");
			bcm2835_i2c_end();
			break;
            case 'D' | 'd':
			bcm2835_i2c_begin();
			bcm2835_i2c_setClockDivider(BCM2835_I2C_CLOCK_DIVIDER_2500);
			bcm2835_i2c_setSlaveAddress(0x68);
			bcm2835_i2c_read(HRS,*buffer);
			ones = *buffer[0] & 0x0F;
			tens = (*buffer[0] >> 4);
			uart_putc(tens+48);
			uart_putc(ones+48);
			bcm2835_i2c_read(MINS,*buffer);
			ones = *buffer[0] & 0x0F;
			tens = (*buffer[0] >> 4);
			uart_putc(':');
			uart_putc(tens+48);
			uart_putc(ones+48);
			bcm2835_i2c_read(SECS,*buffer);
			ones = *buffer[0] & 0x0F;
			tens = (*buffer[0] >> 4);
			uart_putc(':');
			uart_putc(tens+48);
			uart_putc(ones+48);	
			//Psuedo Code Here: End the i2c communication
			break;	
		default:
			uart_puts(MS4);
			TIME();
			break;	
	}
}

void ALARM(void)
{
	uart_puts("\r\nType ALARM (S)et or (D)isplay or (T)est\r\n");
	uint8_t c = '\0';
	while (c == '\0') 
	{
		c = read_char_buffer();
	}
	switch (c) {
		case 'S' | 's':
			uart_puts("\r\nSet Seconds Alarm\r\n");
			uart_puts("\r\nType Starting Alarm Seconds (two digits 05-59): \r\n");
			tens = ((read_char_buffer()-48) << 4) | 0x0F;
			uart_putc((tens >> 4)+48);
			ones = (read_char_buffer()-48) | 0xF0;
			uart_putc((ones & 0x0F)+48);
			if (BCDtoUint8(tens & ones) < 5 || BCDtoUint8(tens & ones) > 59) 
			{
				alarm[0] = 0x05;
				uart_puts("\r\nInvalid Alarm Value, Value Reset to 5!\r\n");
				break;
			}
			else
			{
				alarm[0] = tens & ones;
				uart_puts("\r\nAlarm is now set.\r\n");
			}
			break;
		case 'D' | 'd':
			ones = alarm[0] & 0x0F;
			tens = alarm[0] >> 4;
			uart_putc(tens+48);
			uart_putc(ones+48);
			break;
		case 'T' | 't':
			if (BCDtoUint8(alarm[0]) < 5 || BCDtoUint8(alarm[0]) > 59)
			{
				uart_puts("\r\nAlarm Value Out of Range, Set to a Proper Value First!\r\n");
				break;
			}
			uart_puts("\r\nPlease wait, now testing Alarm...\r\n");
            		bcm2835_i2c_begin();
			bcm2835_i2c_setClockDivider(BCM2835_I2C_CLOCK_DIVIDER_2500);
			bcm2835_i2c_setSlaveAddress(0x68);
			uint32_t restore = mmio_read(0x20200010);
			//Psuedo Code Here: Configure GPIO40 and 45 for PWM to the 3.5mm Audio Jack
			mmio_write(0x20200010, 0x0);
			bcm2835_gpio_fsel(40, BCM2835_GPIO_FSEL_ALT0);
			bcm2835_gpio_fsel(45, BCM2835_GPIO_FSEL_ALT0);
			mmio_write(BCM2835_GPIO_PWM + BCM2835_PWM_CONTROL, 0x0);//Disable PWM
			mmio_write(BCM2835_CLOCK_BASE + BCM2835_PWMCLK_CNTL, 0x5A000020); //Stop the PWM clock
			bcm2835_delayMicroseconds(110);//Long Wait for PWMCLK to settle
			while ((mmio_read(BCM2835_CLOCK_BASE + BCM2835_PWMCLK_CNTL) & 0x80 != 0)) bcm2835_delayMicroseconds(1);//Wait until clock does stop
			mmio_write(BCM2835_CLOCK_BASE + BCM2835_PWMCLK_DIV, 0x5A001000);//Set the PWM clock divder to 1.
			mmio_write(BCM2835_CLOCK_BASE + BCM2835_PWMCLK_CNTL, 0x5A000011);//Enable the PWM clock, make it the 19.2MHZ osc / divider.
			mmio_write(BCM2835_GPIO_PWM + BCM2835_PWM0_RANGE, 96);//Range0 for left channel is the Cycle Time or Frequency = 1/cycle time
			mmio_write(BCM2835_GPIO_PWM + BCM2835_PWM1_RANGE, 96);//Range0 for Right channel is the Cycle Time or Frequency = 1/cycle time	
			mmio_write(BCM2835_GPIO_PWM + BCM2835_PWM0_DATA, 0);//Data0 is the Duty Cycle for the left channel
			mmio_write(BCM2835_GPIO_PWM + BCM2835_PWM1_DATA, 0);//Data1 is the Duty Cycle for the right channel
			mmio_write(BCM2835_GPIO_PWM + BCM2835_PWM_CONTROL, BCM2835_PWM0_ENABLE);//turn on left channel
			mmio_write(BCM2835_GPIO_PWM + BCM2835_PWM_CONTROL, BCM2835_PWM1_ENABLE);//turn on right channel
			bcm2835_i2c_read(SECS,*buffer);
			uint8_t initialsecond = BCDtoUint8(*buffer[0]);
			uint8_t	finalsecond = initialsecond + BCDtoUint8(alarm[0]);
			if (finalsecond > 59) finalsecond = finalsecond - 60;
			while (BCDtoUint8(*buffer[0]) < finalsecond) 
			{
				bcm2835_i2c_read(SECS,*buffer);
			}
			bcm2835_i2c_read(SECS,*buffer);
			initialsecond = BCDtoUint8(*buffer[0]);
			finalsecond = initialsecond + 3;
			if (finalsecond > 59) finalsecond = finalsecond - 60;
			while (BCDtoUint8(*buffer[0]) < finalsecond) 
			{
				for (uint32_t count = 0; count < 200 ; count++) 
				{
					mmio_write(BCM2835_GPIO_PWM + BCM2835_PWM0_DATA, N[count]);
					mmio_write(BCM2835_GPIO_PWM + BCM2835_PWM1_DATA, N[count]);
					bcm2835_delayMicroseconds(5);
				}
				bcm2835_i2c_read(SECS,*buffer);
			}			
			bcm2835_i2c_end();
			mmio_write(BCM2835_GPIO_PWM + BCM2835_PWM0_DATA, 0);//No modulation
			mmio_write(BCM2835_GPIO_PWM + BCM2835_PWM1_DATA, 0);//No modulation
			mmio_write(BCM2835_GPIO_PWM + BCM2835_PWM_CONTROL, 0x0);//Disable PWM
			mmio_write(0x20200010, restore);//restore GPIO40 and 45
			break;
		default:
			uart_puts(MS4);
			ALARM();
			break;	
	}
}

void RES(void)
{
	//Psuedo Code Here: reboot the system
	// D: based off linked vid in chat
	//uart_init();
	//kernel_main(); // D: already initializes stuff in main. TA being cheeky said check boot.s. Stack and stuff initialized there so might need that
	// D: we need to re set stack which is in boot.s. The reset label in boot.s calls kernel main so i think we need to call that boot.s label here!!!!
	reboot(); // D: I think this is right this should reset stack pointers. calls reboot in boot.s -> reset in boot.s -> calls main_kernel
}

void HELP(void) //Command List
{
	uart_puts("\r\n(D)ate,(H)elp,a(L)arm,(R)eset,(T)ime,(V)FP11,T(x)");
}


void displaycommandline(void)
{ 	
	//date
	bcm2835_i2c_begin();
	bcm2835_i2c_setClockDivider(BCM2835_I2C_CLOCK_DIVIDER_2500);
	//Psuedo Code Here: set I2C slave address to 0x68 (?)
	bcm2835_i2c_setSlaveAddress(0x68);
	bcm2835_i2c_read(DOM,*buffer);
	char ones = *buffer[0] & 0x0F;
	char tens = (*buffer[0] >> 4);
	uart_putc(tens+48);
	uart_putc(ones+48);
	bcm2835_i2c_read(MONTH,*buffer);
	ones = *buffer[0] & 0x0F;
	tens = (*buffer[0] >> 4);
	uart_putc('/');
	uart_putc(tens+48);
	uart_putc(ones+48);
	bcm2835_i2c_read(YEAR,*buffer);
	ones = *buffer[0] & 0x0F;
	tens = (*buffer[0] >> 4);
	uart_putc('/');
	uart_putc(tens+48);
	uart_putc(ones+48);	
	bcm2835_i2c_end();
	uart_puts(":");
	//time
	bcm2835_i2c_begin();
	bcm2835_i2c_setClockDivider(BCM2835_I2C_CLOCK_DIVIDER_2500);
	bcm2835_i2c_setSlaveAddress(0x68);
	bcm2835_i2c_read(HRS,*buffer);
	ones = *buffer[0] & 0x0F;
	tens = (*buffer[0] >> 4);
	uart_putc(tens+48);
	uart_putc(ones+48);
	bcm2835_i2c_read(MINS,*buffer);
	ones = *buffer[0] & 0x0F;
	tens = (*buffer[0] >> 4);
	uart_putc('-');
	uart_putc(tens+48);
	uart_putc(ones+48);
	bcm2835_i2c_read(SECS,*buffer);
	ones = *buffer[0] & 0x0F;
	tens = (*buffer[0] >> 4);
	uart_putc('-');
	uart_putc(tens+48);
	uart_putc(ones+48);	
	//Psuedo Code Here: End the i2c communication

	//temp
	uart_puts(":");
	bcm2835_i2c_begin();
    	bcm2835_i2c_setClockDivider(BCM2835_I2C_CLOCK_DIVIDER_2500);
	bcm2835_i2c_setSlaveAddress(0x68);
	bcm2835_i2c_read(TEMP, *buffer);
	char some = (*buffer[0] >> 2);
	char something = (*buffer[0] >> 4);
	//uart_puts("\r\n");
	uart_putc(some+48);
	uart_putc(something+48);
	uart_puts("\r\n");
	bcm2835_i2c_end();
	//Engineer the code here to display the active MM/DD/YY:HOURS-MINUTES-SECONDS:TEMPERATURE> command line prompt.
}

void VFP11(void) //ARM Vector Floating Point Unit Demo
{
    uart_puts("\r\nCalculator Menu\r\n");
    uart_puts("\r\n1. Addition\r\n");
    uart_puts("\r\n2. Subtraction\r\n");
    uart_puts("\r\n3. Multiplication\r\n");
    uart_puts("\r\n4. Division\r\n");
    uart_puts("\r\n9. Quit\r\n");
    //Engineer the VFP11 math coprocessor application here.
    //Send a menu to hyperterminal as was done with the minimal computer
    //FADD, FSUB, FMUL, and FDIV, and any other functions you wish to implement in ARM assembly routines in the boot.s file.
	
}

void command(void)
{
	uart_puts("\r\n");
	uart_puts(MS3);
	uint8_t c = '\0';
	//while (c == '\0') {
	c = read_char_buffer();
	//}
	uart_putc(c);
	switch (c) {
		case 'D' | 'd':
			DATE();   //DATE command is demo’d
			break;
		case 'T' | 't':
			TIME();   //Time command is demo’d
			break;
		case 'L' | 'l':
			ALARM();  //ALARM command is demo’d
			break;   
		case 'R' | 'r':
			RES();	   //RES command is demo’d
			break;
		case 'H' | 'h':
			HELP();	   //HELP command is demo’d
			break;
		case 'V' | 'v':
			VFP11();    //Vector Floating Point Calculator is demo’d
			break;
		case 'X' | 'x':
		        uart_puts("T(x)");
			tx();
			break;
		//Psuedo Code Here: Include the T(x) command for transmitting a large string to Teraterm using Tx interrupt system

		default:
			if (c != 'q') uart_puts(MS4);
			HELP();
			break;
	}
}

int logon(void)
{
	//Engineer your 3 attempt LOGON code here
}

void kernel_main() 
{
	uart_init();
	enable_irq_57();
	enable_irq_52();
	enable_1hz();
	enable_arm_irq();
//	if (logon() == 0) while (1) {}
	banner();
	HELP();
	//displaycommandline(); //works
	while (1) {
	    command();
	}
}

void irq_handler(void)
{
	//uart_puts("\r\nIRQ HANDLER\r\n");
	//re-engineer this irq_handler to handle the four interrupts as described in the project 4 document (reset button, 1HZ command line, Rx, Tx)

	uint32_t pending = mmio_read(0x20200040); //read all gpio interrupt flags
	mmio_write(0x20200040,0xFFFFFFFF);  //clear all gpio interrupt flags

	if ((pending & 0x800000) == 0x800000)  //did the reset button cause the interrupt
	{
	   reboot();	//if so then warm boot
	}
	else if ((pending & 0x1000000) == 0x1000000)
	{
	   displaycommandline();  // if so then update commandline prompt
	}
	else
	{
	   uart_puts("");
	   cbuf = uart_readc(); // if not those then must have been the keyboard
	   uart_puts("");
	}
}


