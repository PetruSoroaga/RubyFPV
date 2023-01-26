LDFLAGS=-lrt -lpcap -lpthread
CPPFLAGS=-Wall -O0 -D _GNU_SOURCE
CFLAGS := $(CFLAGS) 
RELEASE_DIR = ../

all: plugin_core_example

core_plugins_utils.o: ../public/utils/core_plugins_utils.c
	gcc -c -o $@ $< $(CPPFLAGS)

%.o: %.cpp
	g++ $(CFLAGS) -c -o $@ $< $(CPPFLAGS)  


plugin_core_example: core_plugins_utils.o plugin_core_example.o
	gcc plugin_core_example.o core_plugins_utils.o -shared -Wl,-soname,plugin_core_example.so.1 -o plugin_core_example.so.1.0.1 -lc
	cp -f plugin_core_example.so.1.0.1 $(RELEASE_DIR)
	$(info Copy plugin_core_example)
	$(info ----------------------------------------------------)

clean:
	rm -f plugin_core_example.so.* *.o
