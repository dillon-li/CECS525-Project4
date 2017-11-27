//I2C Bus Driver Implementation for Raspberry PI
//original code by Mike McCauley modified by Eugene Rockey

#include <stdint.h>
#include <fcntl.h>
#include "mmio.h"
#include "uart.h"
#include "bcm2835.h"


// The delay time in uS needed to transmit one byte for 100KHZ SCL (see formula).
static int i2c_byte_wait_us = 90;

void bcm2835_i2c_begin(void)
{
	//RPI Model B Version 1	
    // Set the I2C/BSC0 pins to the Alt 0 function to enable I2C access on them
   // bcm2835_gpio_fsel(RPI_GPIO_P1_03, BCM2835_GPIO_FSEL_ALT0); // SDA
   // bcm2835_gpio_fsel(RPI_GPIO_P1_05, BCM2835_GPIO_FSEL_ALT0); // SCL
	
	//RPI MODEL B Version 2	
    // Set the I2C/BSC1 pins to the Alt 0 function to enable I2C access on them
    bcm2835_gpio_fsel(RPI_V2_GPIO_P1_03, BCM2835_GPIO_FSEL_ALT0); // SDA
    bcm2835_gpio_fsel(RPI_V2_GPIO_P1_05, BCM2835_GPIO_FSEL_ALT0); // SCL
}

void bcm2835_i2c_end(void)
{
	//RPI Model B Version 1
    // Set all the I2C/BSC0 pins back to input
   // bcm2835_gpio_fsel(RPI_GPIO_P1_03, BCM2835_GPIO_FSEL_INPT); // SDA
   // bcm2835_gpio_fsel(RPI_GPIO_P1_05, BCM2835_GPIO_FSEL_INPT); // SCL


	//RPI Model B Version 2
    // Set all the I2C/BSC1 pins back to input
    bcm2835_gpio_fsel(RPI_V2_GPIO_P1_03, BCM2835_GPIO_FSEL_INPT); // SDA
    bcm2835_gpio_fsel(RPI_V2_GPIO_P1_05, BCM2835_GPIO_FSEL_INPT); // SCL
}

void bcm2835_i2c_setSlaveAddress(uint8_t addr)
{
	// Set I2C Device Address
	volatile uint32_t* paddr = bcm2835_bsc1 + BCM2835_BSC_A/4;
	bcm2835_peri_write(paddr, addr);
}

void bcm2835_i2c_setClockDivider(uint16_t divider)
{
	//Set SCL frequency
    volatile uint32_t* paddr = bcm2835_bsc1 + BCM2835_BSC_DIV/4;
    bcm2835_peri_write(paddr, divider);
}

// Write a byte of data to I2C slave
void bcm2835_i2c_write(uint8_t sradd, char* buff)
{
    volatile uint32_t* dlen    = bcm2835_bsc1 + BCM2835_BSC_DLEN/4;
    volatile uint32_t* fifo    = bcm2835_bsc1 + BCM2835_BSC_FIFO/4;
    volatile uint32_t* status  = bcm2835_bsc1 + BCM2835_BSC_S/4;
    volatile uint32_t* control = bcm2835_bsc1 + BCM2835_BSC_C/4;
    // Clear FIFO
    bcm2835_peri_set_bits(control, BCM2835_BSC_C_CLEAR_1 , BCM2835_BSC_C_CLEAR_1 );
    // Clear Status
	bcm2835_peri_write_nb(status, BCM2835_BSC_S_CLKT | BCM2835_BSC_S_ERR | BCM2835_BSC_S_DONE);
	// Set Data Length
    bcm2835_peri_write_nb(dlen, 2);
    //Enable I2C, Start Write Operation
	bcm2835_peri_write_nb(control, BCM2835_BSC_C_I2CEN | BCM2835_BSC_C_ST);
	bcm2835_peri_write_nb(fifo, sradd);
	bcm2835_peri_write_nb(fifo, buff[0]);
    bcm2835_peri_set_bits(control, BCM2835_BSC_S_DONE , BCM2835_BSC_S_DONE);
}


// Read a byte from I2C slave sending a repeated start after writing
// the required register. Only works if your device supports this mode
void bcm2835_i2c_read(uint8_t sradd, char* buff)
{   
    volatile uint32_t* dlen    = bcm2835_bsc1 + BCM2835_BSC_DLEN/4;
    volatile uint32_t* fifo    = bcm2835_bsc1 + BCM2835_BSC_FIFO/4;
    volatile uint32_t* status  = bcm2835_bsc1 + BCM2835_BSC_S/4;
    volatile uint32_t* control = bcm2835_bsc1 + BCM2835_BSC_C/4;
    
    // Clear FIFO
    bcm2835_peri_set_bits(control, BCM2835_BSC_C_CLEAR_1 , BCM2835_BSC_C_CLEAR_1 );
    // Clear Status
	bcm2835_peri_write_nb(status, BCM2835_BSC_S_CLKT | BCM2835_BSC_S_ERR | BCM2835_BSC_S_DONE);
	// Set Data Length
    bcm2835_peri_write_nb(dlen, 1);
    // Enable device and start transfer
    bcm2835_peri_write_nb(control, BCM2835_BSC_C_I2CEN | BCM2835_BSC_C_ST);
    bcm2835_peri_write_nb(fifo, sradd);
	
    // poll for transfer has started
    while ( !( bcm2835_peri_read_nb(status) & BCM2835_BSC_S_TA ) )
    {
        // Linux may cause us to miss entire transfer stage
        if(bcm2835_peri_read(status) & BCM2835_BSC_S_DONE)
            break;
    }
    
	// Send a repeated start with read bit set in address
    bcm2835_peri_write_nb(dlen, 1);
    bcm2835_peri_write_nb(control, BCM2835_BSC_C_I2CEN | BCM2835_BSC_C_ST  | BCM2835_BSC_C_READ );
    
    // Wait for write to complete and first byte back.	
    bcm2835_delayMicroseconds(i2c_byte_wait_us * 3);
    // wait for transfer to complete
    while (!(bcm2835_peri_read_nb(status) & BCM2835_BSC_S_DONE))
    {
        // we must empty the FIFO as it is populated and not use any delay
        while (bcm2835_peri_read_nb(status) & BCM2835_BSC_S_RXD)
    	{
    		// Read from FIFO, no barrier
    		buff[0] = bcm2835_peri_read_nb(fifo);
    	}
    }
    bcm2835_peri_set_bits(control, BCM2835_BSC_S_DONE , BCM2835_BSC_S_DONE);
}