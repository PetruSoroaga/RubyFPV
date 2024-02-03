LDFLAGS=-lrt -lpcap -lpthread
CPPFLAGS=-Wall -O0 -D _GNU_SOURCE
CFLAGS := $(CFLAGS) 

all: my_plugin

%.o: %.cpp
	g++ $(CFLAGS) -c -o $@ $< $(CPPFLAGS)  

my_plugin: my_plugin_source.o
	gcc my_plugin_source.o -shared -Wl,-soname,my_plugin.so.1 -o my_plugin.so.1.0.1 -lc

clean:
	rm -f my_plugin.so.* *.o
