CXX?=g++
CXXFLAGS?= -std=c++11 -I.
LIBS=-pthread
OBJ=.o
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
OBJ = .obj
EXE = .exe
endif

ifeq ($(BUILD),DEBUG)
CXXFLAGS += -O0 -g -D_DEBUG
else
CXXFLAGS += -O3 -s
endif

BUILD_VERSION?=$(shell git rev-list --count HEAD)
CXXFLAGS += -DBUILD_VERSION=$(BUILD_VERSION)

PACKDIR:=./publish

DEPS = netsnoop.h command.h
OBJS = command$(OBJ) context2$(OBJ) \
		sock$(OBJ) tcp$(OBJ) udp$(OBJ) \
	   	command_receiver$(OBJ) command_sender$(OBJ) \
		peer$(OBJ) \
		net_snoop_client$(OBJ) net_snoop_server$(OBJ)
EXES = netsnoop$(EXE) netsnoop_test$(EXE) netsnoop_select$(EXE) netsnoop_multicast$(EXE)

.PHONY: all
all: $(EXES)

$(EXES): %$(EXE): $(OBJS) %$(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LIBS)

%$(OBJ): %.cc $(DEPS)
	$(CXX) $(CXXFLAGS) -c -o $@ $< 

.PHONY: win32
win32:
	@make ARCH=WIN32

.PHONY: win32_debug
win32_debug:
	@make ARCH=WIN32 BUILD=DEBUG

.PHONY: debug
debug:
	@make BUILD=DEBUG

.PHONY: pack
pack: all win32
	@mkdir -p $(PACKDIR)
	@echo $(EXES) | xargs -n1 | xargs -i cp ./{} $(PACKDIR)
	$(eval OBJ=.obj)
	$(eval EXE=.exe)
	@echo $(EXES) | xargs -n1 | xargs -i cp ./{} $(PACKDIR)
	@echo copy to $(PACKDIR) dir success!
	@ls $(PACKDIR)

.PHONY: clean
clean:
	@echo $(OBJS) $(EXES) | xargs -n1 rm -f
	$(eval OBJ=.obj)
	$(eval EXE=.exe)
	@echo $(OBJS) $(EXES) | xargs -n1 rm -f
	@echo clean success!

