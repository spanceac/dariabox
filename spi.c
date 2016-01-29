#include <p18f24k22.h>

void spi_init(void)
{
    //SPI Master mode, clock = FOSC /(4 * (SSPxADD+1))
    SSP1CON1 = 10;
    SSP1ADD = 159; //100 KHz SPI clock
    SSP1BUF = 0;
    SSP1STATbits.SMP = 1;
    SSP1STATbits.CKE = 1;
    SSP1CON1bits.SSPEN1 = 1;
}

char spi_write(char data)
{
    SSP1BUF = data;
    while(SSP1STATbits.BF == 0);
    return SSP1BUF;
}

char spi_read(void)
{
    char c;
    c = spi_write(0xFF);
    return c;
}

void set_spi_max_speed(void)
{
    SSP1CON1bits.SSPEN1 = 0;
    SSP1ADD = 1; // set SPI speed to 64MHz/((SSP1ADD + 1) * 4) = 8MHz
    SSP1CON1bits.SSPEN1 = 1;
}
