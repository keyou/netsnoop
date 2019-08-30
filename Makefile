CXX=g++
CXXFLAGS= -std=c++11 -I.
LIBS=-pthread
EXE=

ifeq ($(OS), Windows_NT)
ARCH=WIN32
endif

ifeq ($(ARCH),WIN32)
	ifneq ($(OS), Windows_NT)
	CXX = i686-w64-mingw32-g++-posix
	endif
CXXFLAGS += -D WIN32 -D_WIN32_WINNT=0x601 -DFD_SETSIZE=1024
LIBS += -lws2_32
EXE = .exe
endif

ifeq ($(BUILD),DEBUG)
CXXFLAGS += -O0 -g -D_DEBUG
else
CXXFLAGS += -O3 -s
endif

CXXFLAGS += -DBUILD_VERSION=$(shell git rev-list --count HEAD)

DEPS = netsnoop.h command.h
OBJ = command.o context2.o sock.o tcp.o udp.o command_receiver.o command_sender.o peer.o net_snoop_client.o net_snoop_server.o

.PHONY: all
all: netsnoop netsnoop_test netsnoop_select netsnoop_multicast

.PHONY: win32
win32:
	make ARCH=WIN32

.PHONY: win32_debug
win32_debug:
	make ARCH=WIN32 BUILD=DEBUG

.PHONY: debug
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
	
netsnoop_multicast: $(OBJ) netsnoop_multicast.o
	$(CXX) $(CXXFLAGS) -o $@$(EXE) $^ $(LIBS)

%.o: %.cc %.h $(DEPS)
	$(CXX) $(CXXFLAGS) -c -o $@ $< 

.PHONY: clean
clean:
	rm -rf *.o *.obj
	rm -f netsnoop netsnoop_test netsnoop_select
	rm -f netsnoop$(EXE) netsnoop_test$(EXE) netsnoop_select$(EXE)
