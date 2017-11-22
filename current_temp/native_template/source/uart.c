/* uart.c - UART initialization & communication */
/* Reference material:
 * http://www.raspberrypi.org/wp-content/uploads/2012/02/BCM2835-ARM-Peripherals.pdf
 * Chapter 13: UART
 */
 
#include <stdint.h>
#include <mmio.h>
#include <uart.h>
 
enum 
{
    // The GPIO registers base address.
    GPIO_BASE = 0x20200000,
 
    // The offsets for reach register.
 
    // Controls actuation of pull up/down to ALL GPIO pins.
    GPPUD = (GPIO_BASE + 0x94),
 
    // Controls actuation of pull up/down for specific GPIO pin.
    GPPUDCLK0 = (GPIO_BASE + 0x98),
 
    // The base address for UART.
    UART0_BASE = 0x20201000,
 
    // The offsets for reach register for the UART.
    UART0_DR     = (UART0_BASE + 0x00),
    UART0_RSRECR = (UART0_BASE + 0x04),
    UART0_FR     = (UART0_BASE + 0x18),
    UART0_ILPR   = (UART0_BASE + 0x20),
    UART0_IBRD   = (UART0_BASE + 0x24),
    UART0_FBRD   = (UART0_BASE + 0x28),
    UART0_LCRH   = (UART0_BASE + 0x2C),
    UART0_CR     = (UART0_BASE + 0x30),
    UART0_IFLS   = (UART0_BASE + 0x34), // This should probably be IFSL
    UART0_IMSC   = (UART0_BASE + 0x38),
    UART0_RIS    = (UART0_BASE + 0x3C),
    UART0_MIS    = (UART0_BASE + 0x40),
    UART0_ICR    = (UART0_BASE + 0x44),
    UART0_DMACR  = (UART0_BASE + 0x48),
    UART0_ITCR   = (UART0_BASE + 0x80),
    UART0_ITIP   = (UART0_BASE + 0x84),
    UART0_ITOP   = (UART0_BASE + 0x88),
    UART0_TDR    = (UART0_BASE + 0x8C),
};

// FFF
enum {
    // Data register bits
    DR_OE = 1 << 11, // Overrun error
    DR_BE = 1 << 10, // Break error
    DR_PE = 1 <<  9, // Parity error
    DR_FE = 1 <<  8, // Framing error

    // Receive Status Register / Error Clear Register
    RSRECR_OE = 1 << 3, // Overrun error
    RSRECR_BE = 1 << 2, // Break error
    RSRECR_PE = 1 << 1, // Parity error
    RSRECR_FE = 1 << 0, // Framing error

    // Flag Register (depends on LCRH.FEN)
    FR_TXFE = 1 << 7, // Transmit FIFO empty
    FR_RXFF = 1 << 6, // Receive FIFO full
    FR_TXFF = 1 << 5, // Transmit FIFO full
    FR_RXFE = 1 << 4, // Receive FIFO empty
    FR_BUSY = 1 << 3, // BUSY transmitting data
    FR_CTS  = 1 << 0, // Clear To Send

    // Line Control Register
    LCRH_SPS   = 1 << 7, // sticky parity selected
    LCRH_WLEN  = 3 << 5, // word length (5, 6, 7 or 8 bit)
    LCRH_WLEN5 = 0 << 5, // word length 5 bit
    qLCRH_WLEN6 = 1 << 5, // word length 6 bit
    LCRH_WLEN7 = 2 << 5, // word length 7 bit
    LCRH_WLEN8 = 3 << 5, // word length 8 bit
    LCRH_FEN   = 1 << 4, // Enable FIFOs
    LCRH_STP2  = 1 << 3, // Two stop bits select
    LCRH_EPS   = 1 << 2, // Even Parity Select
    LCRH_PEN   = 1 << 1, // Parity enable
    LCRH_BRK   = 1 << 0, // send break

    // Control Register
    CR_CTSEN  = 1 << 15, // CTS hardware flow control
    CR_RTSEN  = 1 << 14, // RTS hardware flow control
    CR_RTS    = 1 << 11, // (not) Request to send
    CR_RXE    = 1 <<  9, // Receive enable
    CR_TXW    = 1 <<  8, // Transmit enable
    CR_LBE    = 1 <<  7, // Loopback enable
    CR_UARTEN = 1 <<  0, // UART enable

    // Interrupts (IMSC / RIS / MIS / ICR)
    INT_OER   = 1 << 10, // Overrun error interrupt
    INT_BER   = 1 <<  9, // Break error interrupt
    INT_PER   = 1 <<  8, // Parity error interrupt
    INT_FER   = 1 <<  7, // Framing error interrupt
    INT_RTR   = 1 <<  6, // Receive timeout interrupt
    INT_TXR   = 1 <<  5, // Transmit interrupt
    INT_RXR   = 1 <<  4, // Receive interrupt
    INT_DSRRM = 1 <<  3, // unsupported / write zero
    INT_DCDRM = 1 <<  2, // unsupported / write zero
    INT_CTSRM = 1 <<  1, // nUARTCTS modem interrupt
    INT_RIRM  = 1 <<  0, // unsupported / write zero
    INT_ALL = 0x7F2,

    IFSL_RXIFLSEL = 7 << 3,     // Receive interrupt FIFO level select
    IFSL_RX_1_8   = 0b000 << 3, // Receive FIFO 1/8 full
    IFSL_RX_1_4   = 0b001 << 3, // Receive FIFO 1/4 full
    IFSL_RX_1_2   = 0b010 << 3, // Receive FIFO 1/2 full
    IFSL_RX_3_4   = 0b011 << 3, // Receive FIFO 3/4 full
    IFSL_RX_7_8   = 0b100 << 3, // Receive FIFO 7/8 full
    IFSL_TXIFLSEL = 7 << 0,     // Transmit interrupt FIFO level select
    IFSL_TX_1_8   = 0b000 << 0, // Transmit FIFO 1/8 full
    IFSL_TX_1_4   = 0b001 << 0, // Transmit FIFO 1/4 full
    IFSL_TX_1_2   = 0b010 << 0, // Transmit FIFO 1/2 full
    IFSL_TX_3_4   = 0b011 << 0, // Transmit FIFO 3/4 full
    IFSL_TX_7_8   = 0b100 << 0, // Transmit FIFO 7/8 full
};

// End FFF
 
/*
 * delay function
 * int32_t delay: number of cycles to delay
 *
 * This just loops <delay> times in a way that the compiler
 * wont optimize away.
 */
static void delay(int32_t count) 
{
   __asm__ volatile("__delay%=: subs %[count], %[count], #1; bne __delay%=\n"
	     : : [count]"r"(count) : "cc");
}
 
/*
 * Initialize UART0.
 */
void uart_init() 
{
    //uart_puts("\nuart_init\n");
    // Disable UART0.
    mmio_write(UART0_CR, 0x00000000);
    // Setup the GPIO pin 14 && 15.
 
    // Disable pull up/down for all GPIO pins & delay for 150 cycles.
    mmio_write(GPPUD, 0x00000000);
    delay(150);
 
    // Disable pull up/down for pin 14,15 & delay for 150 cycles.
    mmio_write(GPPUDCLK0, (1 << 14) | (1 << 15));
    delay(150);
 
    // Write 0 to GPPUDCLK0 to make it take effect.
    mmio_write(GPPUDCLK0, 0x00000000);
 
    // Clear pending interrupts.
    mmio_write(UART0_ICR, 0x7FF);
 
    // Set integer & fractional part of baud rate.
    // Divider = UART_CLOCK/(16 * Baud)
    // Fraction part register = (Fractional part * 64) + 0.5
    // UART_CLOCK = 3000000; Baud = 115200.
 
    // Divider = 3000000/(16 * 115200) = 1.627 = ~1.
    // Fractional part register = (.627 * 64) + 0.5 = 40.6 = ~40.
    mmio_write(UART0_IBRD, 1);
    mmio_write(UART0_FBRD, 40);
 
    // Disable FIFO. Make 8 bit data transmission (1 stop bit, no parity).
    mmio_write(UART0_LCRH, (1 << 4) | (1 << 5) | (1 << 6));
 
    // Engineer the Interrupt for UART0 Receive // D: save state here somehow. I think he does something similar elsewhere in the code
    //mmio_write(UART0_IMSC, 0x0010);

    mmio_write(UART0_IFLS, IFSL_RX_1_2 | IFSL_TX_1_2);

    mmio_write(UART0_IMSC, (1 << 1) | (1 << 4) | (1 << 5) | (1 << 6) |
			   (1 << 7) | (1 << 8) | (1 << 9) | (1 << 10));
 
    // Enable UART0, receive & transfer part of UART.
    mmio_write(UART0_CR, (1 << 0) | (1 << 8) | (1 << 9));
}
 
/*
 * Transmit a byte via UART0.
 * uint8_t Byte: byte to send.
 */
void uart_putc(uint8_t byte) // D: pg179 of arm pdf. If FIFO is enabled data written to this location is pushed onto the transmit FIFO. OE overrun error bit 11 set to 1 if the fifo already full and receives an input
// D: if the fifo's are enabled the data byte and the 4 bit status is pushed onto the 12 bit wide receive fifo
{
    // test for UART to become ready to transmit
    while (1) 
	{
        if (!(mmio_read(UART0_FR) & (1 << 5))) 
		{
	    break;
		}
    }
    mmio_write(UART0_DR, byte);
}

uint8_t uart_readc(void)
{
	//uart_puts("\r\nuart_readc\r\n");
	// test for UART to become ready to read, can also test the interrupt receive flag.
	while (1) 
	{
        if (!(mmio_read(UART0_FR) & (1 << 4))) 
		{
		//uart_puts("\r\nIF STATEMENT WTF\r\n");
			break;
		}
    	}
	//uart_puts("\r\nend uart_readc\r\n");
	return mmio_read(UART0_DR);
}
 
/*
 * print a string to the UART one character at a time
 * const char *str: 0-terminated string
 */
void uart_puts(const char *str) 
{
    while (*str) 
	{
        uart_putc(*str++);
    	}
}
