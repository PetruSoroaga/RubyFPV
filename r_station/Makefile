LDFLAGS=-lrt -lpcap -lwiringPi -lpthread -ldl
CPPFLAGS=-Wall -O2 -D _GNU_SOURCE
CFLAGS := $(CFLAGS)
RELEASE_DIR = ../

all: ruby_controller ruby_rx_telemetry ruby_vehicle_link_ping ruby_tx_rc ruby_rt_station

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

core_plugins_settings.o: ../base/core_plugins_settings.c
	gcc -c -o $@ $< $(CPPFLAGS)  

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

utils.o: ../base/utils.cpp
	g++ $(CFLAGS) -c -o $@ $< $(CPPFLAGS)  

models.o: ../base/models.cpp
	g++ $(CFLAGS) -c -o $@ $< $(CPPFLAGS)  

camera_utils.o: ../base/camera_utils.cpp
	g++ $(CFLAGS) -c -o $@ $< $(CPPFLAGS)  

commands.o: ../base/commands.cpp
	g++ $(CFLAGS) -c -o $@ $< $(CPPFLAGS)  

launchers.o: ../base/launchers.cpp
	g++ $(CFLAGS) -c -o $@ $< $(CPPFLAGS)  

controller_utils.o: ../base/controller_utils.cpp
	g++ $(CFLAGS) -c -o $@ $< $(CPPFLAGS) 

%.o: %.cpp
	g++ $(CFLAGS) -c -o $@ $< $(CPPFLAGS)  

fec.o: ../radio/fec.c
	gcc -c -o $@ $< $(CPPFLAGS)

string_utils.o: ../common/string_utils.c
	gcc -c -o $@ $< $(CPPFLAGS)

radio_stats.o: ../common/radio_stats.c
	gcc -c -o $@ $< $(CPPFLAGS)

radiotap.o: ../radio/radiotap.c
	gcc -c -o $@ $< $(CPPFLAGS)

radiopackets2.o: ../radio/radiopackets2.c
	gcc -c -o $@ $< $(CPPFLAGS) 

radiopackets_rc.o: ../radio/radiopackets_rc.c
	gcc -c -o $@ $< $(CPPFLAGS) 

radiopacketsqueue.o: ../radio/radiopacketsqueue.c
	gcc -c -o $@ $< $(CPPFLAGS) 

radiolink.o: ../radio/radiolink.c
	gcc -c -o $@ $< $(CPPFLAGS) 

ruby_rx_telemetry: ruby_rx_telemetry.o timers.o shared_mem.o base.o config.o launchers.o hardware.o models.o gpio.o ctrl_settings.o ctrl_interfaces.o hw_procs.o radiotap.o radiolink.o radiopackets2.o utils.o radiopackets_rc.o shared_mem_i2c.o encr.o hardware_i2c.o alarms.o string_utils.o hardware_radio.o controller_utils.o ruby_ipc.o hardware_serial.o
	g++ -o $@ $^ $(LDFLAGS)  
	cp -f ruby_rx_telemetry $(RELEASE_DIR)
	$(info Copy ruby_rx_telemetry done)
	$(info ----------------------------------------------------)

ruby_vehicle_link_ping: ruby_vehicle_link_ping.o timers.o shared_mem.o base.o config.o launchers.o hardware.o models.o gpio.o hw_procs.o radiotap.o radiolink.o radiopackets2.o utils.o encr.o hardware_i2c.o alarms.o string_utils.o hardware_radio.o controller_utils.o ruby_ipc.o hardware_serial.o
	g++ -o $@ $^ $(LDFLAGS)  
	cp -f ruby_vehicle_link_ping $(RELEASE_DIR)
	$(info Copy ruby_vehicle_link_ping done)
	$(info ----------------------------------------------------)

ruby_tx_rc: ruby_tx_rc.o timers.o shared_mem.o base.o config.o launchers.o hardware.o models.o gpio.o hw_procs.o radiotap.o radiolink.o radiopackets2.o utils.o radiopackets_rc.o ctrl_settings.o ctrl_interfaces.o shared_mem_i2c.o encr.o hardware_i2c.o alarms.o string_utils.o hardware_radio.o controller_utils.o ruby_ipc.o hardware_serial.o
	g++ -o $@ $^ $(LDFLAGS)  
	cp -f ruby_tx_rc $(RELEASE_DIR)
	$(info Copy ruby_rx_rc done)
	$(info ----------------------------------------------------)

ruby_rt_station: ruby_rt_station.o timers.o fec.o shared_mem.o base.o config.o hardware.o launchers.o models.o gpio.o ctrl_settings.o hw_procs.o processor_rx_audio.o processor_rx_video.o shared_vars.o radiotap.o radiolink.o radiopackets2.o radiopacketsqueue.o ctrl_interfaces.o utils.o radiopackets_rc.o process_radio_in_packets.o packets_utils.o shared_mem_i2c.o encr.o hardware_i2c.o processor_rx_video_forward.o alarms.o links_utils.o string_utils.o radio_stats.o hardware_radio.o controller_utils.o commands.o ruby_ipc.o core_plugins_settings.o video_link_adaptive.o video_link_keyframe.o camera_utils.o hardware_serial.o
	$(info ----------------------------------------------------)
	$(info Start building ruby_rt_station)
	g++ -o $@ $^ $(LDFLAGS)  
	cp -f ruby_rt_station $(RELEASE_DIR)
	$(info Copy ruby_rt_station done)
	$(info ----------------------------------------------------)

ruby_controller: ruby_controller.o base.o config.o hardware.o gpio.o shared_mem.o models.o hw_procs.o  radiotap.o radiolink.o  radiopackets2.o utils.o encr.o hardware_i2c.o alarms.o string_utils.o hardware_radio.o controller_utils.o ruby_ipc.o hardware_serial.o
	g++ -o $@ $^ $(LDFLAGS)  
	cp -f ruby_controller $(RELEASE_DIR)
	$(info Copy ruby_controller done)
	$(info ----------------------------------------------------)

clean:
	rm -f ruby_controller ruby_rx_telemetry ruby_vehicle_link_ping ruby_tx_rc ruby_rt_station *.o
