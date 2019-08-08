
CXX=g++
CXXFLAGS= -g -std=c++11 -I. -D DEBUG
DEPS = netsnoop.h
OBJ = netsnoop.o

netsnoop: $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^ 

%.o: %.cc $(DEPS)
	$(CXX) $(CXXFLAGS) -c -o $@ $< 

