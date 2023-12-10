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
 *
 * https://raw.githubusercontent.com/raspberrypi/linux/rpi-3.10.y/
 * Documentation/spi/spidev_test.c
 * 
 * https://github.com/WorldFamousElectronics/Raspberry_Pi/blob/master/
 * PulseSensor_C_Pi/Pulse_Sensor_Timer/PulseSensor_timer.c
 *
 ************************************************************************/

/**************************** Header Files ******************************/
#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>

/**************************** Defines  **********************************/
#define ARRAY_SIZE(a) 			(sizeof(a) / sizeof((a)[0]))
#define ADC_CHANNEL_0 			(0xC0)

//For BPM conversion
#define OPT_R (10)        	// min uS allowed lag btw alarm and callback
#define OPT_U (2000)      	// sample time uS between alarms
#define TIME_OUT (30000000)    // uS time allowed without callback response
#define SEC_TO_US(sec) ((sec)*1000000) // Convert seconds to microseconds
#define NS_TO_US(ns)    ((ns)/1000) // Convert nanoseconds to microseconds


/**************************** Global Variables **************************/
static const char *device = "/dev/spidev0.0";
static uint8_t mode;
static uint8_t bits = 8;
static uint32_t speed = 250000;
static uint16_t delay = 0;
int execute_test = 0;
int spi_fd;

// VARIABLES USED TO DETERMINE SAMPLE JITTER & TIME OUT
volatile unsigned int eventCounter, thisTime, lastTime, elapsedTime, jitter;
volatile int sampleFlag = 0;
volatile int sumJitter, firstTime, secondTime, duration;
unsigned int timeOutStart, dataRequestStart, m;

// VARIABLES USED TO DETERMINE BPM
volatile int Signal;
volatile unsigned int sampleCounter;
volatile int threshSetting,lastBeatTime;
volatile int thresh = 550;
volatile int P = 512;        		// set P default
volatile int T = 512;              	// set T default
volatile int firstBeat = 1;        	// set these to avoid noise
volatile int secondBeat = 0;      	// when we get the heartbeat back
volatile int QS = 0;
volatile int rate[10];
volatile int BPM = 0;
volatile int IBI = 600;    		// 600ms per beat = 100 Beats Per Minute
volatile int Pulse = 0;
volatile int amp = 100;            	// beat amplitude 1/10 of input range.

/**************************** Function Declarations *********************/
/* Application functions */
static void pabort(const char *s);
static void print_usage(const char *prog);
static void parse_opts(int argc, char *argv[]);

/* SPI Functions */
static void spi_transfer_test(int fd);
static int pulse_read(int fd);

/* BPM Functions */
void get_bpm();
uint64_t micros();
void initPulseSensorVariables(void);
void startTimer(int latency, unsigned int micros);
void getPulse(int sig_num);

/**************************** main function *****************************/
int main(int argc, char *argv[])
{
	int ret = 0;
	
	parse_opts(argc, argv);

	spi_fd = open(device, O_RDWR);
	if (spi_fd < 0)
		pabort("can't open device");

	/*
	 * spi mode
	 */
	ret = ioctl(spi_fd, SPI_IOC_WR_MODE, &mode);
	if (ret == -1)
		pabort("can't set spi mode");

	ret = ioctl(spi_fd, SPI_IOC_RD_MODE, &mode);
	if (ret == -1)
		pabort("can't get spi mode");

	/*
	 * bits per word
	 */
	ret = ioctl(spi_fd, SPI_IOC_WR_BITS_PER_WORD, &bits);
	if (ret == -1)
		pabort("can't set bits per word");

	ret = ioctl(spi_fd, SPI_IOC_RD_BITS_PER_WORD, &bits);
	if (ret == -1)
		pabort("can't get bits per word");

	/*
	 * max speed hz
	 */
	ret = ioctl(spi_fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed);
	if (ret == -1)
		pabort("can't set max speed hz");

	ret = ioctl(spi_fd, SPI_IOC_RD_MAX_SPEED_HZ, &speed);
	if (ret == -1)
		pabort("can't get max speed hz");

	//printf("spi mode: %d\n", mode);
	//printf("bits per word: %d\n", bits);
	//printf("max speed: %d Hz (%d KHz)\n", speed, speed/1000);
	
	if(execute_test == 1)
	{
		printf("\n\n*** Execute Test ***\n\n");
		spi_transfer_test(spi_fd);
		goto exit;
	}
	
	get_bpm();
	
exit:
	close(spi_fd);
	
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
	     "  -3 --3wire    SI/SO signals shared\n"
	     "  -t --test     execute spi transfer test\n");
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
static void spi_transfer_test(int fd)
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
		printf("can't send spi message\n");
		return -1;
	}
	
	ADC_value = ( ((value[0] & 0x07) << 7) | (value[1] & 0xFE) );
	//printf("ADC Read Value %d\n\n", ADC_value);
	
	return ADC_value;	
}


void get_bpm()
{
	int ret;
	// to store MQTT command format
	char BPM_MQTT_cmd[256];
	// initilaize Pulse Sensor beat finder
	initPulseSensorVariables();
	// start sampling
	startTimer(OPT_R, OPT_U);
	
	while(1)
    	{
        	if(sampleFlag)
        	{
            		sampleFlag = 0;
            		timeOutStart = micros();
            		
            		// PRINT DATA TO TERMINAL
            		printf("BPM: %d\n", BPM);
            		
            		if(BPM >=70 && BPM <= 90)
            		{
			    	//command to execute for sending BPM data to server
			    	snprintf(BPM_MQTT_cmd, sizeof(BPM_MQTT_cmd), "python3 /bin/MQTT/client.py BPM:%d",BPM);

			    	//Execute the command to send data
			    	ret = system(BPM_MQTT_cmd);

			    	if(ret)
			    	{
			 		printf("system: error status %d\n", ret);
					break;
			    	}
			    	else
			    	{
			    		printf("Sending BPM data to MQTT server\n\n");
			    	}
		    	}   	
         	}
         	if((micros() - timeOutStart) > TIME_OUT)
         	{
         		printf("Program timed out\n");
            		break;
        	}
    	}
}

uint64_t micros()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    uint64_t us = SEC_TO_US((uint64_t)ts.tv_sec) + NS_TO_US((uint64_t)ts.tv_nsec);
    return us;
}

void initPulseSensorVariables(void)
{
    	for (int i = 0; i < 10; ++i)
    	{
        	rate[i] = 0;
    	}
    	QS = 0;
    	BPM = 0;
    	IBI = 600;       	// 600ms per beat = 100 Beats Per Minute (BPM)
    	Pulse = 0;
	sampleCounter = 0;
	lastBeatTime = 0;
	P = 512;           	// peak at 1/2 the input range of 0..1023
	T = 512;            	// trough at 1/2 the input range.
	threshSetting = 550;  	// used to seed and reset the thresh variable
	thresh = 550;     	// threshold a little above the trough
	amp = 100;           	// beat amplitude 1/10 of input range.
	firstBeat = 1;     	// looking for the first beat
	secondBeat = 0;    	// not yet looking for the second beat in a row
	lastTime = micros();
	timeOutStart = lastTime;
}

void startTimer(int latency, unsigned int micros)
{
	signal(SIGALRM, getPulse);

	// generate SIGALRM signal
	// after latency interval and
	// every micros interval
	int err = ualarm(latency, micros);
	if(err == 0)
	{
		if(micros > 0)
		{
	    		printf("ualarm ON\n");
		}
		else
		{
	    		printf("ualarm OFF\n");
		}	
	}
}


void getPulse(int sig_num)
{	
	if(sig_num == SIGALRM)
    	{
        	thisTime = micros();
		Signal = pulse_read(spi_fd);
		elapsedTime = thisTime - lastTime;
		lastTime = thisTime;
		jitter = elapsedTime - OPT_U;
		sumJitter += jitter;
		sampleFlag = 1;

		// keep track of the time in mS with this variable
	  	sampleCounter += 2;
	  	// monitor the time since the last beat to avoid noise
		int N = sampleCounter - lastBeatTime;
		
		//  find the peak and trough of the pulse wave
		// avoid dichrotic noise by waiting 3/5 of last IBI
		if (Signal < thresh && N > (IBI / 5) * 3)
		{
			// T is the trough
			if (Signal < T) 
			{
				// keep track of lowest point in pulse wave
      				T = Signal;
    			}
  		}
  		// thresh condition helps avoid noise
  		if (Signal > thresh && Signal > P)
  		{
  			// P is the peak
    			P = Signal;
  		}
  		
  		//  NOW IT'S TIME TO LOOK FOR THE HEART BEAT
  		// signal surges up in value every time there is a pulse
  		if (N > 250) // avoid high frequency noise
  		{
    			if ( (Signal > thresh) && (Pulse == 0) && (N > ((IBI / 5) * 3)) )
    			{
    				// set the Pulse flag when we think there is a pulse
      				Pulse = 1;
      				// measure time between beats in mS
      				IBI = sampleCounter - lastBeatTime;
      				// keep track of time for next pulse 
		      		lastBeatTime = sampleCounter;
		      		
		      		// if this is the second beat, if secondBeat == 1
		      		if (secondBeat)
		      		{
		      			// clear secondBeat flag
		      			secondBeat = 0;
        				// seed the running total to get a realisitic BPM at startup
        				for (int i = 0; i <= 9; i++)
        				{
          					rate[i] = IBI;
        				}
      				}
      				
      				// if it's the first time we found a beat, if firstBeat == 1
      				if (firstBeat) 
      				{
      					// clear firstBeat flag
      					firstBeat = 0;
      					// set the second beat flag
					secondBeat = 1;
					// IBI value is unreliable so discard it
					return;
      				}
      				
      				// keep a running total of the last 10 IBI values
      				int runningTotal = 0;

				// shift data in the rate array
			      	for (int i = 0; i <= 8; i++) 
			      	{
			      		// and drop the oldest IBI value
					rate[i] = rate[i + 1];
					// add up the 9 oldest IBI values
					runningTotal += rate[i];
			      	}

				// add the latest IBI to the rate array
      				rate[9] = IBI;
      				// add the latest IBI to runningTotal
      				runningTotal += rate[9];
      				// average the last 10 IBI values
      				runningTotal /= 10;
      				// how many beats can fit into a minute? that's BPM!
      				BPM = 60000 / runningTotal;
      				// set Quantified Self flag (we detected a beat)
      				QS = 1;
      
    			}
		}
		
		// when the values are going down, the beat is over
		if (Signal < thresh && Pulse == 1) 
		{
			// reset the Pulse flag so we can do it again
    			Pulse = 0;
    			// get amplitude of the pulse wave
    			amp = P - T;
    			// set thresh at 50% of the amplitude
    			thresh = amp / 2 + T;
    			// reset these for next time
    			P = thresh;
    			T = thresh;
  		}
  		
  		// if 2.5 seconds go by without a beat
  		if (N > 2500) 
  		{
  			// set thresh default
    			thresh = threshSetting;
    			// set P default
		    	P = 512;
		    	// set T default
		    	T = 512;
		    	// bring the lastBeatTime up to date
		    	lastBeatTime = sampleCounter;
		    	// set these to avoid noise
		    	firstBeat = 1;
		    	// when we get the heartbeat back
		    	secondBeat = 0;
		    	QS = 0;
		    	BPM = 0;
		    	// 600ms per beat = 100 Beats Per Minute (BPM)
		    	IBI = 600;
		    	Pulse = 0;
		    	// beat amplitude 1/10 of input range.
		    	amp = 100;
  		}
  		
  		duration = micros()-thisTime;
		
	}
}


