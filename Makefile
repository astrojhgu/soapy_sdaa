all: libsdaa.so

LIBS=-lSoapySDR -lyaml-cpp -L ../sdaa_data/target/release -lsdaa_data -lcudart_static -lcufft_static_nocallback -lculibos
CFLAGS=-g -I ../sdaa_data/cuddc -I ../sdaa_ctrl/include -I ../sdaa_data/include

#ddc_kernel.o: ddc_kernel.cu
#	nvcc -c $< -o $@ $(OPT) --cudart=static --cudadevrt=none $(CFLAGS)

sdaa.o:	sdaa.cpp
	g++ -c --std=c++23 $< -o $@ -O3 $(CFLAGS)

libsdaa.so: sdaa.o
	g++ -fPIC --shared  $^ -o $@ $(LIBS)

clean:
	rm -f *.o
