all: libsdaa.so test_data_recv

LIBS=""

sdaa_data.o: sdaa_data.cpp
	g++ --std=c++23 -c $< -o $@ -O3 -g

ddc_kernel.o: ddc_kernel.cu
	nvcc -c $< -o $@ $(OPT) --cudart=static --cudadevrt=none

test_data_recv.o: test_data_recv.cpp
	g++ -c $< -o $@ -O3 -g

sdaa.o:	sdaa.cpp
	g++ -c --std=c++23 $< -o $@ -O3 -g

libsdaa.so: sdaa.o ddc_kernel.o sdaa_data.o
	g++  --shared  $^ -o $@ -lSoapySDR -lyaml-cpp -lcudart ../sdaa_ctrl/target/release/libsdaa_ctrl.a

test_data_recv: test_data_recv.o ddc_kernel.o sdaa_data.o
	g++ $^ -o $@ -O3 -lcudart -L ../sdaa_ctrl/target/release  ../sdaa_ctrl/target/release/libsdaa_ctrl.a -g

clean:
	rm -f *.o
