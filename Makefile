CXX=g++
CXXFLAGS=-g -std=c++17 -Wall -pedantic -lopenal -lfftw3
CXXFLAGS += -lusb-1.0
CXXFLAGS += -Ofast
BIN = mk_pro_m_spec

SRCS = main.cpp
SRCS += ModularSpec/OpenALDataFetcher.cpp
SRCS += ModularSpec/Spectrum.cpp

all:
	$(CXX) $(CXXFLAGS) $(SRCS) -o $(BIN)

clean:
	rm $(BIN)