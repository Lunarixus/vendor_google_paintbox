# Build script to generate easel ramdisk

include $(CLEAR_VARS)

PREBUILT_BIN_MODULES := \
	toybox \
	sh

BIN_MODULES := \
	linker \
	ezlsh \
	pbserver

SYSTEM_LIB_MODULES := \
	libbacktrace \
	libbase \
	libc++ \
	libc \
	libc_malloc_debug \
	libcutils \
	libdl \
	liblzma \
	libm \
	libunwind \
	libutils \
	libz

VENDOR_LIB_MODULES := \
	libcapture \
	libgcam \
	libhdrplusmessenger \
	libhdrplusservice \
	libimageprocessor \
	libmipimux \
	libeasellog \
	libeaselcomm \
	libeaselcontrolservice

TOYBOX_ALL_TOOLS := \
	usr/bin/acpi usr/bin/base64 usr/bin/basename bin/blkid usr/bin/blockdev \
	usr/bin/bunzip2 usr/bin/bzcat usr/bin/cal bin/cat bin/chattr bin/chgrp \
	bin/chmod bin/chown usr/sbin/chroot usr/sbin/chrt bin/cksum usr/bin/clear \
	usr/bin/cmp usr/bin/comm bin/cp bin/cpio usr/bin/cut bin/date usr/bin/dd \
	sbin/df usr/bin/dirname bin/dmesg bin/dos2unix usr/bin/du bin/echo \
	bin/egrep usr/bin/env usr/bin/expand usr/bin/expr usr/bin/fallocate \
	bin/false bin/fgrep usr/bin/file usr/bin/find usr/bin/flock usr/bin/free \
	sbin/freeramdisk usr/sbin/fsfreeze usr/bin/getfattr bin/grep usr/bin/groups \
	usr/bin/head bin/help bin/hostname usr/bin/hwclock usr/bin/id sbin/ifconfig \
	usr/bin/inotifyd sbin/insmod usr/bin/install usr/bin/ionice usr/bin/iorenice \
	bin/kill usr/bin/killall bin/ln usr/bin/logname sbin/losetup bin/ls \
	bin/lsattr sbin/lsmod usr/bin/lsof usr/bin/lsusb usr/bin/makedevs \
	usr/bin/md5sum bin/microcom bin/mkdir usr/bin/mkfifo bin/mknod sbin/mkswap \
	bin/mktemp bin/modinfo sbin/modprobe usr/bin/more bin/mount bin/mountpoint \
	bin/mv usr/bin/nbd-client usr/bin/nc bin/netcat bin/netstat usr/bin/nice \
	bin/nl usr/bin/nohup usr/bin/od sbin/partprobe bin/paste usr/bin/patch \
	usr/bin/pgrep bin/pidof sbin/pivot_root usr/bin/pkill bin/pmap usr/bin/printenv \
	usr/bin/printf usr/bin/ps bin/pwd usr/bin/pwdx usr/bin/readlink usr/bin/realpath \
	usr/bin/renice usr/bin/rev usr/sbin/rfkill bin/rm bin/rmdir sbin/rmmod \
	usr/bin/sed usr/bin/seq usr/bin/setfattr usr/bin/setsid usr/bin/sha1sum \
	usr/bin/sha224sum usr/bin/sha256sum usr/bin/sha384sum usr/bin/sha512sum \
	bin/sleep usr/bin/sort usr/bin/split bin/stat usr/bin/strings sbin/swapoff \
	sbin/swapon bin/sync sbin/sysctl usr/bin/tac usr/bin/tail usr/bin/tar \
	bin/taskset usr/bin/tee usr/bin/time bin/timeout usr/bin/top bin/touch \
	usr/bin/tr usr/bin/traceroute usr/bin/traceroute6 bin/true bin/truncate \
	usr/bin/tty usr/bin/tunctl usr/bin/ulimit bin/umount bin/uname usr/bin/uniq \
	bin/unix2dos usr/bin/uptime bin/usleep usr/bin/uudecode usr/bin/uuencode \
	sbin/vconfig bin/vmstat usr/bin/wc usr/bin/which usr/bin/whoami usr/bin/xargs \
	usr/bin/xxd usr/bin/yes

EASEL_RAMDISK_SRC_DIR := vendor/google_paintbox/firmware/ramdisk/
EASEL_RAMDISK_TOOL_DIR := prebuilts/google/paintbox/tools/
EASEL_PCG_DIR := vendor/google_paintbox/prebuilts/compiled_graph/
EASEL_RAMDISK_DIR := $(TARGET_OUT_INTERMEDIATES)/easel/ramdisk/
EASEL_RAMDISK_PREBUILT_DIR := $(EASEL_RAMDISK_DIR)/prebuilt/
EASEL_RAMDISK_BIN_DIR := $(EASEL_RAMDISK_PREBUILT_DIR)/system/bin/
EASEL_RAMDISK_LIB_DIR := $(EASEL_RAMDISK_PREBUILT_DIR)/system/lib64/
EASEL_RAMDISK_PCG_DIR := $(EASEL_RAMDISK_PREBUILT_DIR)/data/paintbox/compiled_graph/
EASEL_RAMDISK_INSTALL := $(TARGET_OUT_VENDOR)/firmware/easel/

GEN_CPIO=$(EASEL_RAMDISK_TOOL_DIR)/gen_init_cpio
GEN_INITRAMFS_LIST=$(EASEL_RAMDISK_TOOL_DIR)/gen_initramfs_list.sh
MKIMAGE=$(EASEL_RAMDISK_TOOL_DIR)/mkimage

RAMDISK_PHYS_ADDR=0x40890000

.PHONY: easel_ramdisk
easel_ramdisk: $(ACP) $(MKBOOTFS) $(BIN_MODULES) $(SYSTEM_LIB_MODULES) $(VENDOR_LIB_MODULES)
	@echo "Building easel ramdisk"
	@rm -rf $(EASEL_RAMDISK_DIR)
	@mkdir -p $(EASEL_RAMDISK_DIR)

	# files.txt
	$(ACP) -fp $(EASEL_RAMDISK_SRC_DIR)/files.txt $(EASEL_RAMDISK_DIR)

	# Pre-generate symlinks for toybox
	@echo "dir /system 755 0 0" >> $(EASEL_RAMDISK_DIR)/files.txt
	@echo "dir /system/bin 755 0 0" >> $(EASEL_RAMDISK_DIR)/files.txt
	@echo "dir /system/sbin 755 0 0" >> $(EASEL_RAMDISK_DIR)/files.txt
	@echo "dir /system/usr 755 0 0" >> $(EASEL_RAMDISK_DIR)/files.txt
	@echo "dir /system/usr/bin 755 0 0" >> $(EASEL_RAMDISK_DIR)/files.txt
	@echo "dir /system/usr/sbin 755 0 0" >> $(EASEL_RAMDISK_DIR)/files.txt
	$(foreach tool, $(TOYBOX_ALL_TOOLS),\
		echo "slink /system/$(tool) /system/bin/toybox 777 0 0" >> $(EASEL_RAMDISK_DIR)/files.txt;)

	# init
	@mkdir -p $(EASEL_RAMDISK_PREBUILT_DIR)
	$(ACP) -fp $(EASEL_RAMDISK_SRC_DIR)/init $(EASEL_RAMDISK_PREBUILT_DIR)

	# Binary
	@mkdir -p $(EASEL_RAMDISK_BIN_DIR)
	$(ACP) -fp $(TARGET_OUT)/bin/linker64 $(EASEL_RAMDISK_BIN_DIR)
	$(ACP) -fp $(TARGET_OUT_VENDOR)/bin/ezlsh $(EASEL_RAMDISK_BIN_DIR)
	@mv -f $(TARGET_OUT_VENDOR)/bin/pbserver $(EASEL_RAMDISK_BIN_DIR)

	$(foreach module, $(PREBUILT_BIN_MODULES),\
		$(ACP) -fp $(EASEL_RAMDISK_SRC_DIR)/$(module) $(EASEL_RAMDISK_BIN_DIR);)

	# Library
	@mkdir -p $(EASEL_RAMDISK_LIB_DIR)

	$(foreach module, $(SYSTEM_LIB_MODULES),\
		$(ACP) -fp $(TARGET_OUT)/lib64/$(module).so $(EASEL_RAMDISK_LIB_DIR);)

	$(foreach module, $(VENDOR_LIB_MODULES),\
		$(ACP) -fp $(TARGET_OUT_VENDOR)/lib64/$(module).so $(EASEL_RAMDISK_LIB_DIR);)

	@mv -f $(EASEL_RAMDISK_LIB_DIR)/libeasellog.so $(EASEL_RAMDISK_LIB_DIR)/liblog.so
	@mv -f $(EASEL_RAMDISK_LIB_DIR)/libeaselcontrolservice.so $(EASEL_RAMDISK_LIB_DIR)/libeaselcontrol.so

	$(foreach file, $(wildcard $(EASEL_RAMDISK_BIN_DIR)/*),\
		$(TARGET_STRIP) $(file);)

	$(foreach file, $(wildcard $(EASEL_RAMDISK_LIB_DIR)/*),\
		$(TARGET_STRIP) $(file);)

	# PCG
	@mkdir -p $(EASEL_RAMDISK_PCG_DIR)
	$(ACP) -rfp $(EASEL_PCG_DIR)/* $(EASEL_RAMDISK_PCG_DIR)

	$(GEN_INITRAMFS_LIST) $(EASEL_RAMDISK_PREBUILT_DIR) >> $(EASEL_RAMDISK_DIR)/files.txt

	${GEN_CPIO} $(EASEL_RAMDISK_DIR)/files.txt > $(EASEL_RAMDISK_DIR)/initramfs.cpio

	${MKIMAGE} -A arm64 -O linux -C none -T ramdisk \
		-n ramdisk -a $(RAMDISK_PHYS_ADDR) -e $(RAMDISK_PHYS_ADDR) -n "Easel initramfs" \
		-d $(EASEL_RAMDISK_DIR)/initramfs.cpio $(EASEL_RAMDISK_DIR)/ramdisk.img

	$(ACP) -fp $(EASEL_RAMDISK_DIR)/ramdisk.img $(EASEL_RAMDISK_INSTALL)/ramdisk.img
