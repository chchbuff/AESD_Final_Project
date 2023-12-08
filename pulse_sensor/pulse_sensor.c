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
#include <getopt.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>

/**************************** Defines  **********************************/
#define ARRAY_SIZE(a) 			(sizeof(a) / sizeof((a)[0]))
#define ADC_CHANNEL_0 			(0xC0)

/**************************** Global Variables **************************/
static const char *device = "/dev/spidev0.0";
static uint8_t mode;
static uint8_t bits = 8;
static uint32_t speed = 250000;
static uint16_t delay = 100;
int execute_test = 0;

/**************************** Function Declarations *********************/
static void pabort(const char *s);
static void print_usage(const char *prog);
static void parse_opts(int argc, char *argv[]);

static void transfer(int fd);
static int pulse_read(int fd);

/**************************** main function *****************************/
int main(int argc, char *argv[])
{
	int ret = 0;
	int fd;
	
	parse_opts(argc, argv);

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
		 sleep(1);
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
 * @brief	To print usage
 ****************************************/
static void print_usage(const char *prog)
{
	printf("Usage: %s [-DsbdlHOLC3]\n", prog);
	puts("  -D --device   device to use (default /dev/spidev1.1)\n"
	     "  -s --speed    max speed (Hz)\n"
	     "  -d --delay    delay (usec)\n"
	     "  -b --bpw      bits per word \n"
	     "  -l --loop     loopback\n"
	     "  -H --cpha     clock phase\n"
	     "  -O --cpol     clock polarity\n"
	     "  -L --lsb      least significant bit first\n"
	     "  -C --cs-high  chip select active high\n"
	     "  -3 --3wire    SI/SO signals shared\n");
	exit(1);
}

/*****************************************
 * @brief	To parse arguments
 ****************************************/
static void parse_opts(int argc, char *argv[])
{
	while (1) {
		static const struct option lopts[] = {
			{ "device",  1, 0, 'D' },
			{ "speed",   1, 0, 's' },
			{ "delay",   1, 0, 'd' },
			{ "bpw",     1, 0, 'b' },
			{ "loop",    0, 0, 'l' },
			{ "cpha",    0, 0, 'H' },
			{ "cpol",    0, 0, 'O' },
			{ "lsb",     0, 0, 'L' },
			{ "cs-high", 0, 0, 'C' },
			{ "3wire",   0, 0, '3' },
			{ "no-cs",   0, 0, 'N' },
			{ "ready",   0, 0, 'R' },
			{ "test",   0, 0, 't' },
			{ NULL, 0, 0, 0 },
		};
		int c;

		c = getopt_long(argc, argv, "D:s:d:b:lHOLC3NRt", lopts, NULL);

		if (c == -1)
			break;

		switch (c) {
		case 'D':
			device = optarg;
			break;
		case 's':
			speed = atoi(optarg);
			break;
		case 'd':
			delay = atoi(optarg);
			break;
		case 'b':
			bits = atoi(optarg);
			break;
		case 'l':
			mode |= SPI_LOOP;
			break;
		case 'H':
			mode |= SPI_CPHA;
			break;
		case 'O':
			mode |= SPI_CPOL;
			break;
		case 'L':
			mode |= SPI_LSB_FIRST;
			break;
		case 'C':
			mode |= SPI_CS_HIGH;
			break;
		case '3':
			mode |= SPI_3WIRE;
			break;
		case 'N':
			mode |= SPI_NO_CS;
			break;
		case 'R':
			mode |= SPI_READY;
			break;
		case 't':
			execute_test = 1;
			break;
		default:
			print_usage(argv[0]);
			break;
		}
	}
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

/*****************************************
 * @brief	SPI pulse sensor read fn
 ****************************************/
static int pulse_read(int fd)
{
	printf("Reading pulse rate sensor\n");
	
	int ret = 0;
	uint16_t ADC_value = 0;
	
	// transmit buffer
	uint8_t data[] = {
		ADC_CHANNEL_0,
		0x00,
		0x00,	// dummy data
	};
	// receive buffer
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
	
	ADC_value = ( ((value[0] & 0x07) << 7) | (value[1] & 0xFE) );
	printf("%d\n\n", ADC_value);
	
	return 0;
	
}
