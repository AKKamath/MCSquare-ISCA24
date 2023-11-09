FLAGS = -O3 -g -Wall -lrt -march=native
INCLUDES = -I. -I../../include 
EXEC = mc_interpose.so mc_interpose_free.so mc_interpose_fake.so test.exe test_explicit.exe redis_mc_interpose.so

all: ${EXEC}

%.so: %.cpp
	g++ $< -shared -fPIC -ldl ${FLAGS} -o $@ ${INCLUDES} ../../util/m5/build/x86/out/libm5.a

%.exe: %.cpp
	g++ -o $@ $< -g ${FLAGS} ${INCLUDES} ../../util/m5/build/x86/out/libm5.a

clean:
	rm ${EXEC}