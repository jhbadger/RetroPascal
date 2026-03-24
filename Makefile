CXX      = g++
CXXFLAGS = -std=c++17 -O2

all: pascal tpide

pascal: pascal_interpreter.cpp
	$(CXX) $(CXXFLAGS) -o pascal pascal_interpreter.cpp

tpide: turbo_pascal_ide.cpp
	$(CXX) $(CXXFLAGS) -o tpide turbo_pascal_ide.cpp

clean:
	rm -f pascal tpide

.PHONY: all clean
