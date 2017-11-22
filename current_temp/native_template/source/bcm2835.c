//BCM2835 Peripharal Implementations for Raspberry PI
//Universal functions required for register access and peripheral operations/configurations. 
//original code by Mike McCauley modified by Eugene Rockey


#include <stdint.h>
#include <fcntl.h>
#include "mmio.h"
#include "uart.h"
#include "bcm2835.h"


// Read the System Timer Counter (64-bits)
uint64_t bcm2835_st_read(void)
{
    volatile uint32_t* paddr;
    uint64_t st;
    paddr = bcm2835_st + BCM2835_ST_CHI/4;
    st = bcm2835_peri_read(paddr);
    st <<= 32;
    paddr = bcm2835_st + BCM2835_ST_CLO/4;
    st += bcm2835_peri_read(paddr);
    return st;
}

// Writes (and reads) an number of bytes to SPI
void bcm2835_spi_transfernb(char* tbuf, char* rbuf, uint32_t len)
{
    volatile uint32_t* paddr = bcm2835_spi0 + BCM2835_SPI0_CS/4;
    volatile uint32_t* fifo = bcm2835_spi0 + BCM2835_SPI0_FIFO/4;
	
    // This is Polled transfer as per section 10.6.1
    // BUG ALERT: what happens if we get interupted in this section, and someone else
    // accesses a different peripheral? 
	
    // Clear TX and RX fifos
    bcm2835_peri_set_bits(paddr, BCM2835_SPI0_CS_CLEAR, BCM2835_SPI0_CS_CLEAR);
	
    // Set TA = 1
    bcm2835_peri_set_bits(paddr, BCM2835_SPI0_CS_TA, BCM2835_SPI0_CS_TA);
	
    uint32_t i;
    for (i = 0; i < len; i++)
    {
		// Maybe wait for TXD
		while (!(bcm2835_peri_read(paddr) & BCM2835_SPI0_CS_TXD))
			delayMicroseconds(10);
		
		// Write to FIFO, no barrier
		bcm2835_peri_write_nb(fifo, tbuf[i]);
		
		// Wait for RXD
		while (!(bcm2835_peri_read(paddr) & BCM2835_SPI0_CS_RXD))
			delayMicroseconds(10);
		
		// then read the data byte
		rbuf[i] = bcm2835_peri_read_nb(fifo);
    }
    // Wait for DONE to be set
    while (!(bcm2835_peri_read_nb(paddr) & BCM2835_SPI0_CS_DONE))
		delayMicroseconds(10);
	
    // Set TA = 0, and also set the barrier
    bcm2835_peri_set_bits(paddr, 0, BCM2835_SPI0_CS_TA);
}


// Set output pin
void bcm2835_gpio_set(uint8_t pin)
{
    volatile uint32_t* paddr = bcm2835_gpio + BCM2835_GPSET0/4 + pin/32;
    uint8_t shift = pin % 32;
    bcm2835_peri_write(paddr, 1 << shift);
}

void bcm2835_gpio_fsel(uint8_t pin, uint8_t mode)
{
    // Function selects are 10 pins per 32 bit word, 3 bits per pin
    volatile uint32_t* paddr = bcm2835_gpio + BCM2835_GPFSEL0/4 + (pin/10);
    uint8_t   shift = (pin % 10) * 3;
    uint32_t  mask = BCM2835_GPIO_FSEL_MASK << shift;
    uint32_t  value = mode << shift;
    bcm2835_peri_set_bits(paddr, value, mask);
}

// safe read from peripheral
uint32_t bcm2835_peri_read(volatile uint32_t* paddr)
{
		uint32_t ret = *paddr;
		uint32_t dummy = *paddr;
		return ret;
}

// Delays for the specified number of microseconds with offset
void bcm2835_st_delay(uint64_t offset_micros, uint64_t micros)
{
    uint64_t compare = offset_micros + micros;
    while(bcm2835_st_read() < compare);
}

// microseconds
void bcm2835_delayMicroseconds(uint64_t micros)
{
    uint64_t        start;
    start =  bcm2835_st_read();
    bcm2835_st_delay(start, micros);
}

// read from peripheral without the read barrier
uint32_t bcm2835_peri_read_nb(volatile uint32_t* paddr)
{
		return *paddr;
}

// safe write to peripheral
void bcm2835_peri_write(volatile uint32_t* paddr, uint32_t value)
{
		*paddr = value;
		*paddr = value;
}


// write to peripheral without the write barrier
void bcm2835_peri_write_nb(volatile uint32_t* paddr, uint32_t value)
{
		*paddr = value;
}

// Set/clear only the bits in value covered by the mask
void bcm2835_peri_set_bits(volatile uint32_t* paddr, uint32_t value, uint32_t mask)
{
    uint32_t v = bcm2835_peri_read(paddr);
    v = (v & ~mask) | (value & mask);
    bcm2835_peri_write(paddr, v);
}

// Clear output pin
void bcm2835_gpio_clr(uint8_t pin)
{
    volatile uint32_t* paddr = bcm2835_gpio + BCM2835_GPCLR0/4 + pin/32;
    uint8_t shift = pin % 32;
    bcm2835_peri_write(paddr, 1 << shift);
}

// Set the state of an output
void bcm2835_gpio_write(uint8_t pin, uint8_t on)
{
    if (on)
		bcm2835_gpio_set(pin);
    else
		bcm2835_gpio_clr(pin);
}

void bcm2835_spi_chipSelect(uint8_t cs)
{
    volatile uint32_t* paddr = bcm2835_spi0 + BCM2835_SPI0_CS/4;
    // Mask in the CS bits of CS
    bcm2835_peri_set_bits(paddr, cs, BCM2835_SPI0_CS_CS);
}

void bcm2835_spi_setChipSelectPolarity(uint8_t cs, uint8_t active)
{
    volatile uint32_t* paddr = bcm2835_spi0 + BCM2835_SPI0_CS/4;
    uint8_t shift = 21 + cs;
    // Mask in the appropriate CSPOLn bit
    bcm2835_peri_set_bits(paddr, active << shift, 1 << shift);
}

// Set pullup/down
void bcm2835_gpio_pud(uint8_t pud)
{
    volatile uint32_t* paddr = bcm2835_gpio + BCM2835_GPPUD/4;
    bcm2835_peri_write(paddr, pud);
}

// Pullup/down clock
// Clocks the value of pud into the GPIO pin
void bcm2835_gpio_pudclk(uint8_t pin, uint8_t on)
{
    volatile uint32_t* paddr = bcm2835_gpio + BCM2835_GPPUDCLK0/4 + pin/32;
    uint8_t shift = pin % 32;
    bcm2835_peri_write(paddr, (on ? 1 : 0) << shift);
}


void bcm2835_gpio_set_pud(uint8_t pin, uint8_t pud)
{
    bcm2835_gpio_pud(pud);
    delayMicroseconds(10);
    bcm2835_gpio_pudclk(pin, 1);
    delayMicroseconds(10);
    bcm2835_gpio_pud(BCM2835_GPIO_PUD_OFF);
    bcm2835_gpio_pudclk(pin, 0);
}




