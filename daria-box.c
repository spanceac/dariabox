#define _XTAL_FREQ 64000000

#include <p18f24k22.h>
#include "spi.h"
#include <xc.h>
#include <stdlib.h>
#include <string.h>
#include "petitfat/pff.h"

/* 16 MHz Crystal * 4 PLL = 64 MHz osc -> 16 MHz Fcy */
#pragma config FOSC=HSHP, PLLCFG=ON, WDTEN = OFF, LVP = OFF
#define TIMER_TICK_NS 125 // 8MHZ timer clock
#define INTERRUPT_TICK_NS 23000 //44,1 KHz
#define CORRECTION 60
#define TIMER_LOAD_VAL 0xFF - ((INTERRUPT_TICK_NS) / (TIMER_TICK_NS)) + CORRECTION
#define PERIPHERALS_ENABLE() PORTAbits.RA1 = 1
#define PERIPHERALS_DISABLE() PORTAbits.RA1 = 0

FATFS FatFs;		/* FatFs work area needed for each volume */
bit get_next_buffer = 0;
bit finished_play = 0;
char buffer_play[512];
char read_buffer[512];
bit stop_playing = 0;

void delay_ms(int nr)
{
    int i;
    for(i = 0; i < nr; i++)
        __delay_ms(1);
}

void interrupt timer0_interrupt(void)
{
    static int i = 0;
    if(INTCONbits.TMR0IF == 0)
        return;
    T0CONbits.TMR0ON = 0;
    INTCONbits.T0IF = 0;

    if(stop_playing)
    {
        i = 0;
        stop_playing = 0;
        TMR0L = TIMER_LOAD_VAL;
        return;
    }
    
    //play
    CCPR4L = buffer_play[i] >> 2;
    CCP4CON |= buffer_play[i] << 6;

    i++;
    if(i == 450)
        get_next_buffer = 1;
    if(i == 512)
        i = 0;
    TMR0L = TIMER_LOAD_VAL;
    T0CONbits.TMR0ON = 1;
}

void int2_enable(void)
{
    INTCON2bits.INTEDG2 = 0; //interrupt on falling edge
    INTCON3bits.INT2IE = 1;
    INTCON3bits.INT2IF = 0;
}

void adc_init(void)
{
    ADCON0 = 1; // start ADC module, sample on AN0
    ADCON1 = 0;
    ADCON2 = 165;
}

int get_rand_seed_from_adc(void)
{
    ADCON0bits.GODONE = 1;
    while(ADCON0bits.GODONE);
    return (ADRESL & 7);
}

void timer0_init(void)
{
    T0CON = 0; //8 bit timer, 1:2 prescaler
    TMR0H = 0xFF;
    TMR0L = TIMER_LOAD_VAL;
}

void pwm_init(void)
{
    // CCP4 -> simple pwm for p18f25k22
    CCPTMRS1 = 0; //use TMR2 for CCP4 PWM
    PR2 = 64; //PWM period = 4us -> PWM freq = 250 KHz
    CCP4CON = 0x0C; // CCP4 -> PWM mode
    CCPR4L = 32; //duty cycle
    PIR1bits.TMR2IF = 0;
    T2CONbits.TMR2ON = 1; //start timer 2, 1:1 prescaler
    TRISBbits.RB0 = 0; //set CCP4 as output
}

int nr_of_files_on_card(void)
{
    FRESULT res;
    FILINFO fno;
    DIR dir;
    int count = 0;

    res = pf_opendir(&dir, "/");
    if (res == FR_OK)
    {
        for (;;)
        {
            res = pf_readdir(&dir, &fno);
            if(res != FR_OK || fno.fname[0] == 0) break;
            if(!(fno.fattrib & AM_DIR)) //if it's a file name and not a dir
                count ++;
        }
    }
    return count;
}

void get_file_name_from_nr(char *file_name, int nr)
{
    FRESULT res;
    FILINFO fno;
    DIR dir;
    int count = 0;

    res = pf_opendir(&dir, "/");
    if (res == FR_OK)
    {
        while(nr > 0)
        {
            res = pf_readdir(&dir, &fno);
            if(res != FR_OK || fno.fname[0] == 0) break;
            if(!(fno.fattrib & AM_DIR)) //if it's a file name and not a dir
                nr--;
        }
    }
    strcpy(file_name, fno.fname);
}

void main(void)
{
    int ret, i, nr_of_files, rand_seed, nr, rand_nr, throw_nr, swap;
    int played_nr = 0;
    UINT br;
    char file_name[8] = {0};
    char shuffled_list[100];
    //RC3 -> SCL1, RC4 -> SDI1, RC5 -> SDO1, RC6 -> /CS
    /*plans: RB2 -> power on(INT2), RB3 -> next button,
     * RA0 -> analog-in, RA1 --> peripherals power enable */
    PORTC = 0;
    TRISCbits.TRISC3 = 0; // clock output
    TRISCbits.TRISC4 = 1; //data input
    ANSELCbits.ANSC4 = 0; ANSELCbits.ANSC5 = 0;
    TRISCbits.TRISC5 = 0; //data output
    TRISCbits.TRISC6 = 0; //chip select
    TRISBbits.TRISB1 = 0;
    TRISBbits.TRISB3 = 1; ANSELBbits.ANSB3 = 0;
    TRISAbits.TRISA0 = 1; ANSELAbits.ANSA0 = 1; // RA0 analog input
    //TRISAbits.TRISA1 = 0; ANSELAbits.ANSA1 = 0; // periphera power enable
    PORTC = 0;
    PORTCbits.RC5 = 1;
    PORTCbits.RC6 = 1;
    PORTBbits.RB1 = 1;
    TRISBbits.RB2 = 1; // INTP2
    ANSELBbits.ANSB2 = 0;
    INTCON = (1 << 7) | (1 << 6); //enable interrupts

    //PERIPHERALS_ENABLE();
    spi_init();
    timer0_init();
    pwm_init();
    adc_init();
    //int2_enable();
    rand_seed = get_rand_seed_from_adc();
    srand(rand_seed);
    
    INTCONbits.TMR0IE = 1; //enable TMR0 int
    pf_mount(&FatFs);		/* Give a work area to the default drive */
    PORTBbits.RB1 = 0;
    nr_of_files = nr_of_files_on_card();
    
    for(i = 0; i < nr_of_files; i++)
	{
		shuffled_list[i] = i + 1;
	}
    throw_nr = nr_of_files;
	for(i = 0; i < nr_of_files; i++)
	{
		rand_nr = rand() % throw_nr;
		swap = shuffled_list[rand_nr];
		shuffled_list[rand_nr] = shuffled_list[throw_nr - 1];
		shuffled_list[throw_nr - 1] = swap;
		throw_nr--;
	};
    PORTBbits.RB1 = 1;
    while(1)
    {
        if(played_nr == nr_of_files)
            played_nr = 0;
        memset(file_name, 0, 8);
        
        get_file_name_from_nr(file_name, shuffled_list[played_nr]);
        ret = pf_open(file_name);
        if (ret == FR_OK)
        {
            pf_read(read_buffer, 512, &br);
            for(i = 0; i < 512; i++)
                buffer_play[i] = read_buffer[i];
            PORTBbits.RB1 = 0;
            T0CONbits.TMR0ON = 1;

            while(1)
            {
                PORTBbits.RB1 = 0;
                pf_read(read_buffer, 512, &br);
                PORTBbits.RB1 = 1;
                //if we read less than a sector, or the next button was pressed
                if(br < 512 || PORTBbits.RB3 == 0)
                {
                    stop_playing = 1;
                    break;
                }

                while(get_next_buffer == 0);
                for(i = 0; i < 512; i++)
                    buffer_play[i] = read_buffer[i];
                get_next_buffer = 0;
            }
        }
        played_nr++;
    }
}