/*****************************************************************************
 * Copyright (C) 2023 by Chandana Challa
 *
 * Redistribution, modification or use of this software in source or binary
 * forms is permitted as long as the files maintain this copyright. Users are
 * permitted to modify this and use it to learn about the field of embedded
 * software. Chandana Challa and the University of Colorado are not liable for
 * any misuse of this material.
 *
 *****************************************************************************/
/**
 * @file temp_sensor.c
 * @brief This file contains functionality of temperature sensor application.
 *
 * To compile: make
 *
 * @author Chandana Challa
 * @date Dec 2 2023
 * @version 1.0
 * @resources AESD Course Slides
              https://www.ti.com/product/TMP102
              
 */

/* Header files */
#include <stdio.h>
#include <syslog.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <linux/i2c-dev.h>

/* Macro definitions */
#define TMP102_DEVICE_ADDR   0x48
#define I2C_NODE              1
#define SUCCESS               0
#define FAILURE              -1
#define MAX_BUFF_LEN          5
#define MAX_STR_LEN           15

/* Global definitions */


/* Function Prototypes */
static int init_temp_sensor(uint8_t i2c_node);
static float read_temp_sensor(int file_id);

/* Function definitions */
/**
 * @brief Initializes Temperature sensor by configuring i2c slave address.
 *
 * @param i2c_node
 *
 * @return int
 */
static int init_temp_sensor(uint8_t i2c_node)
{
    char device_path[MAX_STR_LEN] = {0};
    int file_fd = -1;
    snprintf(device_path, MAX_STR_LEN, "/dev/i2c-%d", i2c_node);
    file_fd = open(device_path, O_RDWR);
    if (FAILURE == file_fd)
    {
        syslog(LOG_ERR, "Error opening i2c device %s file: %s", device_path, strerror(errno));
        goto exit;
    }
    
    if (FAILURE == ioctl(file_fd, I2C_SLAVE, TMP102_DEVICE_ADDR))
    {
        syslog(LOG_ERR, "Error setting i2c slave address %s", strerror(errno));
        close(file_fd);
        file_fd = -1;
        goto exit;
    }

exit:
    return file_fd;
}

/**
 * @brief Initializes Temperature sensor by configuring i2c slave address.
 *
 * @param file_id
 *
 * @return float
 */
static float read_temp_sensor(int file_id)
{
    char buffer[MAX_BUFF_LEN] = {0};
    float temperature_value = 0;
    uint8_t buffer_len = 0;
    
    /* Write temp register value */
    buffer[0] = 0x00;
    buffer_len = 1;
    if (buffer_len != write(file_id, buffer, buffer_len))
    {
        syslog(LOG_ERR, "Error writing to i2c device %s", 
               strerror(errno));
        return FAILURE;       
    }
    
    /* Read temperature value */
    buffer_len = 2;
    if (buffer_len != read(file_id, buffer, buffer_len))
    {
        syslog(LOG_ERR, "Error reading from i2c device %s", 
               strerror(errno));
        return FAILURE;     
    }
    /* Read 12-bit temp value */
    temperature_value = ((buffer[0] << 4) | (buffer[1] >> 4));
    /* convert to celsius */
    temperature_value *= 0.0625;
    
    return temperature_value;
}

/**
 * @brief Initializes Temperature sensor by configuring i2c slave address.
 *
 * @param i2c_node
 *
 * @return int
 */
int main(int argc, char *argv[])
{
    uint8_t i2c_node = I2C_NODE;
    int file_id = -1;
    float temperature_value = 0;
    if (argc > 2)
    {
        i2c_node = atoi(argv[1]);
        syslog(LOG_DEBUG, "Configured i2c_node = %d", i2c_node);
    }
    
    file_id = init_temp_sensor(i2c_node);
    if (FAILURE == file_id)
    {
        syslog(LOG_ERR, "Error initializing i2c device");
        return FAILURE;   
    }
    
    while (true)
    {
        temperature_value = read_temp_sensor(file_id);
        if (FAILURE == temperature_value)
        {
            syslog(LOG_ERR, "Error reading temperature value");
            goto exit;
        }
        
        syslog(LOG_DEBUG, "Temperature value = %fC", temperature_value);
        
        usleep(100);
    }

exit:
    close(file_id);
    return 0;
}



