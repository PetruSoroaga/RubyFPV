CPPFLAGS+= -O2 -I../openvg -I/opt/vc/include/ -I/opt/vc/include/interface/vcos/pthreads -I/opt/vc/include/interface/vmcs_host/linux -I/usr/include/freetype2
LDFLAGS+= -lfreetype -lz -lpthread -lpcap -lwiringPi -lpng -ljpeg -lz -li2c
LDFLAGS+= -L/usr/lib/arm-linux-gnueabihf -L../openvg -L/opt/vc/lib/ -lbrcmGLESv2 -lbrcmEGL -lopenmaxil -lbcm_host -lvcos -lvchiq_arm -lpthread -lrt -lm
CFLAGS := $(CFLAGS) 
RELEASE_DIR = ../../


all: test_port_rx test_udp_server test_udp_client test_serial_link

base.o: ../base/base.c
	gcc -c -o $@ $< $(CPPFLAGS)

alarms.o: ../base/alarms.c
	gcc -c -o $@ $< $(CPPFLAGS) 

launchers.o: ../base/launchers.cpp
	g++ $(CFLAGS) -c -o $@ $< $(CPPFLAGS)

config.o: ../base/config.c
	gcc -c -o $@ $< $(CPPFLAGS)

encr.o: ../base/encr.c
	gcc -c -o $@ $< $(CPPFLAGS)

gpio.o: ../base/gpio.c
	gcc -c -o $@ $< $(CPPFLAGS)

hardware.o: ../base/hardware.c
	gcc -c -o $@ $< $(CPPFLAGS)

hardware_serial.o: ../base/hardware_serial.c
	gcc -c -o $@ $< $(CPPFLAGS)

hardware_radio.o: ../base/hardware_radio.c
	gcc -c -o $@ $< $(CPPFLAGS) 

hardware_radio_sik.o: ../base/hardware_radio_sik.c
	gcc -c -o $@ $< $(CPPFLAGS) 

hardware_i2c.o: ../base/hardware_i2c.c
	gcc -c -o $@ $< $(CPPFLAGS)  

string_utils.o: ../common/string_utils.c
	gcc -c -o $@ $< $(CPPFLAGS)

render_engine.o: ../renderer/render_engine.cpp
	g++ $(CFLAGS) -c -o $@ $< $(CPPFLAGS)

render_engine_ovg.o: ../renderer/render_engine_ovg.cpp
	g++ $(CFLAGS) -c -o $@ $< $(CPPFLAGS)

render_engine_raw.o: ../renderer/render_engine_raw.cpp
	g++ $(CFLAGS) -c -o $@ $< $(CPPFLAGS)

hw_procs.o: ../base/hw_procs.c
	gcc -c -o $@ $< $(CPPFLAGS)

ruby_ipc.o: ../base/ruby_ipc.c
	gcc -c -o $@ $< $(CPPFLAGS) 

shared_mem.o: ../base/shared_mem.c
	gcc -c -o $@ $< $(CPPFLAGS)

utils.o: ../base/utils.cpp
	g++ $(CFLAGS) -c -o $@ $< $(CPPFLAGS)  

radio_stats.o: ../common/radio_stats.c
	gcc -c -o $@ $< $(CPPFLAGS) 

models.o: ../base/models.cpp
	g++ $(CFLAGS) -c -o $@ $< $(CPPFLAGS)  

commands.o: ../base/commands.cpp
	g++ $(CFLAGS) -c -o $@ $< $(CPPFLAGS)  

%.o: %.cpp
	g++ $(CFLAGS) -c -o $@ $< $(CPPFLAGS)  

fec.o: ../radio/fec.c
	gcc -c -o $@ $< $(CPPFLAGS)  

radiotap.o: ../radio/radiotap.c
	gcc -c -o $@ $< $(CPPFLAGS)

radiopackets2.o: ../radio/radiopackets2.c
	gcc -c -o $@ $< $(CPPFLAGS)

radiolink.o: ../radio/radiolink.c
	gcc -c -o $@ $< $(CPPFLAGS)

test_ruby_vehicle_ping: test_ruby_vehicle_ping.o shared_mem.o base.o config.o radiotap.o radiolink.o hardware.o models.o gpio.o commands.o launchers.o hw_procs.o radiopackets2.o utils.o hardware_serial.o
	g++ -o $@ $^ $(LDFLAGS)  

test_link_speed: test_link_speed.o shared_mem.o base.o config.o radiotap.o radiolink.o hardware.o models.o gpio.o commands.o launchers.o hw_procs.o radiopackets2.o utils.o encr.o  hardware_serial.o
	g++ -o $@ $^ $(LDFLAGS)  
	cp -f test_link_speed $(RELEASE_DIR) 

test_port_rx: test_port_rx.o shared_mem.o base.o config.o radiotap.o radiolink.o hardware.o models.o gpio.o commands.o launchers.o hw_procs.o radiopackets2.o utils.o encr.o encr.o alarms.o ruby_ipc.o hardware_radio.o string_utils.o radio_stats.o hardware_i2c.o hardware_serial.o hardware_radio_sik.o
	g++ -o $@ $^ $(LDFLAGS)  
	cp -f test_port_rx $(RELEASE_DIR) 

test_port_tx: test_port_tx.o shared_mem.o base.o config.o radiotap.o radiolink.o hardware.o models.o gpio.o commands.o launchers.o hw_procs.o radiopackets2.o utils.o encr.o encr.o alarms.o hardware_serial.o
	g++ -o $@ $^ $(LDFLAGS)  
	cp -f test_port_tx $(RELEASE_DIR) 

test_video_rx: test_video_rx.o shared_mem.o base.o config.o radiotap.o radiolink.o hardware.o models.o gpio.o commands.o launchers.o hw_procs.o radiopackets2.o utils.o hardware_serial.o
	g++ -o $@ $^ $(LDFLAGS)  
	cp -f test_video_rx $(RELEASE_DIR) 

test_log: test_log.o shared_mem.o base.o config.o radiotap.o radiolink.o hardware.o models.o gpio.o commands.o launchers.o hw_procs.o radiopackets2.o utils.o
	g++ -o $@ $^ $(LDFLAGS)  

test_camera: test_camera.o shared_mem.o base.o config.o radiotap.o radiolink.o hardware.o models.o gpio.o commands.o launchers.o hw_procs.o radiopackets2.o utils.o
	g++ -o $@ $^ $(LDFLAGS)  
	cp -f test_camera $(RELEASE_DIR) 

test_joystick: test_joystick.o shared_mem.o base.o config.o radiotap.o radiolink.o hardware.o gpio.o commands.o launchers.o hw_procs.o radiopackets2.o
	g++ -o $@ $^ $(LDFLAGS)  
	cp -f test_joystick $(RELEASE_DIR) 

test_i2c: test_i2c.o shared_mem.o base.o config.o radiotap.o radiolink.o hardware.o models.o gpio.o commands.o launchers.o hw_procs.o radiopackets2.o utils.o
	g++ -o $@ $^ $(LDFLAGS)  
	cp -f test_i2c $(RELEASE_DIR) 

test_socket_in: test_socket_in.o shared_mem.o base.o config.o radiotap.o radiolink.o hardware.o models.o gpio.o commands.o launchers.o hw_procs.o radiopackets2.o utils.o
	g++ -o $@ $^ $(LDFLAGS)  
	cp -f test_socket_in $(RELEASE_DIR) 

test_socket_out: test_socket_out.o shared_mem.o base.o config.o radiotap.o radiolink.o hardware.o models.o gpio.o commands.o launchers.o hw_procs.o radiopackets2.o utils.o
	g++ -o $@ $^ $(LDFLAGS)  
	cp -f test_socket_out $(RELEASE_DIR) 

test_serial_read: test_serial_read.o shared_mem.o base.o config.o radiotap.o radiolink.o hardware.o models.o gpio.o commands.o launchers.o hw_procs.o radiopackets2.o utils.o
	g++ -o $@ $^ $(LDFLAGS)  
	cp -f test_serial_read $(RELEASE_DIR) 

test_ui: test_ui.o shared_mem.o base.o config.o radiotap.o radiolink.o hardware.o models.o gpio.o commands.o launchers.o hw_procs.o radiopackets2.o utils.o render_engine.o render_engine_ovg.o render_engine_raw.o fontsystem.o
	g++ -o $@ $^ $(LDFLAGS)  
	cp -f test_ui $(RELEASE_DIR) 

test_udp_client: test_udp_client.o
	g++ -o $@ $^ $(LDFLAGS)  
	cp -f test_udp_client $(RELEASE_DIR) 

test_udp_server: test_udp_server.o
	g++ -o $@ $^ $(LDFLAGS)  
	cp -f test_udp_server $(RELEASE_DIR) 

test_fec: test_fec.o base.o fec.o config.o hardware.o gpio.o launchers.o shared_mem.o models.o radiotap.o radiolink.o hw_procs.o radiopackets2.o utils.o encr.o
	g++ -o $@ $^ $(LDFLAGS)   
	cp -f test_fec $(RELEASE_DIR) 

test_wiringpi_spi: test_wiringpi_spi.o
	g++ -o $@ $^ $(LDFLAGS)   
	cp -f test_wiringpi_spi $(RELEASE_DIR) 

test_serial_link: test_serial_link.o shared_mem.o base.o config.o radiotap.o radiolink.o hardware.o models.o gpio.o commands.o launchers.o hw_procs.o radiopackets2.o utils.o encr.o encr.o alarms.o ruby_ipc.o hardware_radio.o string_utils.o radio_stats.o hardware_i2c.o hardware_serial.o hardware_radio_sik.o
	g++ -o $@ $^ $(LDFLAGS)  
	cp -f test_serial_link $(RELEASE_DIR) 

clean:
	rm -f test_wiringpi_spi test_serial_link test_link_speed test_udp_client test_udp_server test_ruby_vehicle_ping test_port_rx test_port_tx test_log test_camera test_video_rx test_joystick test_i2c test_socket_in test_socket_out test_serial_read test_ui test_fec *.o
