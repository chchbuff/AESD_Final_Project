/***********************************************************************
 * @file      		pulse_sensor.c
 * @version   		0.1
 * @brief		main implementation file for pulse rate sensor
 *
 * @author    		Amey More, Amey.More@Colorado.edu
 * @date      		Dec 5, 2023
 *
 * @institution 	University of Colorado Boulder (UCB)
 * @course      	ECEN 5713: Advanced Embedded Software Development
 * @instructor  	Dan Walkes
 *
 * @assignment 	Final Project
 * @due        	Dec 13, 2023 at 11:59 PM
 *
 * @references
 * https://raw.githubusercontent.com/raspberrypi/linux/rpi-3.10.y/
 * Documentation/spi/spidev_test.c
 ************************************************************************/

/**************************** Header Files ******************************/
#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>

/**************************** Defines  **********************************/
#define ARRAY_SIZE(a) 			(sizeof(a) / sizeof((a)[0]))
#define ADC_CHANNEL_SELECT_MASK 	(0xC0)

/**************************** Global Variables **************************/
static const char *device = "/dev/spidev0.0";
static uint8_t mode;
static uint8_t bits = 8;
static uint32_t speed = 500000;
static uint16_t delay;

/**************************** Function Declarations *********************/
static void pabort(const char *s);
static void transfer(int fd);
static int pulse_read(int fd);

/**************************** main function *****************************/
int main(int argc, char *argv[])
{
	int ret = 0;
	int fd;
	int execute_test = 0;
	
	if(argc > 1)
	{
		if(strcmp("test",argv[0]) == 0)
		{
			execute_test = 1;
		}
	}

	fd = open(device, O_RDWR);
	if (fd < 0)
		pabort("can't open device");

	/*
	 * spi mode
	 */
	ret = ioctl(fd, SPI_IOC_WR_MODE, &mode);
	if (ret == -1)
		pabort("can't set spi mode");

	ret = ioctl(fd, SPI_IOC_RD_MODE, &mode);
	if (ret == -1)
		pabort("can't get spi mode");

	/*
	 * bits per word
	 */
	ret = ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits);
	if (ret == -1)
		pabort("can't set bits per word");

	ret = ioctl(fd, SPI_IOC_RD_BITS_PER_WORD, &bits);
	if (ret == -1)
		pabort("can't get bits per word");

	/*
	 * max speed hz
	 */
	ret = ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed);
	if (ret == -1)
		pabort("can't set max speed hz");

	ret = ioctl(fd, SPI_IOC_RD_MAX_SPEED_HZ, &speed);
	if (ret == -1)
		pabort("can't get max speed hz");

	printf("spi mode: %d\n", mode);
	printf("bits per word: %d\n", bits);
	printf("max speed: %d Hz (%d KHz)\n", speed, speed/1000);
	
	if(execute_test == 1)
	{
		printf("\n\n*** Execute Test ***\n\n");
		transfer(fd);
		goto exit;
	}
	
	while(1)
	{
		ret = pulse_read(fd);
		if(ret == -1)
			goto exit;
		 usleep(200);
	}
	
exit:
	close(fd);
	
	printf("\n\n*** End App ***\n\n");

	return ret;
}


/**************************** Function Definitions **********************/

/*****************************************
 * @brief	Error and abort function
 ****************************************/
static void pabort(const char *s)
{
	perror(s);
	abort();
}

/*****************************************
 * @brief	SPI Transfer data function
 ****************************************/
static void transfer(int fd)
{
	int ret;
	uint8_t tx[] = {
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
		0x40, 0x00, 0x00, 0x00, 0x00, 0x95,
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
		0xDE, 0xAD, 0xBE, 0xEF, 0xBA, 0xAD,
		0xF0, 0x0D,
	};
	uint8_t rx[ARRAY_SIZE(tx)] = {0, };
	struct spi_ioc_transfer tr = {
		.tx_buf = (unsigned long)tx,
		.rx_buf = (unsigned long)rx,
		.len = ARRAY_SIZE(tx),
		.delay_usecs = delay,
		.speed_hz = speed,
		.bits_per_word = bits,
	};

	ret = ioctl(fd, SPI_IOC_MESSAGE(1), &tr);
	if (ret < 1)
		pabort("can't send spi message");

	for (ret = 0; ret < ARRAY_SIZE(tx); ret++) {
		if (!(ret % 6))
			puts("");
		printf("%.2X ", rx[ret]);
	}
	puts("");
}


static int pulse_read(int fd)
{
	printf("Reading pulse rate sensor\n");
	
	int ret = 0;
	int i;
	// Channel 0 of 8-channel ADC
	uint8_t channel_n = 0;
	
	// data to be transferred
	uint8_t data[] = {
		((ADC_CHANNEL_SELECT_MASK) | (channel_n << 3)),
	};
	// data received
	uint8_t value[ARRAY_SIZE(data)] = {0, };
	
	// SPI Transfer data struct
	struct spi_ioc_transfer tr = {
		.tx_buf = (unsigned long)data,
		.rx_buf = (unsigned long)value,
		.len = ARRAY_SIZE(data),
		.delay_usecs = delay,
		.speed_hz = speed,
		.bits_per_word = bits,
	};
	
	ret = ioctl(fd, SPI_IOC_MESSAGE(1), &tr);
	if (ret < 1)
	{
		printf("can't send spi message");
		return -1;
	}
		//pabort("can't send spi message");

	for (i = 0; i < ARRAY_SIZE(data); i++)
	{
		printf("%.2X ", value[i]);
	}
	puts("");
	puts("");
	
	return 0;
	
}
