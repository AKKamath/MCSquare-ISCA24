.PHONY: mount_img umount_img compile_gem5 launch_gem5

GEM5_IMG ?= ./mcsquare-20.04.2.img
GEM5_CPU ?= O3CPU#TimingSimpleCPU
GEM5_KERNEL ?= ./vmlinux-5.7
GEM5_TESTS ?= #seq-access #rand-access #multi-test##script# #seq-access 
RESULTS ?= ./results

MNT ?= mnt

LDLIBS = -lrt

mount_img:
	../util/gem5img.py mount ${GEM5_IMG} ${MNT}; \
	sudo /bin/mount -o bind /sys ${MNT}/sys; \
	sudo /bin/mount -o bind /dev ${MNT}/dev; \
	sudo /bin/mount -o bind /proc ${MNT}/proc; \
	sudo /usr/sbin/chroot ${MNT} /bin/bash;

umount_img:
	sudo /bin/umount ${MNT}/sys; \
	sudo /bin/umount ${MNT}/proc; \
	sudo /bin/umount ${MNT}/dev; \
	../util/gem5img.py umount ${MNT};

compile_gem5:
	cd ..; scons build/X86/gem5.opt

NTHREADS ?= 2
MEM_SIZE ?= 3GB
MEM_CHAN ?= 8

CMD_LINE ?= "earlyprintk=ttyS0 console=ttyS0 lpj=7999923 root=/dev/hda1"

launch_gem5:
	sudo ls; \
	$(MAKE) compile_gem5; \
	for test in ${GEM5_TESTS}; do \
		$(MAKE) single_launch_gem5 TEST=$${test}; \
		$(MAKE) extract_test TEST=$${test}; \
	done;

TEST ?= ${GEM5_TESTS}
single_launch_gem5:
	mkdir -p ${RESULTS}/${TEST};
	sudo ../build/X86/gem5.opt --debug-flags=PseudoInst -d ${RESULTS}/${TEST} ./fs.py  --fast-forward=1000000000000000000 --mem-size=${MEM_SIZE} --cpu-type=${GEM5_CPU} \
		--l1i-hwp-type=StridePrefetcher --l1d-hwp-type=StridePrefetcher --l2-hwp-type=StridePrefetcher --command-line=${CMD_LINE} --cpu-clock="4GHz" \
		--kernel=${GEM5_KERNEL} --disk-image=${GEM5_IMG} -n ${NTHREADS} --caches --l2cache --mem-channels=${MEM_CHAN} --script=./scripts/run-${TEST}.sh > ${RESULTS}/${TEST}/fullout_${GEM5_CPU}.txt;

single_launch_gem5_debug:
	mkdir -p ${RESULTS}/${TEST};
	sudo gdb -x run.sh --args ../build/X86/gem5.opt --debug-flags=PseudoInst -d ${RESULTS}/${TEST} ./fs.py  --fast-forward=1000000000000000000 --mem-size=${MEM_SIZE} --cpu-type=${GEM5_CPU} \
		--l1i-hwp-type=StridePrefetcher --l1d-hwp-type=StridePrefetcher --l2-hwp-type=StridePrefetcher --command-line=${CMD_LINE} --cpu-clock="4GHz" \
		--kernel=${GEM5_KERNEL} --disk-image=${GEM5_IMG} -n ${NTHREADS} --caches --l2cache --mem-channels=${MEM_CHAN} --script=./scripts/run-${TEST}.sh > ${RESULTS}/${TEST}/fullout_${GEM5_CPU}.txt;

extract_test:
	python ./scripts/extract-${TEST}.py ${RESULTS}/${TEST}/stats.txt > ${RESULTS}/res_${TEST}.txt;

copy_sweep: copy_sweep.cpp
	g++ -o $@ $^ $(LDLIBS) -g  -I../include ../util/m5/build/x86/out/libm5.a -lpthread -pthread
