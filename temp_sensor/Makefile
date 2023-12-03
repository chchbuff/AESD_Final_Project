CC ?= $(CROSS_COMPILE)gcc
CFLAGS ?= -Wall -Werror -g 
LDFLAGS ?= 

all: temp_app

temp_app: temp_sensor.o
	$(CC) $^ $(LDFLAGS) -o $@

temp_sensor.o: temp_sensor.c
	$(CC) -c $^ $(CFLAGS) -o $@ 

clean:
	rm -f *.o temp_app

