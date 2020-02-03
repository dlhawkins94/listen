all: .bin/test

CPPC = g++
LIBS = eigen3 fftw3 gl glew glfw3 pangocairo libpulse-simple
CFLAGS = -pthread -O2 -g -std=c++11 `pkg-config --cflags $(LIBS)` -I..
LFLAGS = -pthread -lm `pkg-config --libs $(LIBS)` -L../videotk/.lib -L../audiotk/.lib -laudiotk -lvideotk

OBJS = .bld/denoise.o .bld/dataset.o .bld/net.o .bld/statistics.o .bld/vad.o

#.bin/listen: main.cpp $(OBJS)
#	$(CPPC) $(CFLAGS) main.cpp $(OBJS) -o .bin/listen $(LFLAGS)

#.bin/record: record.cpp .bld/denoise.o
#	$(CPPC) $(CFLAGS) record.cpp .bld/denoise.o  -o .bin/record $(LFLAGS)

.bin/test: test.cpp $(OBJS)
	$(CPPC) $(CFLAGS) test.cpp $(OBJS) -o .bin/test $(LFLAGS)

.bld/dataset.o: dataset.cpp dataset.hpp
	$(CPPC) -c $(CFLAGS) dataset.cpp -o .bld/dataset.o $(LFLAGS)

.bld/denoise.o: denoise.cpp denoise.hpp
	$(CPPC) -c $(CFLAGS) denoise.cpp -o .bld/denoise.o $(LFLAGS)

.bld/net.o: net.cpp net.hpp
	$(CPPC) -c $(CFLAGS) net.cpp -o .bld/net.o $(LFLAGS)

.bld/statistics.o: statistics.cpp statistics.hpp
	$(CPPC) -c $(CFLAGS) statistics.cpp -o .bld/statistics.o $(LFLAGS)

.bld/vad.o: vad.cpp vad.hpp
	$(CPPC) -c $(CFLAGS) vad.cpp -o .bld/vad.o $(LFLAGS)

clean:
	rm .bld/* .bin/*
