/*
 * spi_rt68f.c - SPI interface for rt68f SD card driver
 */

#include "emutos.h"
#include "spi.h"

// Byte access -> odd addresses
#define RT68F_SPI_DTLW  *(volatile UBYTE*)(0xf20001) // Data LSB
#define RT68F_SPI_DTHI  *(volatile UBYTE*)(0xf20003) // Data MSB
#define RT68F_SPI_CDST  *(volatile UBYTE*)(0xf20005) // Command/Status
#define RT68F_SPI_CONF  *(volatile UBYTE*)(0xf20007) // Config

#define RT68F_SPI_CONF_TRSZ(x)  (((x)&0b00000011)<<3)   // Transfer size: 00=4-bit, 01=8-bit, 10=12-bit, 11=16-bit
#define RT68F_SPI_CONF_CDIV(x)  (((x)&0b00000111)<<0)   // Clock divisor: 000=clk/2, 001=clk/4, ..., 111=clk/256

// RT68F_SPI_CDST
// Write access
#define RT68F_SPI_CDST_START(x) (((x)&0b00000001)<<0)   // Start bit, automatically clears
#define RT68F_SPI_CDST_CS(x)    (((x)&0b00000001)<<1)   // Chip select: 1 = assert, 0 = de-assert
#define RT68F_SPI_CDST_IRQE(x)  (((x)&0b00000001)<<2)   // IRQ enable: 1 = enable, 0 = disable
#define RT68F_SPI_CDST_SPIAD(x) (((x)&0b00000111)<<4)   // SPI address
// Read access
#define RT68F_SPI_CDST_BUSY     (0b00000001)            // Busy: 1 = transfer in progress, 0 = idle

// Identification mode: 
// - transfer size = 8 bit
// - clock divisor = 25 MHz / 128 = 195.3125 KHz
#define RT68F_SPI_IDENT_MODE    RT68F_SPI_CONF_TRSZ(0b01) | RT68F_SPI_CONF_CDIV(0b110)

// SD mode: 
// - transfer size = 8 bit
// - clock divisor = 25 MHz / 2 = 12.5 MHz
#define RT68F_SPI_SD_MODE       RT68F_SPI_CONF_TRSZ(0b01) | RT68F_SPI_CONF_CDIV(0b000)

// Commands
#define RT68F_SPI_DEASSERT_CS   RT68F_SPI_CDST_SPIAD(0) | RT68F_SPI_CDST_IRQE(0) | RT68F_SPI_CDST_CS(0) | RT68F_SPI_CDST_START(0)
#define RT68F_SPI_ASSERT_CS     RT68F_SPI_CDST_SPIAD(0) | RT68F_SPI_CDST_IRQE(0) | RT68F_SPI_CDST_CS(1) | RT68F_SPI_CDST_START(0)
#define RT68F_SPI_START         RT68F_SPI_CDST_SPIAD(0) | RT68F_SPI_CDST_IRQE(0) | RT68F_SPI_CDST_CS(1) | RT68F_SPI_CDST_START(1)


// TODO: it has to be integrated in the other files, refert to the repository emutos-rt68f

void spi_clock_ident(void)
{
    KDEBUG(("rt68f: spi_clock_ident, conf = 0x%02x\n", RT68F_SPI_IDENT_MODE));
    RT68F_SPI_CONF = RT68F_SPI_IDENT_MODE;
}

void spi_clock_mmc(void)
{
    // Not used for SD cards    
    KDEBUG(("rt68f: spi_clock_mmc\n"));
}

void spi_clock_sd(void)
{
    KDEBUG(("rt68f: spi_clock_sd, conf = 0x%02x\n", RT68F_SPI_SD_MODE));
    RT68F_SPI_CONF = RT68F_SPI_SD_MODE;
}

void spi_cs_assert(void)
{
    // KDEBUG(("rt68f: spi_cs_assert, cdst = 0x%02x\n", RT68F_SPI_ASSERT_CS));
    RT68F_SPI_CDST = RT68F_SPI_ASSERT_CS;    
    spi_send_byte(0xff);  // dummy byte to force a write to the register
}

void spi_cs_unassert(void)
{   
    // KDEBUG(("rt68f: spi_cs_unassert, cdst = 0x%02x\n", RT68F_SPI_DEASSERT_CS));
    RT68F_SPI_CDST = RT68F_SPI_DEASSERT_CS;
    spi_send_byte(0xff);  // dummy byte to force a write to the register
}

void spi_initialise(void)
{
    KDEBUG(("rt68f: spi_initialise, conf = 0x%02x\n", RT68F_SPI_IDENT_MODE));
    RT68F_SPI_CONF = RT68F_SPI_IDENT_MODE;
}

UBYTE spi_recv_byte(void)
{
    spi_send_byte(0xff);    // Trigger 8 clock pulses
    return RT68F_SPI_DTLW;    
}

void spi_send_byte(UBYTE input)
{    
    // 1. Wait for BUSY bit (bit 0 of RT68F_SPI_CDST) to be 0
    while(RT68F_SPI_CDST & RT68F_SPI_CDST_BUSY);

    // 2. Load the data into Data Low (Address 0)
    RT68F_SPI_DTLW = input;

    // 3. Start transfer: assert Start CDST[0] = 1
    RT68F_SPI_CDST = RT68F_SPI_START;

    // 4. Wait for transfer to finish before returning 
    // (required to finish reading commands)
    while(RT68F_SPI_CDST & RT68F_SPI_CDST_BUSY);
}
