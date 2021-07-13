#
# exi-codec - Makefile
#

cflags := -O3 -g3 -Wall -MD -fmessage-length=0 -std=c++17 -Wl,--no-as-needed -fPIC
includes :=  -I"include" -I"ceps-core/include" -I"ceps-core/include/include-gen" 
objfiles := bin/main.o 
cepslibs := ceps-core/bin

all: exi-codec

exi-codec: $(objfiles) 
	$(CXX)   $(cflags) $(includes) -L$(cepslibs) bin/main.o -lpthread -lcepscore  -o exi-codec
bin/main.o: src/main.cpp
	$(CXX)   $(cflags) $(includes) src/main.cpp -c -o bin/main.o
