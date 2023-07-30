FLAGS = -O3 -g -Wall -lrt -march=native
INCLUDES = -I. -I../../include 
EXEC = mc_interpose.so test.exe

all: ${EXEC}

mc_interpose.so: mc_interpose.cpp
	g++ mc_interpose.cpp -shared -fPIC -ldl ${FLAGS} -o $@ ${INCLUDES} ../../util/m5/build/x86/out/libm5.a

test.exe: test.cpp
	g++ -o $@ $< -g

clean:
	rm ${EXEC}