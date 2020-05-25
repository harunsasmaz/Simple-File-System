CXX      := g++
CXXFLAGS := -std=c++11
OBJ1 := contiguous
OBJ2 := linked

all: cntgs linked

cntgs:
	$(CXX) $(CXXFLAGS) -o $(OBJ1) contiguous.cpp

linked:
	$(CXX) $(CXXFLAGS) -o $(OBJ2) linked.cpp

clean:
	rm -f *.o results.log $(OBJ1) $(OBJ2)