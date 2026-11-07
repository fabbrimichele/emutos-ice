/*
 * rt68ice_spi.c - SPI interface for rt68ice SD card driver
 */

#include "emutos.h"
#include "spi.h"

/* Word data writes/reads; byte receive and control accesses use odd addresses. */
#define RT68ICE_SPI_DATA  *(volatile UWORD*)(0xf20000) // Full data register
#define RT68ICE_SPI_RXLO  *(volatile UBYTE*)(0xf20001) // Low received byte
#define RT68ICE_SPI_CDST  *(volatile UBYTE*)(0xf20003) // Command/Status
#define RT68ICE_SPI_CONF  *(volatile UBYTE*)(0xf20005) // Config

#define RT68ICE_SPI_CONF_TRSZ(x)  (((x)&0b00000011)<<3)   // Transfer size: 00=4-bit, 01=8-bit, 10=12-bit, 11=16-bit
#define RT68ICE_SPI_CONF_CDIV(x)  (((x)&0b00000111)<<0)   // Clock divisor: 000=clk/2, 001=clk/4, ..., 111=clk/256

// RT68ICE_SPI_CDST
// Write access
#define RT68ICE_SPI_CDST_START(x) (((x)&0b00000001)<<0)   // Start bit, automatically clears
#define RT68ICE_SPI_CDST_CS(x)    (((x)&0b00000001)<<1)   // Chip select: 1 = assert, 0 = de-assert
#define RT68ICE_SPI_CDST_IRQE(x)  (((x)&0b00000001)<<2)   // IRQ enable: 1 = enable, 0 = disable
#define RT68ICE_SPI_CDST_SPIAD(x) (((x)&0b00000111)<<4)   // SPI address
// Read access
#define RT68ICE_SPI_CDST_BUSY     (0b00000001)            // Busy: 1 = transfer in progress, 0 = idle

// Identification mode: 
// - transfer size = 8 bit
// - clock divisor = 25 MHz / 128 = 195.3125 KHz
#define RT68ICE_SPI_IDENT_MODE    RT68ICE_SPI_CONF_TRSZ(0b01) | RT68ICE_SPI_CONF_CDIV(0b110)

// SD mode: 
// - transfer size = 8 bit
// - clock divisor = 25 MHz / 2 = 12.5 MHz
#define RT68ICE_SPI_SD_MODE       RT68ICE_SPI_CONF_TRSZ(0b01) | RT68ICE_SPI_CONF_CDIV(0b000)

/* Last byte-mode configuration, including the selected clock divisor. */
static UBYTE spi_byte_mode = RT68ICE_SPI_IDENT_MODE;

// Commands
#define RT68ICE_SPI_DEASSERT_CS   RT68ICE_SPI_CDST_SPIAD(0) | RT68ICE_SPI_CDST_IRQE(0) | RT68ICE_SPI_CDST_CS(0) | RT68ICE_SPI_CDST_START(0)
#define RT68ICE_SPI_ASSERT_CS     RT68ICE_SPI_CDST_SPIAD(0) | RT68ICE_SPI_CDST_IRQE(0) | RT68ICE_SPI_CDST_CS(1) | RT68ICE_SPI_CDST_START(0)
#define RT68ICE_SPI_START         RT68ICE_SPI_CDST_SPIAD(0) | RT68ICE_SPI_CDST_IRQE(0) | RT68ICE_SPI_CDST_CS(1) | RT68ICE_SPI_CDST_START(1)


void spi_clock_ident(void)
{
    KDEBUG(("rt68ice: spi_clock_ident, conf = 0x%02x\n", RT68ICE_SPI_IDENT_MODE));
    spi_byte_mode = RT68ICE_SPI_IDENT_MODE;
    RT68ICE_SPI_CONF = spi_byte_mode;
}

void spi_clock_mmc(void)
{
    // Not used for SD cards    
    KDEBUG(("rt68ice: spi_clock_mmc\n"));
}

void spi_clock_sd(void)
{
    KDEBUG(("rt68ice: spi_clock_sd, conf = 0x%02x\n", RT68ICE_SPI_SD_MODE));
    spi_byte_mode = RT68ICE_SPI_SD_MODE;
    RT68ICE_SPI_CONF = spi_byte_mode;
}

void spi_cs_assert(void)
{
    // KDEBUG(("rt68ice: spi_cs_assert, cdst = 0x%02x\n", RT68ICE_SPI_ASSERT_CS));
    RT68ICE_SPI_CDST = RT68ICE_SPI_ASSERT_CS;    
    spi_send_byte(0xff);  // dummy byte to force a write to the register
}

void spi_cs_unassert(void)
{   
    // KDEBUG(("rt68ice: spi_cs_unassert, cdst = 0x%02x\n", RT68ICE_SPI_DEASSERT_CS));
    RT68ICE_SPI_CDST = RT68ICE_SPI_DEASSERT_CS;
    spi_send_byte(0xff);  // dummy byte to force a write to the register
}

void spi_initialise(void)
{
    KDEBUG(("rt68ice: spi_initialise, conf = 0x%02x\n", RT68ICE_SPI_IDENT_MODE));
    spi_byte_mode = RT68ICE_SPI_IDENT_MODE;
    RT68ICE_SPI_CONF = spi_byte_mode;
}

UBYTE spi_recv_byte(void)
{
    spi_send_byte(0xff);    // Trigger 8 clock pulses
    return RT68ICE_SPI_RXLO;
}

void spi_send_byte(UBYTE input)
{    
    // 1. Wait for BUSY bit (bit 0 of RT68ICE_SPI_CDST) to be 0
    while(RT68ICE_SPI_CDST & RT68ICE_SPI_CDST_BUSY);

    // 2. Data writes require a word access, even for an 8-bit SPI transfer.
    RT68ICE_SPI_DATA = input;

    // 3. Start transfer: assert Start CDST[0] = 1
    RT68ICE_SPI_CDST = RT68ICE_SPI_START;

    // 4. Wait for transfer to finish before returning 
    // (required to finish reading commands)
    while(RT68ICE_SPI_CDST & RT68ICE_SPI_CDST_BUSY);
}

/*
 * Payload-only helpers.  Commands, tokens and CRC bytes still use byte mode.
 * The FPGA data register is accessed as a word; SPI shifts its MSB first.
 * Aligned RAM buffers use word accesses too (the 68000 is big-endian).
 * Unaligned buffers are packed/unpacked as bytes to avoid address errors.
 */
void spi_recv_block(UBYTE *buf, UWORD len)
{
    if (len >= 2) {
        while (RT68ICE_SPI_CDST & RT68ICE_SPI_CDST_BUSY)
            ;
        RT68ICE_SPI_CONF = (spi_byte_mode & ~RT68ICE_SPI_CONF_TRSZ(3))
                            | RT68ICE_SPI_CONF_TRSZ(3);
        /*
         * Hardware-specific to the RT68ICE spi_master core: START copies
         * the persistent TX buffer into a separate RX/TX shift register.
         * Transfers leave the TX buffer unchanged, so every START below
         * reuses these dummy 0xffff bits without reloading the registers.
         * Controllers with a shared data register or TX FIFO may instead
         * require dummy data to be supplied for every transfer.
         */
        RT68ICE_SPI_DATA = 0xffff;
        if (buf && !((ULONG)buf & 1)) {
            UWORD *words = (UWORD *)buf;
            while (len >= 2) {
                RT68ICE_SPI_CDST = RT68ICE_SPI_START;
                while (RT68ICE_SPI_CDST & RT68ICE_SPI_CDST_BUSY)
                    ;
                *words++ = RT68ICE_SPI_DATA;
                len -= 2;
            }
            buf = (UBYTE *)words;
        } else {
            while (len >= 2) {
                RT68ICE_SPI_CDST = RT68ICE_SPI_START;
                while (RT68ICE_SPI_CDST & RT68ICE_SPI_CDST_BUSY)
                    ;
                if (buf) {
                    UWORD value = RT68ICE_SPI_DATA;
                    *buf++ = value >> 8;
                    *buf++ = value;
                }
                len -= 2;
            }
        }
        RT68ICE_SPI_CONF = spi_byte_mode;
    }
    if (len) {
        UBYTE last = spi_recv_byte();
        if (buf)
            *buf = last;
    }
}

void spi_send_block(const UBYTE *buf, UWORD len)
{
    if (len >= 2) {
        while (RT68ICE_SPI_CDST & RT68ICE_SPI_CDST_BUSY)
            ;
        RT68ICE_SPI_CONF = (spi_byte_mode & ~RT68ICE_SPI_CONF_TRSZ(3))
                            | RT68ICE_SPI_CONF_TRSZ(3);
        if (!((ULONG)buf & 1)) {
            const UWORD *words = (const UWORD *)buf;
            while (len >= 2) {
                RT68ICE_SPI_DATA = *words++;
                RT68ICE_SPI_CDST = RT68ICE_SPI_START;
                while (RT68ICE_SPI_CDST & RT68ICE_SPI_CDST_BUSY)
                    ;
                len -= 2;
            }
            buf = (const UBYTE *)words;
        } else {
            while (len >= 2) {
                UWORD value = (UWORD)buf[0] << 8 | buf[1];
                buf += 2;
                RT68ICE_SPI_DATA = value;
                RT68ICE_SPI_CDST = RT68ICE_SPI_START;
                while (RT68ICE_SPI_CDST & RT68ICE_SPI_CDST_BUSY)
                    ;
                len -= 2;
            }
        }
        RT68ICE_SPI_CONF = spi_byte_mode;
    }
    if (len)
        spi_send_byte(*buf);
}
