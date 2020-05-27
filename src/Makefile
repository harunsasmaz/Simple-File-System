CXX      := g++
CXXFLAGS := -std=c++14
OBJ1 := contiguous
OBJ2 := linked

all: cntgs linked

cntgs:
	$(CXX) $(CXXFLAGS) -o $(OBJ1) contiguous.cpp

linked:
	$(CXX) $(CXXFLAGS) -o $(OBJ2) linked.cpp

clean:
	rm -f *.o results.txt $(OBJ1) $(OBJ2)