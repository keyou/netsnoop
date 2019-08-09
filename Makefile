
CXX=g++
CXXFLAGS= -g -std=c++11 -I. -D _DEBUG
LIBS=-pthread
DEPS = netsnoop.h
OBJ = command.o context2.o sock.o tcp.o udp.o action.o peer.o net_snoop_client.o net_snoop_server.o netsnoop.o

netsnoop: $(OBJ)
	$(CXX) $(CXXFLAGS) $(LIBS) -o $@ $^ 

%.o: %.cc $(DEPS)
	$(CXX) $(CXXFLAGS) -c -o $@ $< 

clear:
	rm -rf *.o