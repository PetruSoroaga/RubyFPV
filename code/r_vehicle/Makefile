LDFLAGS=-lrt -lpcap -lwiringPi -lpthread -ldl 
CPPFLAGS=-Wall -O2 -D _GNU_SOURCE
CFLAGS := $(CFLAGS) 
RELEASE_DIR = ../../

all: ruby_vehicle ruby_rx_commands ruby_tx_telemetry ruby_rx_rc ruby_rt_vehicle

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

gpio.o: ../base/gpio.c
	gcc -c -o $@ $< $(CPPFLAGS) 

hardware.o: ../base/hardware.c
	gcc -c -o $@ $< $(CPPFLAGS) 

hardware_radio.o: ../base/hardware_radio.c
	gcc -c -o $@ $< $(CPPFLAGS)

hardware_radio_sik.o: ../base/hardware_radio_sik.c
	gcc -c -o $@ $< $(CPPFLAGS) 

hardware_serial.o: ../base/hardware_serial.c
	gcc -c -o $@ $< $(CPPFLAGS)

hw_procs.o: ../base/hw_procs.c
	gcc -c -o $@ $< $(CPPFLAGS)

core_plugins_settings.o: ../base/core_plugins_settings.c
	gcc -c -o $@ $< $(CPPFLAGS)   

ruby_ipc.o: ../base/ruby_ipc.c
	gcc -c -o $@ $< $(CPPFLAGS)

shared_mem.o: ../base/shared_mem.c
	gcc -c -o $@ $< $(CPPFLAGS) 

commands.o: ../base/commands.cpp
	g++ $(CFLAGS) -c -o $@ $< $(CPPFLAGS)

models.o: ../base/models.cpp
	g++ $(CFLAGS) -c -o $@ $< $(CPPFLAGS)  

camera_utils.o: ../base/camera_utils.cpp
	g++ $(CFLAGS) -c -o $@ $< $(CPPFLAGS)  

launchers.o: ../base/launchers.cpp
	g++ $(CFLAGS) -c -o $@ $< $(CPPFLAGS)

fec.o: ../radio/fec.c
	gcc -c -o $@ $< $(CPPFLAGS) 

utils.o: ../base/utils.cpp
	g++ $(CFLAGS) -c -o $@ $< $(CPPFLAGS) 

string_utils.o: ../common/string_utils.c
	gcc -c -o $@ $< $(CPPFLAGS)

radio_stats.o: ../common/radio_stats.c
	gcc -c -o $@ $< $(CPPFLAGS)

%.o: %.cpp
	g++ $(CFLAGS) -c -o $@ $< $(CPPFLAGS)  

radiotap.o: ../radio/radiotap.c
	gcc -c -o $@ $< $(CPPFLAGS)

radiolink.o: ../radio/radiolink.c
	gcc -c -o $@ $< $(CPPFLAGS)

radiopackets2.o: ../radio/radiopackets2.c
	gcc -c -o $@ $< $(CPPFLAGS)

radiopackets_rc.o: ../radio/radiopackets_rc.c
	gcc -c -o $@ $< $(CPPFLAGS)

radiopacketsqueue.o: ../radio/radiopacketsqueue.c
	gcc -c -o $@ $< $(CPPFLAGS)

ruby_vehicle: ruby_vehicle.o timers.o shared_mem.o base.o config.o hardware.o models.o gpio.o launchers.o launchers_vehicle.o utils.o hw_procs.o radiotap.o radiolink.o radiopackets2.o radiopackets_rc.o radio_utils.o shared_vars.o encr.o hardware_i2c.o alarms.o string_utils.o hardware_radio.o ruby_ipc.o hardware_serial.o hardware_radio_sik.o
	$(info ----------------------------------------------------)
	$(info Start building ruby_vehicle)
	g++ -o $@ $^ $(LDFLAGS)  
	cp -f ruby_vehicle $(RELEASE_DIR)
	$(info Copy ruby_vehicle done)
	$(info ----------------------------------------------------)

ruby_rx_rc: ruby_rx_rc.o timers.o shared_mem.o base.o config.o hardware.o models.o gpio.o commands.o launchers.o hw_procs.o radiotap.o radiolink.o radiopackets2.o utils.o radiopackets_rc.o encr.o hardware_i2c.o alarms.o string_utils.o hardware_radio.o ruby_ipc.o hardware_serial.o hardware_radio_sik.o
	$(info ----------------------------------------------------)
	$(info Start building ruby_rx_rc)
	g++ -o $@ $^ $(LDFLAGS)  
	cp -f ruby_rx_rc $(RELEASE_DIR) 
	$(info Copy ruby_rx_rc done)
	$(info ----------------------------------------------------)

ruby_rx_commands: ruby_rx_commands.o timers.o shared_mem.o base.o config.o radiotap.o radiolink.o hardware.o models.o gpio.o commands.o launchers.o launchers_vehicle.o hw_procs.o radiopackets2.o utils.o radiopackets_rc.o shared_vars.o encr.o hardware_i2c.o radio_utils.o alarms.o string_utils.o utils_vehicle.o hardware_radio.o process_upload.o ruby_ipc.o core_plugins_settings.o hardware_serial.o hardware_radio_sik.o
	$(info ----------------------------------------------------)
	$(info Start building ruby_rx_commands)
	g++ -o $@ $^ $(LDFLAGS)  
	cp -f ruby_rx_commands $(RELEASE_DIR) 
	$(info Copy ruby_rx_commands done)
	$(info ----------------------------------------------------)

ruby_tx_telemetry: ruby_tx_telemetry.o timers.o shared_mem.o base.o config.o radiotap.o radiolink.o hardware.o launchers.o models.o gpio.o commands.o parse_fc_telemetry.o parse_fc_telemetry_ltm.o hw_procs.o radiopackets2.o launchers_vehicle.o utils.o radiopackets_rc.o shared_vars.o encr.o hardware_i2c.o alarms.o string_utils.o hardware_radio.o ruby_ipc.o hardware_serial.o hardware_radio_sik.o
	$(info ----------------------------------------------------)
	$(info Start building ruby_tx_telemetry)
	g++ -o $@ $^ $(LDFLAGS)  
	cp -f ruby_tx_telemetry $(RELEASE_DIR)
	$(info Copy ruby_tx_telemetry done)
	$(info ----------------------------------------------------)

ruby_rt_vehicle: ruby_rt_vehicle.o timers.o fec.o shared_mem.o base.o config.o hardware.o models.o gpio.o radiotap.o radiolink.o launchers.o hw_procs.o shared_vars.o processor_tx_audio.o processor_tx_video.o radiotap.o radiolink.o radiopackets2.o radiopacketsqueue.o utils.o launchers_vehicle.o process_received_ruby_messages.o radiopackets_rc.o radio_utils.o packets_utils.o encr.o hardware_i2c.o process_local_packets.o alarms.o string_utils.o utils_vehicle.o hardware_radio.o video_link_stats_overwrites.o radio_stats.o commands.o video_link_check_bitrate.o ruby_ipc.o core_plugins_settings.o video_link_auto_keyframe.o camera_utils.o hardware_serial.o relay_rx.o relay_tx.o process_radio_in_packets.o hardware_radio_sik.o
	$(info ----------------------------------------------------)
	$(info Start building ruby_rt_vehicle)
	g++ -o $@ $^ $(LDFLAGS)  
	cp -f ruby_rt_vehicle $(RELEASE_DIR)
	$(info Copy ruby_rt_vehicle done)
	$(info ----------------------------------------------------)

clean:
	rm -f ruby_vehicle ruby_tx_telemetry ruby_rx_commands ruby_rx_rc ruby_rt_vehicle *.o
