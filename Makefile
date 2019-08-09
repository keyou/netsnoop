
CXX=g++
CXXFLAGS= -g -std=c++11 -I. -D _DEBUG
LIBS=-pthread
DEPS = netsnoop.h
OBJ = netsnoop.o

netsnoop: $(OBJ)
	$(CXX) $(CXXFLAGS) $(LIBS) -o $@ $^ 

%.o: %.cc $(DEPS)
	$(CXX) $(CXXFLAGS) -c -o $@ $< 

