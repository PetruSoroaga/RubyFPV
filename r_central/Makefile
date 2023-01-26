CPPFLAGS+= -Wl,--export-dynamic -O2 -I../openvg -I/opt/vc/include/ -I/opt/vc/include/interface/vcos/pthreads -I/opt/vc/include/interface/vmcs_host/linux -I/usr/include/freetype2
LDFLAGS+= -lfreetype -lz -lpthread -lpcap -lwiringPi -lpng -ljpeg -lz -ldl
LDFLAGS+= -L/usr/lib/arm-linux-gnueabihf -L../openvg -L/opt/vc/lib/ -lbrcmGLESv2 -lbrcmEGL -lopenmaxil -lbcm_host -lvcos -lvchiq_arm -lpthread -lrt -lm
LDFLAGS2 = $(LDFLAGS) -L/opt/vc/lib/ -lbrcmGLESv2 -lbrcmEGL -lopenmaxil -lbcm_host -lvcos -lvchiq_arm -lpthread -lrt -lm -lopenmaxil -lbcm_host -lvcos -lvchiq_arm -lmmal  -lmmal_core -lmmal_util -lmmal_vc_client 
CFLAGS := $(CFLAGS) 
RELEASE_DIR = ../

MENU_ITEMS_ALL := menu_items.o menu_item_select_base.o menu_item_select.o menu_item_slider.o menu_item_range.o menu_item_edit.o menu_item_section.o menu_item_text.o menu_item_legend.o menu_item_checkbox.o menu_item_radio.o
MENU_ALL := menu.o menu_objects.o menu_preferences_buttons.o menu_root.o menu_search.o menu_spectator.o menu_vehicles.o menu_vehicle.o menu_vehicle_general.o menu_vehicle_expert.o menu_vehicle_video.o menu_vehicle_camera.o menu_vehicle_osd.o menu_confirmation.o menu_storage.o menu_vehicle_telemetry.o menu_vehicle_video_encodings.o menu_text.o menu_txinfo.o menu_calibrate_hdmi.o
MENU_ALL2 := menu_vehicle_relay.o menu_controller.o menu_vehicle_alarms.o menu_vehicle_camera_gains.o menu_controller_peripherals.o menu_controller_expert.o menu_controller_radio_interfaces.o menu_system.o menu_vehicle_osd_instruments.o menu_preferences_ui.o menu_system_expert.o menu_vehicle_video_profile.o menu_confirmation_import.o menu_system_alarms.o
MENU_ALL3 := menu_vehicle_management.o menu_vehicle_import.o menu_switch_vehicle.o menu_controller_joystick.o menu_system_all_params.o menu_color_picker.o menu_controller_video.o menu_controller_telemetry.o menu_update_vehicle.o menu_device_i2c.o menu_vehicle_osd_stats.o menu_vehicle_audio.o menu_channels_select.o menu_tx_power_max.o menu_system_dev_logs.o menu_item_vehicle.o menu_radio_config.o
MENU_ALL4 := menu_vehicle_data_link.o menu_controller_network.o menu_vehicle_osd_plugins.o menu_vehicle_instruments_general.o menu_vehicle_osd_elements.o menu_vehicle_osd_plugin.o menu_controller_plugins.o menu_controller_encryption.o menu_search_connect.o menu_system_hardware.o menu_confirmation_hdmi.o menu_controller_recording.o menu_system_video_profiles.o menu_info_booster.o menu_controller_radio_interface.o
MENU_ALL5 := menu_system_dev_stats.o menu_vehicle_radio_link.o menu_vehicle_radio_interface.o menu_vehicle_management_plugins.o menu_vehicle_peripherals.o
MENU_RC := menu_vehicle_rc.o menu_vehicle_rc_failsafe.o menu_vehicle_rc_channels.o menu_vehicle_rc_expo.o menu_vehicle_rc_camera.o menu_vehicle_rc_input.o menu_vehicle_functions.o

POPUP_ALL := popup.o popup_log.o popup_commands.o popup_camera_params.o
RENDER_ALL := colors.o render_commands.o render_joysticks.o process_router_messages.o render_engine.o render_engine_raw.o render_engine_ui.o
RENDER_RAW := lodepng.o nanojpeg.o fbgraphics.o dispmanx.o
OSD_ALL := osd_common.o osd.o osd_stats.o osd_ahi.o osd_lean.o osd_warnings.o osd_gauges.o osd_plugins.o osd_stats_dev.o osd_links.o
BASE_ALL := models.o gpio.o base.o hardware.o hw_procs.o launchers.o config.o shared_mem.o commands.o ctrl_settings.o ctrl_interfaces.o utils.o plugins_settings.o encr.o hardware_i2c.o hdmi.o alarms.o config_video.o
CENTRAL_ALL := events.o shared_vars_ipc.o shared_vars_state.o

all: ruby_central

render_engine.o: ../renderer/render_engine.cpp
	g++ $(CFLAGS) -c -o $@ $< $(CPPFLAGS)

render_engine_raw.o: ../renderer/render_engine_raw.cpp
	g++ $(CFLAGS) -c -o $@ $< $(CPPFLAGS)

render_engine_ui.o: ../renderer/render_engine_ui.cpp
	g++ $(CFLAGS) -c -o $@ $< $(CPPFLAGS)

lodepng.o: ../renderer/lodepng.c
	gcc -c -o $@ $< $(CPPFLAGS)

nanojpeg.o: ../renderer/nanojpeg.c
	gcc -c -o $@ $< $(CPPFLAGS)

fbgraphics.o: ../renderer/fbgraphics.c
	gcc -c -o $@ $< $(CPPFLAGS)

dispmanx.o: ../renderer/fbg_dispmanx.c
	gcc -c -o $@ $< $(CPPFLAGS) 

base.o: ../base/base.c
	gcc -c -o $@ $< $(CPPFLAGS)

alarms.o: ../base/alarms.c
	gcc -c -o $@ $< $(CPPFLAGS)

gpio.o: ../base/gpio.c
	gcc -c -o $@ $< $(CPPFLAGS) 

hardware_i2c.o: ../base/hardware_i2c.c
	gcc -c -o $@ $< $(CPPFLAGS)

hardware.o: ../base/hardware.c
	gcc -c -o $@ $< $(CPPFLAGS)

hardware_serial.o: ../base/hardware_serial.c
	gcc -c -o $@ $< $(CPPFLAGS)

hardware_radio.o: ../base/hardware_radio.c
	gcc -c -o $@ $< $(CPPFLAGS)

hw_procs.o: ../base/hw_procs.c
	gcc -c -o $@ $< $(CPPFLAGS)

hdmi.o: ../base/hdmi.c
	gcc -c -o $@ $< $(CPPFLAGS)

ruby_ipc.o: ../base/ruby_ipc.c
	gcc -c -o $@ $< $(CPPFLAGS)

shared_mem.o: ../base/shared_mem.c
	gcc -c -o $@ $< $(CPPFLAGS)

shared_mem_i2c.o: ../base/shared_mem_i2c.c
	gcc -c -o $@ $< $(CPPFLAGS)

config.o: ../base/config.c
	gcc -c -o $@ $< $(CPPFLAGS)

encr.o: ../base/encr.c
	gcc -c -o $@ $< $(CPPFLAGS)

ctrl_settings.o: ../base/ctrl_settings.c
	gcc -c -o $@ $< $(CPPFLAGS)

ctrl_interfaces.o: ../base/ctrl_interfaces.cpp
	g++ $(CFLAGS) -c -o $@ $< $(CPPFLAGS)  

core_plugins_settings.o: ../base/core_plugins_settings.c
	gcc -c -o $@ $< $(CPPFLAGS)   

config_video.o: ../base/config_video.cpp
	g++ $(CFLAGS) -c -o $@ $< $(CPPFLAGS)  

plugins_settings.o: ../base/plugins_settings.cpp
	g++ $(CFLAGS) -c -o $@ $< $(CPPFLAGS) 

commands.o: ../base/commands.cpp
	g++ $(CFLAGS) -c -o $@ $< $(CPPFLAGS) 

models.o: ../base/models.cpp
	g++ $(CFLAGS) -c -o $@ $< $(CPPFLAGS) 

launchers.o: ../base/launchers.cpp
	g++ $(CFLAGS) -c -o $@ $< $(CPPFLAGS) 

string_utils.o: ../common/string_utils.c
	gcc -c -o $@ $< $(CPPFLAGS)

radio_stats.o: ../common/radio_stats.c
	gcc -c -o $@ $< $(CPPFLAGS) 

radiolink.o: ../radio/radiolink.c
	gcc -c -o $@ $< $(CPPFLAGS)

radiopackets2.o: ../radio/radiopackets2.c
	gcc -c -o $@ $< $(CPPFLAGS)

radiopackets_rc.o: ../radio/radiopackets_rc.c
	gcc -c -o $@ $< $(CPPFLAGS)

radiotap.o: ../radio/radiotap.c
	gcc -c -o $@ $< $(CPPFLAGS)

utils.o: ../base/utils.cpp
	g++ $(CFLAGS) -c -o $@ $< $(CPPFLAGS) 

controller_utils.o: ../base/controller_utils.cpp
	g++ $(CFLAGS) -c -o $@ $< $(CPPFLAGS) 

%.o: %.cpp
	g++ $(CFLAGS) -c -o $@ $< $(CPPFLAGS) 

chars: chars.o
	g++ -o $@ $^ $(LDFLAGS) 

ruby_central: $(BASE_ALL) $(CENTRAL_ALL) $(OSD_ALL) $(MENU_ITEMS_ALL) $(MENU_ALL) $(MENU_ALL2) $(MENU_ALL3) $(MENU_ALL4) $(MENU_ALL5) $(MENU_RC) $(POPUP_ALL) $(RENDER_ALL) $(RENDER_RAW) ruby_central.o media.o shared_vars.o pairing.o radiolink.o radiotap.o link_watch.o warnings.o handle_commands.o alarms.o notifications.o launchers_controller.o local_stats.o rx_scope.o radiopackets2.o radiopackets_rc.o forward_watch.o timers.o shared_mem_i2c.o ui_alarms.o string_utils.o radio_stats.o hardware_radio.o controller_utils.o ruby_ipc.o core_plugins_settings.o hardware_serial.o
	$(info ----------------------------------------------------)
	$(info Start building ruby_central)
	g++ -Wl,--export-dynamic -o $@ $^ $(LDFLAGS2) 
	cp -f ruby_central $(RELEASE_DIR)
	$(info Copy ruby_central done)
	$(info ----------------------------------------------------)

clean:
	rm -f ruby_central chars *.o
