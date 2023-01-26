LDFLAGS=-lrt -lpcap -lwiringPi -lpthread
CPPFLAGS=-Wall -O2 -D _GNU_SOURCE
CFLAGS := $(CFLAGS) 
RELEASE_DIR = ../

all: ruby_i2c

base.o: ../base/base.c
	gcc -c -o $@ $< $(CPPFLAGS) 

alarms.o: ../base/alarms.c
	gcc -c -o $@ $< $(CPPFLAGS) 

config.o: ../base/config.c
	gcc -c -o $@ $< $(CPPFLAGS) 

encr.o: ../base/encr.c
	gcc -c -o $@ $< $(CPPFLAGS) 

hardware_i2c.o: ../base/hardware_i2c.c
	gcc -c -o $@ $< $(CPPFLAGS) 

ctrl_settings.o: ../base/ctrl_settings.c
	gcc -c -o $@ $< $(CPPFLAGS) 

ctrl_interfaces.o: ../base/ctrl_interfaces.cpp
	g++ $(CFLAGS) -c -o $@ $< $(CPPFLAGS)  

gpio.o: ../base/gpio.c
	gcc -c -o $@ $< $(CPPFLAGS) 

hardware.o: ../base/hardware.c
	gcc -c -o $@ $< $(CPPFLAGS) 

hardware_serial.o: ../base/hardware_serial.c
	gcc -c -o $@ $< $(CPPFLAGS) 

hardware_radio.o: ../base/hardware_radio.c
	gcc -c -o $@ $< $(CPPFLAGS) 

hw_procs.o: ../base/hw_procs.c
	gcc -c -o $@ $< $(CPPFLAGS)

ruby_ipc.o: ../base/ruby_ipc.c
	gcc -c -o $@ $< $(CPPFLAGS)

shared_mem.o: ../base/shared_mem.c
	gcc -c -o $@ $< $(CPPFLAGS) 

shared_mem_i2c.o: ../base/shared_mem_i2c.c
	gcc -c -o $@ $< $(CPPFLAGS) 

commands.o: ../base/commands.cpp
	g++ $(CFLAGS) -c -o $@ $< $(CPPFLAGS)

utils.o: ../base/utils.cpp
	g++ $(CFLAGS) -c -o $@ $< $(CPPFLAGS)  

models.o: ../base/models.cpp
	g++ $(CFLAGS) -c -o $@ $< $(CPPFLAGS)  

launchers.o: ../base/launchers.cpp
	g++ $(CFLAGS) -c -o $@ $< $(CPPFLAGS)

string_utils.o: ../common/string_utils.c
	gcc -c -o $@ $< $(CPPFLAGS) 

radiotap.o: ../radio/radiotap.c
	gcc -c -o $@ $< $(CPPFLAGS)

radiolink.o: ../radio/radiolink.c
	gcc -c -o $@ $< $(CPPFLAGS) 

radiopackets2.o: ../radio/radiopackets2.c
	gcc -c -o $@ $< $(CPPFLAGS) 

%.o: %.cpp
	g++ $(CFLAGS) -c -o $@ $< $(CPPFLAGS)  

ruby_i2c: ruby_i2c.o base.o config.o hardware.o gpio.o launchers.o shared_mem.o models.o hw_procs.o utils.o radiotap.o radiolink.o radiopackets2.o shared_mem_i2c.o ctrl_interfaces.o ctrl_settings.o encr.o hardware_i2c.o alarms.o string_utils.o hardware_radio.o ruby_ipc.o hardware_serial.o
	g++ -o $@ $^ $(LDFLAGS)  
	cp -f ruby_i2c $(RELEASE_DIR)
	$(info Copy ruby_i2c done)
	$(info ----------------------------------------------------)

clean:
	rm -f ruby_i2c *.o
