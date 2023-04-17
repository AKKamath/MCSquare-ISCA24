.PHONY: mount_img umount_img compile_gem5 launch_gem5

GEM5_IMG ?= ./mcsquare-18.04.img
GEM5_CPU ?= O3CPU#TimingSimpleCPU # #X86KvmCPU
GEM5_KERNEL ?= ./vmlinux-4.19.83
GEM5_SCRIPT ?= ./scripts/run-script_copy.sh #./scripts/copy_sweep.sh

MNT ?= mnt2

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

launch_gem5:
	sudo ls; \
	$(MAKE) compile_gem5; \
	sudo ../build/X86/gem5.opt --debug-flags=PseudoInst -d ./results/ ./fs.py  --fast-forward=1000000000000000000 --mem-size=8GB --cpu-type=${GEM5_CPU} --kernel=${GEM5_KERNEL} --disk-image=${GEM5_IMG} -n 1 --script=${GEM5_SCRIPT} --ruby
	grep -e "switch_cpus.numCycles" results/stats.txt

copy_sweep: copy_sweep.cpp
	g++ -o $@ $^ $(LDLIBS) -g  -I../include ../util/m5/build/x86/out/libm5.a -lpthread -pthread
