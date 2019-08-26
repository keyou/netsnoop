CXX=g++
CXXFLAGS= -g -std=c++11 -I. -D _DEBUG
LIBS=-pthread
EXE=

WIN32_CXX=i686-w64-mingw32-g++-posix
WIN32_FLAGS= -I/usr/i686-w64-mingw32/include -D WIN32
WIN32_LIBS=-pthread -lws2_32
WIN32_EXE=.exe

ifeq ($(ARCH), WIN32)
CXX=$(WIN32_CXX)
CXXFLAGS+=$(WIN32_FLAGS)
LIBS=$(WIN32_LIBS)
EXE=$(WIN32_EXE)
endif

DEPS = netsnoop.h command.h
OBJ = command.o context2.o sock.o tcp.o udp.o command_receiver.o command_sender.o peer.o net_snoop_client.o net_snoop_server.o

.PHONY: all
all: netsnoop.out netsnoop_test.out netsnoop_select.out

%.out: $(OBJ) %.o
	$(CXX) $(CXXFLAGS) -o $*$(EXE) $^ $(LIBS)

%.o: %.cc %.h $(DEPS)
	$(CXX) $(CXXFLAGS) -c -o $@ $< 

.PHONY: clean
clean:
	rm -rf *.o *.obj
	rm -f netsnoop netsnoop_test netsnoop_select
	rm -f netsnoop$(EXE) netsnoop_test$(EXE) netsnoop_select$(EXE)
