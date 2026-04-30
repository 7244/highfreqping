CXX = g++
CXXFLAGS = -std=c++23 -Wall -Wextra -O3 -ffast-math
LDFLAGS =

release:
	$(CXX) $(CXXFLAGS) -o highfreqping main.cpp $(LDFLAGS)

clean:
	rm -f highfreqping
