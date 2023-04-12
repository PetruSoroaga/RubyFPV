LDFLAGS=-lrt -lpcap -lwiringPi -lpthread
CPPFLAGS=-Wall -O0 -D _GNU_SOURCE
CFLAGS := $(CFLAGS) 
RELEASE_DIR = ../../

all: ruby_start

base.o: ../base/base.c
	gcc -c -o $@ $< $(CPPFLAGS)

alarms.o: ../base/alarms.c
	gcc -c -o $@ $< $(CPPFLAGS) 

config.o: ../base/config.c
	gcc -c -o $@ $< $(CPPFLAGS)

encr.o: ../base/encr.c
	gcc -c -o $@ $< $(CPPFLAGS)

ctrl_settings.o: ../base/ctrl_settings.c
	gcc -c -o $@ $< $(CPPFLAGS)

gpio.o: ../base/gpio.c
	gcc -c -o $@ $< $(CPPFLAGS)

hardware.o: ../base/hardware.c
	gcc -c -o $@ $< $(CPPFLAGS)

hardware_i2c.o: ../base/hardware_i2c.c
	gcc -c -o $@ $< $(CPPFLAGS)

hardware_radio.o: ../base/hardware_radio.c
	gcc -c -o $@ $< $(CPPFLAGS)

hardware_radio_sik.o: ../base/hardware_radio_sik.c
	gcc -c -o $@ $< $(CPPFLAGS) 

hardware_serial.o: ../base/hardware_serial.c
	gcc -c -o $@ $< $(CPPFLAGS)

hw_procs.o: ../base/hw_procs.c
	gcc -c -o $@ $< $(CPPFLAGS)

ruby_ipc.o: ../base/ruby_ipc.c
	gcc -c -o $@ $< $(CPPFLAGS)

shared_mem.o: ../base/shared_mem.c
	gcc -c -o $@ $< $(CPPFLAGS)

string_utils.o: ../common/string_utils.c
	gcc -c -o $@ $< $(CPPFLAGS)

commands.o: ../base/commands.cpp
	g++ $(CFLAGS) -c -o $@ $< $(CPPFLAGS)

models.o: ../base/models.cpp
	g++ $(CFLAGS) -c -o $@ $< $(CPPFLAGS)  

launchers.o: ../base/launchers.cpp
	g++ $(CFLAGS) -c -o $@ $< $(CPPFLAGS)

fec.o: fec.c
	gcc -c -o $@ $< $(CPPFLAGS) 

utils.o: ../base/utils.cpp
	g++ $(CFLAGS) -c -o $@ $< $(CPPFLAGS) 

controller_utils.o: ../base/controller_utils.cpp
	g++ $(CFLAGS) -c -o $@ $< $(CPPFLAGS) 

%.o: %.cpp
	g++ $(CFLAGS) -c -o $@ $< $(CPPFLAGS)  

radiotap.o: ../radio/radiotap.c
	gcc -c -o $@ $< $(CPPFLAGS)

radiolink.o: ../radio/radiolink.c
	gcc -c -o $@ $< $(CPPFLAGS)

radiopackets2.o: ../radio/radiopackets2.c
	gcc -c -o $@ $< $(CPPFLAGS)

ruby_start: ruby_start.o shared_mem.o base.o config.o hardware.o hw_procs.o models.o gpio.o launchers.o radiotap.o radiolink.o radiopackets2.o ctrl_settings.o utils.o encr.o hardware_i2c.o alarms.o hw_config_check.o string_utils.o hardware_radio.o controller_utils.o ruby_ipc.o hardware_serial.o hardware_radio_sik.o
	g++ -o $@ $^ $(LDFLAGS)
	cp -f ruby_start $(RELEASE_DIR)
	$(info Copy ruby_start done)
	$(info ----------------------------------------------------)

clean:
	rm -f ruby_start *.o
