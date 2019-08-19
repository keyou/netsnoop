
CXX=g++
CXXFLAGS= -g -std=c++11 -I. -D _DEBUG
LIBS=-pthread
DEPS = netsnoop.h command.h
OBJ = command.o context2.o sock.o tcp.o udp.o command_receiver.o command_sender.o peer.o net_snoop_client.o net_snoop_server.o

.PHONY: all
all: netsnoop test

netsnoop: $(OBJ) netsnoop.o
	$(CXX) $(CXXFLAGS) $(LIBS) -o $@ $^ 

%.o: %.cc %.h $(DEPS)
	$(CXX) $(CXXFLAGS) -c -o $@ $< 

netsnoop_test: $(OBJ) netsnoop_test.o
	$(CXX) $(CXXFLAGS) $(LIBS) -o $@ $^ 

.PHONY: test
test: netsnoop_test

.PHONY: clean
clean:
	rm -rf *.o
	rm netsnoop netsnoop_test
