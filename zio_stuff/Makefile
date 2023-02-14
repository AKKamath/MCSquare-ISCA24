.PHONY: mount_img umount_img 

GEM5_IMG ?= ./zio-test.img
GEM5_CPU ?= X86KvmCPU
GEM5_KERNEL ?= ./vmlinux-4.19.83
GEM5_SCRIPT ?= ./scripts/run-script_copy.sh

LDLIBS = -lrt

mount_img:
	../util/gem5img.py mount ${GEM5_IMG} mnt; \
	sudo /bin/mount -o bind /sys mnt/sys; \
	sudo /bin/mount -o bind /dev mnt/dev; \
	sudo /bin/mount -o bind /proc mnt/proc; \
	sudo /usr/sbin/chroot mnt /bin/bash;

umount_img:
	sudo /bin/umount mnt/sys; \
	sudo /bin/umount mnt/proc; \
	sudo /bin/umount mnt/dev; \
	../util/gem5img.py umount mnt;

launch_gem5:
	sudo ../build/X86/gem5.opt --debug-flags=Exec -d ./results/ ../configs/example/fs.py --mem-size=8GB --cpu-type=${GEM5_CPU} --kernel=${GEM5_KERNEL} --disk-image=${GEM5_IMG} -n 4 --script=${GEM5_SCRIPT}

copy_sweep: copy_sweep.cpp
	g++ -o $@ $^ $(LDLIBS) -g  -I../include ../util/m5/build/x86/out/libm5.a -lpthread -pthread
