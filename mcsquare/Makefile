.PHONY: mount_img umount_img compile_gem5 launch_gem5

GEM5_IMG ?= ./os/mcsquare-20.04.final.img
GEM5_CPU ?= O3CPU
GEM5_KERNEL ?= ./os/vmlinux-5.7
GEM5_TESTS ?= micro/rand-access micro/seq-access#seq-access #rand-access #multi-test##script# #seq-access 
RESULTS ?= ./results
FIGURES ?= ./figures

# Default command. Compile GEM5.
compile_gem5:
	cd ..; scons build/X86/gem5.opt

############################################################
#                DISK IMAGE OPERATIONS
#  create_img: Create a fresh disk image (untested)
#  mount_img: Mount the disk image GEM5_IMG
#  umount_img: Unmount the image mounted at MNT
############################################################

MNT ?= mnt
NEW_IMG ?= ./os/mcsquare-20.04-new.img
UBUNTU_BASE ?= ./os/ubuntu-base-20.04.2-base-amd64.tar.gz
create_img:
	../util/gem5img.py init ${NEW_IMG} 4096
	../util/gem5img.py mount ${NEW_IMG} ${MNT}
	sudo tar xzvf ${UBUNTU_BASE} -C ${MNT}
	../util/gem5img.py umount mnt

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

############################################################
#                   GEM5 TEST COMMANDS
#  launch_gem5: Launch GEM5 for GEM5_TESTS and extract results
#  single_launch_gem5: Launch GEM5 for a single test TEST
#  single_launch_gem5_debug: Same as above but with GDB
#  extract_test: Extract results for a single test TEST
############################################################

NCPUS    ?= 8
MEM_SIZE ?= 3GB
MEM_CHAN ?= 2
PREFETCH ?= --l1d-hwp-type=StridePrefetcher --l2-hwp-type=StridePrefetcher
CTT_SIZE ?= 2048
CTT_FRAC ?= 0.5
CTT_LAT  ?= "787ps"
BPQ_SIZE ?= 8
DEST_WB  ?= 3

CMD_LINE ?= "earlyprintk=ttyS0 console=ttyS0 lpj=7999923 root=/dev/hda1 numa=on"

launch_gem5:
	sudo ls; \
	$(MAKE) compile_gem5; \
	for test in ${GEM5_TESTS}; do \
		$(MAKE) single_launch_gem5 TEST=$${test} SCRIPT=./scripts/$${test}-run.sh & \
	done; \
	wait; \
	for test in ${GEM5_TESTS}; do \
		$(MAKE) extract_test TEST=$${test} SCRIPT=./scripts/$${test}-extract.py; \
	done; \

TEST ?= ${GEM5_TESTS}
SCRIPT ?= ./scripts/${TEST}-run.sh
single_launch_gem5:
	mkdir -p ${RESULTS}/${TEST};
	mkdir -p ${FIGURES};
	sudo ../build/X86/gem5.opt --debug-flags=PseudoInst -d ${RESULTS}/${TEST} ./fs.py \
		--fast-forward=1000000000000000000 --mem-size=${MEM_SIZE} --cpu-type=${GEM5_CPU} \
		--l1i-hwp-type=StridePrefetcher ${PREFETCH} --command-line=${CMD_LINE} --ctt-size=${CTT_SIZE} \
		--ctt-frac=${CTT_FRAC} --ctt-lat=${CTT_LAT} --cpu-clock="4GHz" --mem-type="DDR4_2400_8x8" --bpq-size=${BPQ_SIZE} \
		--wb-reads=${DEST_WB} --kernel=${GEM5_KERNEL} --disk-image=${GEM5_IMG} -n ${NCPUS} --caches --l2cache \
		--mem-channels=${MEM_CHAN} --script=${SCRIPT} > ${RESULTS}/${TEST}/fullout_${GEM5_CPU}.txt 2> ${RESULTS}/${TEST}/err_${GEM5_CPU}.txt;

single_launch_gem5_debug:
	mkdir -p ${RESULTS}/${TEST};
	mkdir -p ${FIGURES};
	sudo gdb -x run.sh --args ../build/X86/gem5.opt --debug-flags=PseudoInst,MCSquare -d ${RESULTS}/${TEST} ./fs.py \
		--fast-forward=1000000000000000000 --mem-size=${MEM_SIZE} --cpu-type=${GEM5_CPU} \
		--l1i-hwp-type=StridePrefetcher ${PREFETCH} --command-line=${CMD_LINE} --ctt-size=${CTT_SIZE} \
		--ctt-frac=${CTT_FRAC} --ctt-lat=${CTT_LAT} --cpu-clock="4GHz" --mem-type="DDR4_2400_8x8" --bpq-size=${BPQ_SIZE} \
		--wb-reads=${DEST_WB} --kernel=${GEM5_KERNEL} --disk-image=${GEM5_IMG} -n ${NCPUS} --caches --l2cache \
		--mem-channels=${MEM_CHAN} --script=${SCRIPT} > ${RESULTS}/${TEST}/fullout_${GEM5_CPU}.txt 2> ${RESULTS}/${TEST}/err_${GEM5_CPU}.txt;

extract_test:
	mkdir -p ${FIGURES};
	python3 ${SCRIPT} ${RESULTS}/${TEST}/stats.txt

launch_all:
	$(MAKE) launch_micro_latency
	$(MAKE) launch_micro_breakdown
	$(MAKE) launch_micro_seq
	$(MAKE) launch_micro_rand
	$(MAKE) launch_protobuf
	$(MAKE) launch_mongo
	$(MAKE) launch_mvcc
	$(MAKE) launch_hugepage_access
	$(MAKE) launch_pipe
	$(MAKE) launch_src_write

extract_all:
	$(MAKE) -s extract_micro_latency > ${RESULTS}/figure_10.txt;
	$(MAKE) -s extract_micro_breakdown > ${RESULTS}/figure_11.txt;
	$(MAKE) -s extract_micro_seq > ${RESULTS}/figure_12.txt;
	$(MAKE) -s extract_micro_rand > ${RESULTS}/figure_13.txt;
	$(MAKE) -s extract_protobuf > ${RESULTS}/figure_20.txt;
	$(MAKE) -s extract_mongo > ${RESULTS}/figure_15.txt;
	$(MAKE) -s extract_mvcc > ${RESULTS}/figures_16_17.txt;
	$(MAKE) -s extract_hugepage_access > ${RESULTS}/figure_18.txt;
	$(MAKE) -s extract_pipe > ${RESULTS}/figure_19.txt;
	$(MAKE) -s extract_src_write > ${RESULTS}/figure_21.txt;
############################################################
#                   MICROBENCHMARK LAUNCHERS			   #
############################################################

# FIGURE 10
launch_micro_latency:
	sudo ls; \
	$(MAKE) compile_gem5; \
	$(MAKE) single_launch_gem5 TEST=micro/multi-test & \
	$(MAKE) single_launch_gem5 TEST=micro/multi-test-cached & \
	wait; \
	$(MAKE) -s extract_micro_latency > ${RESULTS}/figure_10.txt;

extract_micro_latency:
	python3 scripts/micro/multi-test-extract.py "multi-test multi-test-cached" ${FIGURES}/figure_10.png

# FIGURE 11
launch_micro_breakdown:
	sudo ls; \
	$(MAKE) compile_gem5; \
	$(MAKE) single_launch_gem5 TEST=micro/multi-test-breakdown;
	$(MAKE) -s extract_micro_breakdown > ${RESULTS}/figure_11.txt;

extract_micro_breakdown:
	python3 scripts/micro/multi-test-breakdown-extract.py ${RESULTS}/micro/multi-test-breakdown/stats.txt ${FIGURES}/figure_11.png

# FIGURE 12
launch_micro_seq:
	sudo ls; \
	$(MAKE) compile_gem5; \
	$(MAKE) single_launch_gem5 TEST=micro/seq/MCSquare SCRIPT=./scripts/micro/seq-access-run.sh & \
	$(MAKE) single_launch_gem5 TEST=micro/seq/MCSquare-NoPrefetch SCRIPT=./scripts/micro/seq-access-run.sh PREFETCH= & \
	$(MAKE) single_launch_gem5 TEST=micro/seq/Memcpy SCRIPT=./scripts/micro/seq-access-memcpy-run.sh & \
	$(MAKE) single_launch_gem5 TEST=micro/seq/zIO SCRIPT=./scripts/micro/seq-access-zio-run.sh & \
	wait;
	$(MAKE) -s extract_micro_seq > ${RESULTS}/figure_12.txt;

extract_micro_seq:
	python3 scripts/micro/seq-extract.py results/micro/seq/ "Memcpy zIO MCSquare MCSquare-NoPrefetch" ${FIGURES}/figure_12.png

# FIGURE 13
launch_micro_rand:
	sudo ls; \
	$(MAKE) compile_gem5; \
	$(MAKE) single_launch_gem5 TEST=micro/rand/MCSquare-NoWriteback SCRIPT=./scripts/micro/rand-access-run.sh DEST_WB=0 & \
	$(MAKE) single_launch_gem5 TEST=micro/rand/MCSquare SCRIPT=./scripts/micro/rand-access-run.sh DEST_WB=3 & \
	$(MAKE) single_launch_gem5 TEST=micro/rand/Memcpy SCRIPT=./scripts/micro/rand-access-memcpy-run.sh & \
	$(MAKE) single_launch_gem5 TEST=micro/rand/zIO SCRIPT=./scripts/micro/rand-access-zio-run.sh & \
	wait;
	$(MAKE) -s extract_micro_rand > ${RESULTS}/figure_13.txt;

extract_micro_rand:
	python3 scripts/micro/rand-extract.py results/micro/rand/ "Memcpy zIO MCSquare MCSquare-NoWriteback" ${FIGURES}/figure_13.png

############################################################
#                   EXPERIMENT LAUNCHERS				   #
############################################################

# FIGURES 14, 20
PROTOBUF_TESTS ?= protobuf-mc protobuf protobuf-zio
PB_CTT_SIZE=1024 2048 4096
PB_FLUSH_FRAC=0.1 0.25 0.5 0.75 0.9
launch_protobuf:
	sudo ls; \
	$(MAKE) compile_gem5; \
	for ctt_frac in ${PB_FLUSH_FRAC}; do \
		$(MAKE) single_launch_gem5 TEST=protobuf/protobuf-mc-1024-$${ctt_frac} \
			SCRIPT=./scripts/protobuf/protobuf-mc-run.sh \
			CTT_SIZE=1024 CTT_FRAC=$${ctt_frac} CTT_LAT="646ps" & \
		$(MAKE) single_launch_gem5 TEST=protobuf/protobuf-mc-2048-$${ctt_frac} \
			SCRIPT=./scripts/protobuf/protobuf-mc-run.sh \
			CTT_SIZE=2048 CTT_FRAC=$${ctt_frac} CTT_LAT="787ps" & \
		$(MAKE) single_launch_gem5 TEST=protobuf/protobuf-mc-4096-$${ctt_frac} \
			SCRIPT=./scripts/protobuf/protobuf-mc-run.sh \
			CTT_SIZE=4096 CTT_FRAC=$${ctt_frac} CTT_LAT="979ps" & \
	done; \
	$(MAKE) single_launch_gem5 TEST=protobuf/protobuf & \
	$(MAKE) single_launch_gem5 TEST=protobuf/protobuf-zio & \
	wait;
	$(MAKE) -s extract_protobuf > ${RESULTS}/figure_20.txt;

extract_protobuf:
	python3 ./scripts/protobuf/protobuf-extract.py "${PB_CTT_SIZE}" "${PB_FLUSH_FRAC}" ${FIGURES}/figure_14.png

# FIGURE 15
MONGO_TESTS_DUP ?= mongo-100K
MONGO_ZIO_TESTS_DUP ?= mongo-100K-zio
MONGO_MC_TESTS_DUP ?= mongo-100K-mc-all
MONGO_ITERS ?= 1 2 3 4
launch_mongo:
	sudo ls; \
	$(MAKE) compile_gem5; \
	for j in ${MONGO_ITERS}; do \
		for i in ${MONGO_MC_TESTS_DUP}; do \
			$(MAKE) single_launch_gem5 TEST=mongo-new/mc/$${i}-$${j} SCRIPT=./scripts/mongo/$$i-run.sh & \
		done; \
		for i in ${MONGO_ZIO_TESTS_DUP}; do \
			$(MAKE) single_launch_gem5 TEST=mongo-new/zio/$${i}-$${j} SCRIPT=./scripts/mongo/$$i-run.sh & \
		done; \
		for i in ${MONGO_TESTS_DUP}; do \
			$(MAKE) single_launch_gem5 TEST=mongo-new/norm/$${i}-$${j} SCRIPT=./scripts/mongo/$$i-run.sh & \
		done; \
	done; \
	wait;
	$(MAKE) -s extract_mongo > ${RESULTS}/figure_15.txt;

launch_mongo_norm:
	for j in ${MONGO_ITERS}; do \
		for i in ${MONGO_TESTS_DUP}; do \
			$(MAKE) single_launch_gem5 TEST=mongo-new/norm/$${i}-$${j} SCRIPT=./scripts/mongo/$$i-run.sh & \
		done; \
	wait;

launch_mongo_zio:
	for j in ${MONGO_ITERS}; do \
		for i in ${MONGO_ZIO_TESTS_DUP}; do \
			$(MAKE) single_launch_gem5 TEST=mongo-new/zio/$${i}-$${j} SCRIPT=./scripts/mongo/$$i-run.sh & \
		done; \
	wait;

launch_mongo_mc:
	for j in ${MONGO_ITERS}; do \
		for i in ${MONGO_MC_TESTS_DUP}; do \
			$(MAKE) single_launch_gem5 TEST=mongo-new/mc/$${i}-$${j} SCRIPT=./scripts/mongo/$$i-run.sh & \
		done; \
	wait;

extract_mongo:
	python3 ./scripts/mongo/mongo-extract.py "norm/mongo-100K zio/mongo-100K-zio mc/mongo-100K-mc-all" "Baseline zIO (MC)^2" "${MONGO_ITERS}" ${FIGURES}/figure_15.png

# FIGURES 16, 17
MVCC_SIZES=4K
launch_mvcc:
	sudo ls; \
	$(MAKE) compile_gem5; \
	for SIZE in ${MVCC_SIZES}; do \
		$(MAKE) single_launch_gem5 GEM5_TESTS=mvcc/cicada-$${SIZE} & \
		$(MAKE) single_launch_gem5 GEM5_TESTS=mvcc/cicada-mc-$${SIZE} & \
		$(MAKE) single_launch_gem5 GEM5_TESTS=mvcc/cicada-nt-mc-$${SIZE} & \
		$(MAKE) single_launch_gem5 GEM5_TESTS=mvcc/cicada-rmw-$${SIZE} & \
		$(MAKE) single_launch_gem5 GEM5_TESTS=mvcc/cicada-rmw-mc-$${SIZE} & \
	done; \
	wait;
	$(MAKE) -s extract_mvcc > ${RESULTS}/figures_16_17.txt;

# zIO gives an error due to not supporting MAP_SHARED
#		$(MAKE) single_launch_gem5 GEM5_TESTS=mvcc/cicada-zio-$${SIZE} & \
		$(MAKE) single_launch_gem5 GEM5_TESTS=mvcc/cicada-rmw-zio-$${SIZE} & \

MVCC_FRAC=0.0625 0.125 0.25 0.5 1
MVCC_TESTS ?=4K mc-4K nt-mc-4K
MVCC_RMW_TESTS ?=rmw-4K rmw-mc-4K
extract_mvcc:
	python3 scripts/mvcc/cicada-extract.py "${MVCC_RMW_TESTS}" "Baseline (MC)^2" "${MVCC_FRAC}" ${FIGURES}/figure_16.png
	python3 scripts/mvcc/cicada-extract.py "${MVCC_TESTS}" "Baseline (MC)^2 (MC)^2[Nontemporal]" "${MVCC_FRAC}" ${FIGURES}/figure_17.png

# FIGURE 18
launch_hugepage_access:
	sudo ls; \
	$(MAKE) compile_gem5; \
	$(MAKE) single_launch_gem5 TEST=kernel/hugepage-copy-rmw-mini-lazy GEM5_KERNEL=${GEM5_KERNEL}-lazycopy SCRIPT=./scripts/kernel/hugepage-copy-rmw-mini-run.sh & \
	$(MAKE) single_launch_gem5 TEST=kernel/hugepage-copy-rmw-mini SCRIPT=./scripts/kernel/hugepage-copy-rmw-mini-run.sh & \
	wait;
	$(MAKE) -s extract_hugepage_access > ${RESULTS}/figure_18.txt;

extract_hugepage_access:
	python3 scripts/kernel/page-copy-extract.py "hugepage-copy-rmw-mini hugepage-copy-rmw-mini-lazy" "Native (MC)^2" ${FIGURES}/figure_18.png

# FIGURE 19
PIPE_SIZES=1K 2K 4K 8K 16K
launch_pipe:
	sudo ls; \
	$(MAKE) compile_gem5; \
	for i in ${PIPE_SIZES}; do \
		$(MAKE) single_launch_gem5 TEST=kernel/pipe/big-pipe-$${i} SCRIPT=./scripts/kernel/pipe/big-pipe-$${i}-run.sh & \
		$(MAKE) single_launch_gem5 TEST=kernel/pipe/big-pipe-lazy-$${i} SCRIPT=./scripts/kernel/pipe/big-pipe-$${i}-run.sh GEM5_KERNEL=./os/vmlinux-5.7-lazycopy2 & \
	done; \
	wait;
	$(MAKE) -s extract_pipe > ${RESULTS}/figure_19.txt;

extract_pipe:
	python3 scripts/kernel/pipe-extract.py "big-pipe big-pipe-lazy" "Native (MC)^2" "${PIPE_SIZES}" ${FIGURES}/figure_19.png

############################################################
#                   SENSITIVITY LAUNCHERS				   #
############################################################

# FIGURE 21
SRC_WRITE_SIZES ?= 1 2 4 8 16
launch_src_write:
	sudo ls; \
	$(MAKE) compile_gem5; \
	for i in ${SRC_WRITE_SIZES}; do \
		$(MAKE) single_launch_gem5 TEST=micro/src-write/seq-access-$${i} SCRIPT=./scripts/micro/seq-access-src-write-run.sh BPQ_SIZE=$${i} & \
	done; \
	wait;
	$(MAKE) -s extract_src_write > ${RESULTS}/figure_21.txt;

extract_src_write:
	python3 scripts/micro/src-write-extract.py results/micro/src-write/seq-access- "${SRC_WRITE_SIZES}" ${FIGURES}/figure_21.png

############################################################
#                 EXTRA MISC TESTS (IGNORE)	               #
############################################################

CTT_OVERFLOW_SIZES=64 256 1024 4096 16384
launch_ctt_overflow:
	$(MAKE) compile_gem5;
	for i in ${CTT_OVERFLOW_SIZES}; do \
		$(MAKE) single_launch_gem5_debug TEST=micro/overflow/ctt-overflow-$${i} SCRIPT=./scripts/micro/ctt-overflow-run.sh CTT_SIZE=$${i} & \
	done; \
	wait;

#
	
#	$(MAKE) single_launch_gem5 GEM5_TESTS=kernel/pipe SCRIPT=./scripts/kernel/pipe-run.sh & \
	$(MAKE) single_launch_gem5 TEST=kernel/pipe-lazy SCRIPT=./scripts/kernel/pipe-run.sh GEM5_KERNEL=./os/vmlinux-5.7-lazycopy & \
	wait;
#	python3 scripts/kernel/pipe-extract.py "pipe pipe-lazy"

launch_mvcc_test:
	sudo ls; \
	$(MAKE) compile_gem5; \
	$(MAKE) single_launch_gem5 GEM5_TESTS=mvcc/cicada-rmw-4K-test & \
	$(MAKE) single_launch_gem5 GEM5_TESTS=mvcc/cicada-4K-test & \
	$(MAKE) single_launch_gem5_debug GEM5_TESTS=mvcc/cicada-rmw-mc-4K-test & \
	$(MAKE) single_launch_gem5_debug GEM5_TESTS=mvcc/cicada-nt-mc-4K-test & \
	wait;
	$(MAKE) extract_mvcc;

launch_mvcc_sweep:
	sudo ls; \
	MVCC_SCRIPT=./scripts/mvcc/cicada-rmw-mc-sweep-4K-run.sh; \
	MVCC_TEST=mvcc_sweep/cicada-rmw-mc-4K; \
	$(MAKE) compile_gem5; \
	for ctt_frac in ${PB_FLUSH_FRAC}; do \
		$(MAKE) single_launch_gem5 TEST=$${MVCC_TEST}-1024-$${ctt_frac} SCRIPT=$${MVCC_SCRIPT} \
			CTT_SIZE=1024 CTT_FRAC=$${ctt_frac} CTT_LAT="646ps" & \
		$(MAKE) single_launch_gem5 TEST=$${MVCC_TEST}-2048-$${ctt_frac} SCRIPT=$${MVCC_SCRIPT} \
			CTT_SIZE=2048 CTT_FRAC=$${ctt_frac} CTT_LAT="787ps" & \
		$(MAKE) single_launch_gem5 TEST=$${MVCC_TEST}-4096-$${ctt_frac} SCRIPT=$${MVCC_SCRIPT} \
			CTT_SIZE=4096 CTT_FRAC=$${ctt_frac} CTT_LAT="979ps" & \
		$(MAKE) single_launch_gem5 TEST=$${MVCC_TEST}-8192-$${ctt_frac} SCRIPT=$${MVCC_SCRIPT} \
			CTT_SIZE=8192 CTT_FRAC=$${ctt_frac} CTT_LAT="1060ps" & \
		$(MAKE) single_launch_gem5 TEST=$${MVCC_TEST}-16384-$${ctt_frac} SCRIPT=$${MVCC_SCRIPT} \
			CTT_SIZE=16384 CTT_FRAC=$${ctt_frac} CTT_LAT="1352ps" & \
	done; \
	wait;
	$(MAKE) single_launch_gem5 TEST=mvcc_sweep/cicada-rmw-4K \
		SCRIPT=./scripts/mvcc/cicada-rmw-sweep-4K-run.sh & \

extract_mvcc_sweep:
	tests=""; \
	for ctt_frac in ${PB_FLUSH_FRAC}; do \
		for ctt_size in 1024 2048 4096 8192 16384; do \
			tests2="$${tests}mvcc_sweep/cicada-rmw-mc-4K-$${ctt_size}-$${ctt_frac} "; \
			tests=$${tests2}; \
		done; \
	done; \
	tests2="$${tests}mvcc_sweep/cicada-rmw-4K "; \
	tests=$${tests2}; \
	echo $${tests}; \
	printf "Throughput\t"; \
	for FRAC in ${MVCC_FRAC}; do \
		printf "$${FRAC}\t"; \
	done; \
	printf "\n";\
	for SIZE in ${MVCC_SIZES}; do \
		for TEST in $${tests}; do \
			printf "$${TEST}\t"; \
			thput=$$(grep "throughput" ${RESULTS}/$${TEST}/system.pc.com_1.device | grep -oE "[0-9]+\.[0-9]+" | tail -n +2); \
			for i in $${thput}; do \
				printf "$${i}\t"; \
			done; \
			printf "\n"; \
		done; \
	done;

extract_mvcc_sweep2:
	python3 scripts/mvcc/cicada-sweep-extract.py "${PB_CTT_SIZE}" "${PB_FLUSH_FRAC}"