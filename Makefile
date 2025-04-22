all: libsdaa.so test_data_recv test_soapy

LIBS=-lSoapySDR -lyaml-cpp -lcudart ../sdaa_ctrl/target/release/libsdaa_ctrl.a ../cuddc/libcuddc.a
CFLAGS=-g -I ../cuddc

sdaa_data.o: sdaa_data.cpp
	g++ --std=c++23 -c $< -o $@ -O3 $(CFLAGS)

#ddc_kernel.o: ddc_kernel.cu
#	nvcc -c $< -o $@ $(OPT) --cudart=static --cudadevrt=none $(CFLAGS)

test_data_recv.o: test_data_recv.cpp
	g++ -c $< -o $@ -O3 -g $(CFLAGS)

sdaa.o:	sdaa.cpp
	g++ -c --std=c++23 $< -o $@ -O3 $(CFLAGS)

#libsdaa.so: sdaa.o ddc_kernel.o sdaa_data.o
#	g++ --shared  $^ -o $@ $(LIBS)

test_data_recv: test_data_recv.o sdaa_data.o
	g++ $^ -o $@ -O3 $(LIBS)

test_soapy: test_soapy.o
	g++ $< -o $@ -O3 $(LIBS)

test_soapy.o: test_soapy.cpp
	g++ -c $< -o $@ -O3 $(CFLAGS)


clean:
	rm -f *.o
