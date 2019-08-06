
CXX=g++
CXXFLAGS= -g -std=c++11 -I. 
DEPS = netsnoop.h
OBJ = netsnoop.o

netsnoop: $(OBJ)
	$(CXX) -o $@ $^ $(CXXFLAGS)

%.o: %.cc $(DEPS)
	$(CXX) -c -o $@ $< $(CXXFLAGS)

