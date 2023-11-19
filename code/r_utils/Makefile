LDFLAGS=-lrt -lpcap -lwiringPi -lpthread
CPPFLAGS=-Wall -O2 -D _GNU_SOURCE
CFLAGS := $(CFLAGS) 
RELEASE_DIR = ../../

all: ruby_timeinit ruby_update ruby_alive ruby_gpio_detect ruby_sik_config ruby_video_proc ruby_initdhcp ruby_initradio ruby_update_worker test_model_load ruby_logger test_video_cmd

base.o: ../base/base.c
	gcc -c -o $@ $< $(CPPFLAGS) 

alarms.o: ../base/alarms.c
	gcc -c -o $@ $< $(CPPFLAGS) 

config.o: ../base/config.c
	gcc -c -o $@ $< $(CPPFLAGS) 

encr.o: ../base/encr.c
	gcc -c -o $@ $< $(CPPFLAGS) 

vehicle_settings.o: ../base/vehicle_settings.c
	gcc -c -o $@ $< $(CPPFLAGS) 

ctrl_settings.o: ../base/ctrl_settings.c
	gcc -c -o $@ $< $(CPPFLAGS) 

ctrl_preferences.o: ../base/ctrl_preferences.c
	gcc -c -o $@ $< $(CPPFLAGS)

ctrl_interfaces.o: ../base/ctrl_interfaces.cpp
	g++ $(CFLAGS) -c -o $@ $< $(CPPFLAGS)  

gpio.o: ../base/gpio.c
	gcc -c -o $@ $< $(CPPFLAGS) 

hardware_i2c.o: ../base/hardware_i2c.c
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

ruby_ipc.o: ../base/ruby_ipc.c
	gcc -c -o $@ $< $(CPPFLAGS)

shared_mem.o: ../base/shared_mem.c
	gcc -c -o $@ $< $(CPPFLAGS) 

string_utils.o: ../common/string_utils.c
	gcc -c -o $@ $< $(CPPFLAGS)

commands.o: ../base/commands.cpp
	g++ $(CFLAGS) -c -o $@ $< $(CPPFLAGS)

utils.o: ../base/utils.cpp
	g++ $(CFLAGS) -c -o $@ $< $(CPPFLAGS)  

models.o: ../base/models.cpp
	g++ $(CFLAGS) -c -o $@ $< $(CPPFLAGS)  

models_list.o: ../base/models_list.cpp
	g++ $(CFLAGS) -c -o $@ $< $(CPPFLAGS)  

relay_utils.o: ../common/relay_utils.cpp
	g++ $(CFLAGS) -c -o $@ $< $(CPPFLAGS)  

radio_utils.o: ../base/radio_utils.cpp
	g++ $(CFLAGS) -c -o $@ $< $(CPPFLAGS)

radio_stats.o: ../common/radio_stats.c
	gcc -c -o $@ $< $(CPPFLAGS)

radio_rx.o: ../radio/radio_rx.c
	gcc -c -o $@ $< $(CPPFLAGS)

radio_tx.o: ../radio/radio_tx.c
	gcc -c -o $@ $< $(CPPFLAGS)

radio_duplicate_det.o: ../radio/radio_duplicate_det.c
	gcc -c -o $@ $< $(CPPFLAGS)

controller_utils.o: ../base/controller_utils.cpp
	g++ $(CFLAGS) -c -o $@ $< $(CPPFLAGS)

%.o: %.cpp
	g++ $(CFLAGS) -c -o $@ $< $(CPPFLAGS)  

fec.o: ../radio/fec.c
	gcc -c -o $@ $< $(CPPFLAGS)  

radiotap.o: ../radio/radiotap.c
	gcc -c -o $@ $< $(CPPFLAGS)

radiolink.o: ../radio/radiolink.c
	gcc -c -o $@ $< $(CPPFLAGS) 

radiopackets2.o: ../radio/radiopackets2.c
	gcc -c -o $@ $< $(CPPFLAGS) 

radiopackets_short.o: ../radio/radiopackets_short.c
	gcc -c -o $@ $< $(CPPFLAGS) 

ruby_timeinit: ruby_timeinit.o base.o config.o hardware.o gpio.o radio_utils.o shared_mem.o models.o models_list.o hw_procs.o radiotap.o radiolink.o radiopackets2.o radiopackets_short.o utils.o encr.o hardware_i2c.o alarms.o string_utils.o hardware_radio.o controller_utils.o ruby_ipc.o hardware_serial.o hardware_radio_sik.o radio_rx.o radio_tx.o radio_duplicate_det.o radio_stats.o ctrl_preferences.o ctrl_interfaces.o
	g++ -o $@ $^ $(LDFLAGS)  
	cp -f ruby_timeinit $(RELEASE_DIR)
	$(info Copy ruby_timeinit done)
	$(info ----------------------------------------------------)

ruby_logger: ruby_logger.o base.o config.o hardware.o gpio.o radio_utils.o shared_mem.o models.o models_list.o hw_procs.o radiotap.o radiolink.o radiopackets2.o utils.o encr.o hardware_i2c.o alarms.o string_utils.o hardware_radio.o controller_utils.o ruby_ipc.o hardware_serial.o hardware_radio_sik.o radiopackets_short.o radio_rx.o radio_tx.o radio_duplicate_det.o radio_stats.o ctrl_preferences.o ctrl_interfaces.o
	g++ -o $@ $^ $(LDFLAGS)  
	cp -f ruby_logger $(RELEASE_DIR)
	$(info Copy ruby_logger done)
	$(info ----------------------------------------------------)

ruby_initdhcp: ruby_initdhcp.o base.o config.o hardware.o gpio.o radio_utils.o shared_mem.o models.o models_list.o hw_procs.o radiotap.o radiolink.o  radiopackets2.o utils.o ctrl_settings.o encr.o hardware_i2c.o alarms.o string_utils.o hardware_radio.o controller_utils.o ruby_ipc.o hardware_serial.o hardware_radio_sik.o radiopackets_short.o radio_rx.o radio_tx.o radio_duplicate_det.o radio_stats.o ctrl_preferences.o ctrl_interfaces.o
	g++ -o $@ $^ $(LDFLAGS)  
	cp -f ruby_initdhcp $(RELEASE_DIR)
	$(info Copy ruby_initdhcp done)
	$(info ----------------------------------------------------)

ruby_initradio: ruby_initradio.o base.o config.o hardware.o gpio.o radio_utils.o shared_mem.o models.o models_list.o ctrl_interfaces.o ctrl_settings.o hw_procs.o radiotap.o radiolink.o radiopackets2.o utils.o encr.o hardware_i2c.o alarms.o string_utils.o hardware_radio.o controller_utils.o ruby_ipc.o hardware_serial.o hardware_radio_sik.o radiopackets_short.o radio_stats.o radio_rx.o radio_tx.o radio_duplicate_det.o ctrl_preferences.o
	g++ -o $@ $^ $(LDFLAGS)  
	cp -f ruby_initradio $(RELEASE_DIR)
	$(info Copy ruby_initradio done)
	$(info ----------------------------------------------------)

ruby_sik_config: ruby_sik_config.o base.o config.o hardware.o gpio.o radio_utils.o shared_mem.o models.o models_list.o ctrl_interfaces.o ctrl_settings.o hw_procs.o radiotap.o radiolink.o radiopackets2.o utils.o encr.o hardware_i2c.o alarms.o string_utils.o hardware_radio.o controller_utils.o ruby_ipc.o hardware_serial.o hardware_radio_sik.o radiopackets_short.o radio_rx.o radio_tx.o radio_duplicate_det.o radio_stats.o ctrl_preferences.o
	g++ -o $@ $^ $(LDFLAGS)  
	cp -f ruby_sik_config $(RELEASE_DIR)
	$(info Copy ruby_sik_config done)
	$(info ----------------------------------------------------)

ruby_alive: ruby_alive.o base.o config.o hardware.o gpio.o shared_mem.o models.o models_list.o hw_procs.o  radiotap.o radiolink.o  radiopackets2.o utils.o encr.o hardware_i2c.o alarms.o string_utils.o hardware_radio.o controller_utils.o ruby_ipc.o hardware_serial.o hardware_radio_sik.o radiopackets_short.o ctrl_preferences.o ctrl_interfaces.o
	g++ -o $@ $^ $(LDFLAGS)  
	cp -f ruby_alive $(RELEASE_DIR)
	$(info Copy ruby_alive done)
	$(info ----------------------------------------------------)

ruby_video_proc: ruby_video_proc.o base.o config.o hardware.o gpio.o radio_utils.o shared_mem.o models.o models_list.o hw_procs.o  radiotap.o radiolink.o  radiopackets2.o utils.o encr.o hardware_i2c.o alarms.o string_utils.o hardware_radio.o controller_utils.o ruby_ipc.o hardware_serial.o hardware_radio_sik.o radiopackets_short.o radio_rx.o radio_tx.o radio_duplicate_det.o radio_stats.o ctrl_preferences.o ctrl_interfaces.o
	g++ -o $@ $^ $(LDFLAGS)  
	cp -f ruby_video_proc $(RELEASE_DIR)
	$(info Copy ruby_video_proc done)
	$(info ----------------------------------------------------)

ruby_gpio_detect: ruby_gpio_detect.o base.o config.o hardware.o gpio.o radio_utils.o shared_mem.o models.o models_list.o hw_procs.o  radiotap.o radiolink.o  radiopackets2.o utils.o encr.o hardware_i2c.o alarms.o string_utils.o hardware_radio.o controller_utils.o ruby_ipc.o hardware_serial.o hardware_radio_sik.o radiopackets_short.o radio_rx.o radio_tx.o radio_duplicate_det.o radio_stats.o ctrl_preferences.o ctrl_interfaces.o
	g++ -o $@ $^ $(LDFLAGS)  
	cp -f ruby_gpio_detect $(RELEASE_DIR)
	$(info Copy ruby_gpio_detect done)
	$(info ----------------------------------------------------)

ruby_update: ruby_update.o base.o config.o hardware.o gpio.o radio_utils.o shared_mem.o models.o models_list.o hw_procs.o ctrl_settings.o radiotap.o radiolink.o  radiopackets2.o utils.o encr.o hardware_i2c.o ctrl_interfaces.o alarms.o string_utils.o hardware_radio.o controller_utils.o ruby_ipc.o hardware_serial.o hardware_radio_sik.o radiopackets_short.o vehicle_settings.o radio_rx.o radio_tx.o radio_duplicate_det.o radio_stats.o ctrl_preferences.o ctrl_interfaces.o
	g++ -o $@ $^ $(LDFLAGS)  
	cp -f ruby_update $(RELEASE_DIR)
	$(info Copy ruby_update done)
	$(info ----------------------------------------------------)

ruby_update_worker: ruby_update_worker.o base.o config.o hardware.o gpio.o radio_utils.o shared_mem.o models.o models_list.o hw_procs.o ctrl_settings.o radiotap.o radiolink.o  radiopackets2.o utils.o encr.o hardware_i2c.o alarms.o string_utils.o hardware_radio.o controller_utils.o ruby_ipc.o hardware_serial.o hardware_radio_sik.o radiopackets_short.o radio_rx.o radio_tx.o radio_duplicate_det.o radio_stats.o ctrl_preferences.o ctrl_interfaces.o
	g++ -o $@ $^ $(LDFLAGS)  
	cp -f ruby_update_worker $(RELEASE_DIR)
	$(info Copy ruby_update_worker done)
	$(info ----------------------------------------------------)

test_model_load: test_model_load.o base.o config.o hardware.o gpio.o radio_utils.o shared_mem.o models.o models_list.o ctrl_interfaces.o ctrl_settings.o hw_procs.o radiotap.o radiolink.o radiopackets2.o utils.o encr.o hardware_i2c.o alarms.o string_utils.o hardware_radio.o controller_utils.o ruby_ipc.o hardware_serial.o hardware_radio_sik.o radiopackets_short.o radio_rx.o radio_tx.o radio_duplicate_det.o radio_stats.o ctrl_preferences.o ctrl_interfaces.o
	g++ -o $@ $^ $(LDFLAGS)  
	cp -f test_model_load $(RELEASE_DIR)
	$(info Copy test_model_load done)
	$(info ----------------------------------------------------)

test_video_cmd: test_video_cmd.o
	g++ -o $@ $^ $(LDFLAGS)  
	cp -f test_video_cmd $(RELEASE_DIR)
	$(info Copy test_video_cmd done)
	$(info ----------------------------------------------------)

clean:
	rm -f ruby_timeinit ruby_logger ruby_gpio_detect ruby_update ruby_sik_config ruby_alive ruby_video_proc ruby_initdhcp ruby_initradio ruby_update_worker test_model_load test_video_cmd *.o
