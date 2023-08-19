.PHONY: mount_img umount_img compile_gem5 launch_gem5

GEM5_IMG ?= ./os/mcsquare-20.04.final.img
GEM5_CPU ?= O3CPU#TimingSimpleCPU
GEM5_KERNEL ?= ./os/vmlinux-5.7
GEM5_TESTS ?= micro/rand-access micro/seq-access#seq-access #rand-access #multi-test##script# #seq-access 
RESULTS ?= ./results

MNT ?= mnt

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

NCPUS ?= 8
MEM_SIZE ?= 3GB
MEM_CHAN ?= 8

CMD_LINE ?= "earlyprintk=ttyS0 console=ttyS0 lpj=7999923 root=/dev/hda1"

launch_gem5:
	sudo ls; \
	$(MAKE) compile_gem5; \
	for test in ${GEM5_TESTS}; do \
		$(MAKE) single_launch_gem5 TEST=$${test} SCRIPT=./scripts/$${test}-run.sh; \
		$(MAKE) extract_test TEST=$${test} SCRIPT=./scripts/$${test}-extract.py; \
	done;

TEST ?= ${GEM5_TESTS}
SCRIPT ?= ./scripts/${TEST}-run.sh
single_launch_gem5:
	mkdir -p ${RESULTS}/${TEST};
	sudo ../build/X86/gem5.opt --debug-flags=PseudoInst -d ${RESULTS}/${TEST} ./fs.py  --fast-forward=1000000000000000000 --mem-size=${MEM_SIZE} --cpu-type=${GEM5_CPU} \
		--l1i-hwp-type=StridePrefetcher --l1d-hwp-type=StridePrefetcher --l2-hwp-type=StridePrefetcher --command-line=${CMD_LINE} --cpu-clock="4GHz" \
		--kernel=${GEM5_KERNEL} --disk-image=${GEM5_IMG} -n ${NCPUS} --caches --l2cache --mem-channels=${MEM_CHAN} --script=${SCRIPT} > ${RESULTS}/${TEST}/fullout_${GEM5_CPU}.txt;

single_launch_gem5_debug:
	mkdir -p ${RESULTS}/${TEST};
	sudo gdb -x run.sh --args ../build/X86/gem5.opt --debug-flags=PseudoInst -d ${RESULTS}/${TEST} ./fs.py  --fast-forward=1000000000000000000 --mem-size=${MEM_SIZE} --cpu-type=${GEM5_CPU} \
		--l1i-hwp-type=StridePrefetcher --l1d-hwp-type=StridePrefetcher --l2-hwp-type=StridePrefetcher --command-line=${CMD_LINE} --cpu-clock="4GHz" \
		--kernel=${GEM5_KERNEL} --disk-image=${GEM5_IMG} -n ${NCPUS} --caches --l2cache --mem-channels=${MEM_CHAN} --script=${SCRIPT} > ${RESULTS}/${TEST}/fullout_${GEM5_CPU}.txt;

extract_test:
	python ${SCRIPT} ${RESULTS}/${TEST}/stats.txt > ${RESULTS}/${TEST}_res.txt;

REDIS_TESTS ?= redis-64K redis-zio-64K redis-mc-64K redis-mc-fake-64K
launch_redis:
	sudo ls; \
	$(MAKE) compile_gem5; \
	for test in ${REDIS_TESTS}; do \
		$(MAKE) single_launch_gem5 TEST=redis/$${test} SCRIPT=./scripts/redis/$${test}-run.sh & \
	done; \
	wait;

	$(MAKE) extract_redis REDIS_TESTS="${REDIS_TESTS}"

extract_redis:
	python ./scripts/redis/redis-extract.py "${REDIS_TESTS}" > ${RESULTS}/redis.txt;
