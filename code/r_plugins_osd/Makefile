LDFLAGS=-lrt -lpcap -lpthread
CPPFLAGS=-Wall -O2
CFLAGS := $(CFLAGS) -Wall -O2 
RELEASE_DIR = ../../

all: ruby_plugin my_plugin ruby_plugin_gauge_speed ruby_plugin_gauge_altitude ruby_plugin_gauge_ahi ruby_plugin_gauge_heading

osd_plugins_utils.o: ../public/utils/osd_plugins_utils.c
	gcc -c -o $@ $< $(CPPFLAGS)

core_plugins_utils.o: ../public/utils/core_plugins_utils.c
	gcc -c -o $@ $< $(CPPFLAGS)

%.o: %.cpp
	g++ $(CFLAGS) -std=gnu++11 -c -o $@ $< $(CPPFLAGS)  


ruby_plugin: ruby_plugin_osd_ahi.o osd_plugins_utils.o core_plugins_utils.o
	gcc ruby_plugin_osd_ahi.o -shared -Wl,-soname,ruby_ahi_plugin.so.1 -o ruby_ahi_plugin.so.1.0.1 -lc
	cp -f ruby_ahi_plugin.so.1.0.1 $(RELEASE_DIR)
	$(info Copy ruby_ahi_plugin done)
	$(info ----------------------------------------------------)

ruby_plugin_gauge_speed: ruby_plugin_gauge_speed.o osd_plugins_utils.o
	gcc ruby_plugin_gauge_speed.o -shared -Wl,-soname,ruby_plugin_gauge_speed.so.1 -o ruby_plugin_gauge_speed.so.1.0.1 -lc
	cp -f ruby_plugin_gauge_speed.so.1.0.1 $(RELEASE_DIR)
	$(info Copy ruby_plugin_gauge_speed done)
	$(info ----------------------------------------------------)

ruby_plugin_gauge_altitude: ruby_plugin_gauge_altitude.o osd_plugins_utils.o core_plugins_utils.o
	gcc ruby_plugin_gauge_altitude.o osd_plugins_utils.o core_plugins_utils.o -shared -Wl,-soname,ruby_plugin_gauge_altitude.so.1 -o ruby_plugin_gauge_altitude.so.1.0.1 -lc
	cp -f ruby_plugin_gauge_altitude.so.1.0.1 $(RELEASE_DIR)
	$(info Copy ruby_plugin_gauge_altitude done)
	$(info ----------------------------------------------------)

ruby_plugin_gauge_ahi: ruby_plugin_gauge_ahi.o osd_plugins_utils.o core_plugins_utils.o
	gcc ruby_plugin_gauge_ahi.o -shared -Wl,-soname,ruby_plugin_gauge_ahi.so.1 -o ruby_plugin_gauge_ahi.so.1.0.1 -lc
	cp -f ruby_plugin_gauge_ahi.so.1.0.1 $(RELEASE_DIR)
	$(info Copy ruby_plugin_gauge_ahi done)
	$(info ----------------------------------------------------)

ruby_plugin_gauge_heading: ruby_plugin_gauge_heading.o osd_plugins_utils.o core_plugins_utils.o
	gcc ruby_plugin_gauge_heading.o -shared -Wl,-soname,ruby_plugin_gauge_heading.so.1 -o ruby_plugin_gauge_heading.so.1.0.1 -lc
	cp -f ruby_plugin_gauge_heading.so.1.0.1 $(RELEASE_DIR)
	$(info Copy ruby_plugin_gauge_heading done)
	$(info ----------------------------------------------------)

my_plugin: my_plugin_source.o
	gcc my_plugin_source.o -shared -Wl,-soname,my_plugin.so.1 -o my_plugin.so.1.0.1 -lc
	cp -f my_plugin.so.1.0.1 $(RELEASE_DIR)

clean:
	rm -f ruby_ahi_plugin.so.* ruby_plugin_gauge_speed.so.* ruby_plugin_gauge_altitude.so.* ruby_plugin_gauge_ahi.so.* ruby_plugin_gauge_heading.so.* my_plugin.* *.o
