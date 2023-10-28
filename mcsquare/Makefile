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

NEW_IMG ?= ./os/mcsquare-20.04-new.img
UBUNTU_BASE ?= ./os/ubuntu-base-20.04.2-base-amd64.tar.gz
create_img:
	../util/gem5img.py init ${NEW_IMG} 4096
	../util/gem5img.py mount ${NEW_IMG} ${MNT}
	sudo tar xzvf ${UBUNTU_BASE} -C ${MNT}
	../util/gem5img.py umount mnt

NCPUS    ?= 8
MEM_SIZE ?= 3GB
MEM_CHAN ?= 8
PREFETCH ?= --l1d-hwp-type=StridePrefetcher --l2-hwp-type=StridePrefetcher
BPQ_SIZE ?= 4
DEST_WB  ?= 3

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
	sudo ../build/X86/gem5.opt --debug-flags=PseudoInst -d ${RESULTS}/${TEST} ./fs.py \
		--fast-forward=1000000000000000000 --mem-size=${MEM_SIZE} --cpu-type=${GEM5_CPU} \
		--l1i-hwp-type=StridePrefetcher ${PREFETCH} --command-line=${CMD_LINE} \
		--cpu-clock="4GHz" --mem-type="DDR4_2400_8x8" --bpq-size=${BPQ_SIZE} \
		--wb-reads=${DEST_WB} --kernel=${GEM5_KERNEL} --disk-image=${GEM5_IMG} -n ${NCPUS} --caches --l2cache \
		--mem-channels=${MEM_CHAN} --script=${SCRIPT} > ${RESULTS}/${TEST}/fullout_${GEM5_CPU}.txt 2> ${RESULTS}/${TEST}/err_${GEM5_CPU}.txt;

single_launch_gem5_debug:
	mkdir -p ${RESULTS}/${TEST};
	sudo gdb -x run.sh --args ../build/X86/gem5.opt --debug-flags=PseudoInst,MCSquare -d ${RESULTS}/${TEST} ./fs.py \
		--fast-forward=1000000000000000000 --mem-size=${MEM_SIZE} --cpu-type=${GEM5_CPU} \
		--l1i-hwp-type=StridePrefetcher ${PREFETCH} --command-line=${CMD_LINE} \
		--cpu-clock="4GHz" --mem-type="DDR4_2400_8x8" --bpq-size=${BPQ_SIZE} \
		--wb-reads=${DEST_WB} --kernel=${GEM5_KERNEL} --disk-image=${GEM5_IMG} -n ${NCPUS} --caches --l2cache \
		--mem-channels=${MEM_CHAN} --script=${SCRIPT} > ${RESULTS}/${TEST}/fullout_${GEM5_CPU}.txt 2> ${RESULTS}/${TEST}/err_${GEM5_CPU}.txt;

extract_test:
	python ${SCRIPT} ${RESULTS}/${TEST}/stats.txt > ${RESULTS}/${TEST}_res.txt;

REDIS_TESTS ?= redis-8K redis redis-32K redis-64K 
REDIS_MC_TESTS ?= redis-mc-8K redis-mc redis-mc-32K redis-mc-64K
launch_redis:
	sudo ls; \
	$(MAKE) compile_gem5; \
	for test in ${REDIS_TESTS}; do \
		$(MAKE) single_launch_gem5 TEST=redis/$${test} & \
	done; \
	for test in ${REDIS_MC_TESTS}; do \
		$(MAKE) single_launch_gem5 TEST=redis/$${test}-1 SCRIPT=./scripts/redis/$${test}-run.sh BPQ_SIZE=1 & \
		$(MAKE) single_launch_gem5 TEST=redis/$${test}-2 SCRIPT=./scripts/redis/$${test}-run.sh BPQ_SIZE=2 & \
		$(MAKE) single_launch_gem5 TEST=redis/$${test}-4 SCRIPT=./scripts/redis/$${test}-run.sh BPQ_SIZE=4 & \
		$(MAKE) single_launch_gem5 TEST=redis/$${test}-8 SCRIPT=./scripts/redis/$${test}-run.sh BPQ_SIZE=8 & \
	done; \
	wait;

	$(MAKE) extract_redis > ${RESULTS}/redis.txt;

extract_redis:
	python ./scripts/redis/redis-extract.py "${REDIS_TESTS}" "${REDIS_MC_TESTS}" "1 2 4 8"

MC_PICKLE_IMG ?= ./os/mcsquare-20.04-python.img
PICKLE_TESTS ?= pickle-1M pickle-128K pickle-16K
PICKLE_MC_TESTS ?= pickle-mc-1M pickle-mc-128K pickle-mc-16K
launch_pickle:
	sudo ls; \
	$(MAKE) compile_gem5; \
	for i in ${PICKLE_MC_TESTS}; do \
		$(MAKE) single_launch_gem5 GEM5_IMG=${MC_PICKLE_IMG} TEST=pickle/$$i & \
	done; \
	for i in ${PICKLE_TESTS}; do \
		$(MAKE) single_launch_gem5 TEST=pickle/$$i & \
	done; \
	wait;
	$(MAKE) extract_pickle > ${RESULTS}/pickle/pickle.txt;

extract_pickle:
	python ./scripts/pickle/pickle-extract.py "${PICKLE_TESTS} ${PICKLE_MC_TESTS}"

PROTOBUF_TESTS ?= protobuf protobuf-mc
launch_protobuf:
	sudo ls; \
	$(MAKE) compile_gem5; \
	for i in ${PROTOBUF_TESTS}; do \
		$(MAKE) single_launch_gem5 TEST=protobuf/$$i & \
	done; \
	wait;

SRC_WRITE_SIZES ?= 1 2 4 8 16
launch_src_write:
	sudo ls; \
	$(MAKE) compile_gem5; \
	for i in ${SRC_WRITE_SIZES}; do \
		$(MAKE) single_launch_gem5 TEST=micro/src-write/seq-access-$${i} SCRIPT=./scripts/micro/seq-access-src-write-run.sh BPQ_SIZE=$${i} & \
	done; \
	$(MAKE) single_launch_gem5 TEST=micro/src-write/seq-access-memcpy SCRIPT=./scripts/micro/seq-access-src-write-memcpy-run.sh  & \
	$(MAKE) single_launch_gem5 TEST=micro/src-write/seq-access-zio SCRIPT=./scripts/micro/seq-access-src-write-zio-run.sh & \
	wait;

	$(MAKE) extract_src_write > ${RESULTS}/micro/seq-access-src-write-batch.txt;

extract_src_write:
	python scripts/micro/access-batch-extract.py results/micro/src-write/seq-access- "memcpy zio ${SRC_WRITE_SIZES}"

launch_seq_access_micro:
	sudo ls; \
	$(MAKE) compile_gem5; \
	$(MAKE) single_launch_gem5 TEST=micro/seq/MCSquare SCRIPT=./scripts/micro/seq-access-run.sh & \
	$(MAKE) single_launch_gem5 TEST=micro/seq/MCSquare-noprefetch SCRIPT=./scripts/micro/seq-access-run.sh PREFETCH= & \
	$(MAKE) single_launch_gem5 TEST=micro/seq/Memcpy SCRIPT=./scripts/micro/seq-access-memcpy-run.sh & \
	$(MAKE) single_launch_gem5 TEST=micro/seq/zIO SCRIPT=./scripts/micro/seq-access-zio-run.sh & \
	wait;

	$(MAKE) extract_seq_access > ${RESULTS}/micro/seq-access-batch.txt;

extract_seq_access:
	python scripts/micro/access-batch-extract.py results/micro/seq/ "Memcpy zIO MCSquare MCSquare-noprefetch"

launch_rand_access_micro:
	sudo ls; \
	$(MAKE) compile_gem5; \
	$(MAKE) single_launch_gem5 TEST=micro/rand/access-0 SCRIPT=./scripts/micro/rand-access-run.sh DEST_WB=0 & \
	$(MAKE) single_launch_gem5 TEST=micro/rand/access-1 SCRIPT=./scripts/micro/rand-access-run.sh DEST_WB=1 & \
	$(MAKE) single_launch_gem5 TEST=micro/rand/access-2 SCRIPT=./scripts/micro/rand-access-run.sh DEST_WB=2 & \
	$(MAKE) single_launch_gem5 TEST=micro/rand/access-memcpy SCRIPT=./scripts/micro/rand-access-memcpy-run.sh & \
	$(MAKE) single_launch_gem5 TEST=micro/rand/access-zio SCRIPT=./scripts/micro/rand-access-zio-run.sh & \
	wait;

	$(MAKE) extract_rand_access > ${RESULTS}/micro/rand-access-batch.txt;

extract_rand_access:
	python scripts/micro/access-batch-extract.py results/micro/rand/ "access-0 access-1 access-2 access-memcpy access-zio"
