######################## Makefile ###########################

######################## Sources ############################
SRCS = ./pulse_sensor.c


######################## Flags ##############################
CC ?= $(CROSS_COMPILE)gcc
CFLAGS ?= -Wall -Werror -g
LDFLAGS ?= 

######################## Targets ############################
all: pulse_app

pulse_app: $(SRCS)
	$(CC) $(SRCS) $(CFLAGS) $(LDFLAGS) -o pulse_app


######################## Clean ##############################
clean:
	rm -rf pulse_app
