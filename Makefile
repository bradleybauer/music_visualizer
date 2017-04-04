CXX = g++
LIB = -lglfw -lm -lGL -lGLEW -lGLU -lfftw3 -lfftw3_omp -lpulse-simple -lpthread -lpulse
INCLUDE = -I/usr/include/libdrm -I/usr/include/GL -I/usr/include/pulse -Ifftwpp
CXXFLAGS = -Wall -c -O3 -std=c++14 -Wno-switch $(INCLUDE)
# CXXFLAGS = -Wall -c -O0 -g -std=c++14 -Wno-switch $(INCLUDE)
LDFLAGS = $(LIB)
EXE = main

all: $(EXE)

$(EXE): main.o draw.o pulse.o fftw++.o
	$(CXX) -fopenmp $^ $(LDFLAGS) -o $@

fftw++.o: fftwpp/fftw++.cc
	$(CXX) -fopenmp $(CXXFLAGS) $< -o $@

draw.o: draw.cpp
	$(CXX) $(CXXFLAGS) $< -o $@

pulse.o: pulse.cpp
	$(CXX) $(CXXFLAGS) $< -o $@

main.o: main.cpp
	$(CXX) $(CXXFLAGS) $< -o $@

clean:
	rm *.o && rm $(EXE)
install:
	mv main /usr/bin/app
