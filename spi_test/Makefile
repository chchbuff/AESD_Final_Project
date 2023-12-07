######################## Makefile ###########################

######################## Sources ############################
SRCS = ./spidev_test.c


######################## Flags ##############################
CC ?= $(CROSS_COMPILE)gcc
CFLAGS ?= -Wall -Werror -g
LDFLAGS ?= 

######################## Targets ############################
all: spidev_test

spidev_test: $(SRCS)
	$(CC) $(SRCS) $(CFLAGS) $(LDFLAGS) -o spidev_test


######################## Clean ##############################
clean:
	rm -rf spidev_test
