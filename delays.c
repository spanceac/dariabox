#define _XTAL_FREQ 64000000
#include <p18f24k22.h>
#include <xc.h>
#include "delays.h"

void dly_us (UINT n)	/* Delay n microseconds (avr-gcc -Os) */
{
    int i = 0;
    for(i = 0; i < n; i++)
        __delay_us(1);
}