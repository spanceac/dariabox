/* 
 * File:   spi.h
 * Author: spanceac
 *
 * Created on March 16, 2015, 9:26 AM
 */

#ifndef SPI_H
#define	SPI_H

#ifdef	__cplusplus
extern "C" {
#endif




#ifdef	__cplusplus
}
#endif

#endif	/* SPI_H */


void spi_init(void);
int spi_write(char data);
char spi_read(void);
void set_spi_max_speed(void);