FLAGS = -O3 -g -Wall -lrt -march=native
INCLUDES = -I. -I../../include 
EXEC = mc_interpose.so mc_interpose_fake.so test.exe

all: ${EXEC}

%.so: %.cpp
	g++ $< -shared -fPIC -ldl ${FLAGS} -o $@ ${INCLUDES} ../../util/m5/build/x86/out/libm5.a

test.exe: test.cpp
	g++ -o $@ $< -g

clean:
	rm ${EXEC}