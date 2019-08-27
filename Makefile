CXX=g++
CXXFLAGS= -std=c++11 -I.
LIBS=-pthread
EXE=

WIN32_CXX=g++#i686-w64-mingw32-g++-posix
WIN32_FLAGS= -D WIN32 -D_WIN32_WINNT=0x601 -DFD_SETSIZE=1024 #-D__USE_W32_SOCKETS #-I/usr/i686-w64-mingw32/include
WIN32_LIBS=-pthread -lws2_32
WIN32_EXE=.exe

ifeq ($(ARCH), WIN32)
CXX=$(WIN32_CXX)
CXXFLAGS+=$(WIN32_FLAGS)
LIBS=$(WIN32_LIBS)
EXE=$(WIN32_EXE)
endif

ifeq ($(BUILD),DEBUG)
CXXFLAGS += -O0 -g -D_DEBUG
else
CXXFLAGS += -O3 -s
endif

DEPS = netsnoop.h command.h
OBJ = command.o context2.o sock.o tcp.o udp.o command_receiver.o command_sender.o peer.o net_snoop_client.o net_snoop_server.o

.PHONY: all
all: netsnoop netsnoop_test netsnoop_select

win32:
	make ARCH=WIN32

debug:
	make BUILD=DEBUG

# %.out: $(OBJ) %.o
# 	$(CXX) $(CXXFLAGS) -o $*$(EXE) $^ $(LIBS)
	
netsnoop: $(OBJ) netsnoop.o
	$(CXX) $(CXXFLAGS) -o $@$(EXE) $^ $(LIBS)
	
netsnoop_test: $(OBJ) netsnoop_test.o
	$(CXX) $(CXXFLAGS) -o $@$(EXE) $^ $(LIBS)
	
netsnoop_select: $(OBJ) netsnoop_select.o
	$(CXX) $(CXXFLAGS) -o $@$(EXE) $^ $(LIBS)

%.o: %.cc %.h $(DEPS)
	$(CXX) $(CXXFLAGS) -c -o $@ $< 

.PHONY: clean
clean:
	rm -rf *.o *.obj
	rm -f netsnoop netsnoop_test netsnoop_select
	rm -f netsnoop$(EXE) netsnoop_test$(EXE) netsnoop_select$(EXE)
