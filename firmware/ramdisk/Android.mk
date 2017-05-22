# Build script to generate easel ramdisk

include $(CLEAR_VARS)

LOCAL_MODULE := easel_ramdisk.img
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH := $(TARGET_OUT_VENDOR)/firmware/easel
LOCAL_MODULE_STEM := ramdisk.img
LOCAL_MODULE_OWNER := google

# TODO(cjluo): Add notice file before launch.

include $(BUILD_SYSTEM)/base_rules.mk

EASEL_RAMDISK_SRC_DIR := vendor/google_paintbox/firmware/ramdisk

PREBUILT_BIN_MODULES := \
	$(EASEL_RAMDISK_SRC_DIR)/toybox \
	$(EASEL_RAMDISK_SRC_DIR)/sh

BIN_MODULES := \
	$(call intermediates-dir-for,EXECUTABLES,linker)/linker64 \
	$(call intermediates-dir-for,EXECUTABLES,ezlsh)/ezlsh \
	$(call intermediates-dir-for,EXECUTABLES,pbserver)/pbserver \
	$(call intermediates-dir-for,EXECUTABLES,logd.easel)/logd.easel \
	$(call intermediates-dir-for,EXECUTABLES,crash_dump)/crash_dump64

LIB_MODULES := \
	$(call intermediates-dir-for,SHARED_LIBRARIES,libbacktrace)/libbacktrace.so \
	$(call intermediates-dir-for,SHARED_LIBRARIES,libbase)/libbase.so \
	$(call intermediates-dir-for,SHARED_LIBRARIES,libc++)/libc++.so \
	$(call intermediates-dir-for,SHARED_LIBRARIES,libc)/libc.so \
	$(call intermediates-dir-for,SHARED_LIBRARIES,libc_malloc_debug)/libc_malloc_debug.so \
	$(call intermediates-dir-for,SHARED_LIBRARIES,libcutils)/libcutils.so \
	$(call intermediates-dir-for,SHARED_LIBRARIES,libdl)/libdl.so \
	$(call intermediates-dir-for,SHARED_LIBRARIES,libeaselcontrolservice)/libeaselcontrolservice.so \
	$(call intermediates-dir-for,SHARED_LIBRARIES,liblzma)/liblzma.so \
	$(call intermediates-dir-for,SHARED_LIBRARIES,libm)/libm.so \
	$(call intermediates-dir-for,SHARED_LIBRARIES,libunwind)/libunwind.so \
	$(call intermediates-dir-for,SHARED_LIBRARIES,libutils)/libutils.so \
	$(call intermediates-dir-for,SHARED_LIBRARIES,libz)/libz.so \
	$(call intermediates-dir-for,SHARED_LIBRARIES,libcapture)/libcapture.so \
	$(call intermediates-dir-for,SHARED_LIBRARIES,libgcam)/libgcam.so \
	$(call intermediates-dir-for,SHARED_LIBRARIES,libhdrplusmessenger)/libhdrplusmessenger.so \
	$(call intermediates-dir-for,SHARED_LIBRARIES,libhdrplusservice)/libhdrplusservice.so \
	$(call intermediates-dir-for,SHARED_LIBRARIES,libimageprocessor)/libimageprocessor.so \
	$(call intermediates-dir-for,SHARED_LIBRARIES,libmipimux)/libmipimux.so \
	$(call intermediates-dir-for,SHARED_LIBRARIES,libeaselcomm)/libeaselcomm.so \
	$(call intermediates-dir-for,SHARED_LIBRARIES,liblog)/liblog.so \
	$(call intermediates-dir-for,SHARED_LIBRARIES,libnl)/libnl.so \
	$(call intermediates-dir-for,SHARED_LIBRARIES,libsysutils)/libsysutils.so \
	$(call intermediates-dir-for,SHARED_LIBRARIES,libprocinfo)/libprocinfo.so

EASEL_PCG_DIR := vendor/google_paintbox/prebuilts/compiled_graph/

PCG_MODULES := $(wildcard $(EASEL_PCG_DIR)/*)

EASEL_RAMDISK_TOOL_DIR := vendor/google_paintbox/firmware/tools/

GEN_CPIO := $(HOST_OUT_EXECUTABLES)/gen_init_cpio
GEN_INITRAMFS_LIST := $(EASEL_RAMDISK_TOOL_DIR)/gen_initramfs_list.sh
MKIMAGE := $(EASEL_RAMDISK_TOOL_DIR)/mkimage

$(LOCAL_BUILT_MODULE): TOYBOX_ALL_TOOLS := \
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

$(LOCAL_BUILT_MODULE): RAMDISK_PHYS_ADDR := 0x40890000

$(LOCAL_BUILT_MODULE): PRIVATE_GEN_CPIO := $(GEN_CPIO)
$(LOCAL_BUILT_MODULE): PRIVATE_GEN_INITRAMFS_LIST := $(GEN_INITRAMFS_LIST)
$(LOCAL_BUILT_MODULE): PRIVATE_MKIMAGE := $(MKIMAGE)

$(LOCAL_BUILT_MODULE): PRIVATE_EASEL_RAMDISK_SRC_DIR := $(EASEL_RAMDISK_SRC_DIR)
$(LOCAL_BUILT_MODULE): PRIVATE_PREBUILT_BIN_MODULES := $(PREBUILT_BIN_MODULES)
$(LOCAL_BUILT_MODULE): PRIVATE_BIN_MODULES := $(BIN_MODULES)
$(LOCAL_BUILT_MODULE): PRIVATE_LIB_MODULES := $(LIB_MODULES)
$(LOCAL_BUILT_MODULE): PRIVATE_PCG_MODULES := $(PCG_MODULES)

$(LOCAL_BUILT_MODULE): EASEL_PREBUILT := prebuilt/
$(LOCAL_BUILT_MODULE): EASEL_BIN := prebuilt/system/bin/
$(LOCAL_BUILT_MODULE): EASEL_LIB := prebuilt/system/lib64/
$(LOCAL_BUILT_MODULE): EASEL_PCG := prebuilt/data/paintbox/compiled_graph/

$(LOCAL_BUILT_MODULE): \
	$(PREBUILT_BIN_MODULES) \
	$(BIN_MODULES) \
	$(LIB_MODULES) \
	$(PCG_MODULES) \
	$(GEN_CPIO) $(GEN_INITRAMFS_LIST) $(MKIMAGE) \
	$(EASEL_RAMDISK_SRC_DIR)/files.txt \
	$(EASEL_RAMDISK_SRC_DIR)/init

	@rm -rf $(dir $@)
	@mkdir -p $(dir $@)

	# files.txt
	@cp -f $(PRIVATE_EASEL_RAMDISK_SRC_DIR)/files.txt $(dir $@)

	# Pre-generate symlinks for toybox
	@echo "dir /system 755 0 0" >> $(dir $@)/files.txt
	@echo "dir /system/bin 755 0 0" >> $(dir $@)/files.txt
	@echo "dir /system/sbin 755 0 0" >> $(dir $@)/files.txt
	@echo "dir /system/usr 755 0 0" >> $(dir $@)/files.txt
	@echo "dir /system/usr/bin 755 0 0" >> $(dir $@)/files.txt
	@echo "dir /system/usr/sbin 755 0 0" >> $(dir $@)/files.txt
	$(foreach tool, $(TOYBOX_ALL_TOOLS),\
		echo "slink /system/$(tool) /system/bin/toybox 777 0 0" >> $(dir $@)/files.txt;)

	# init
	@mkdir -p $(dir $@)/$(EASEL_PREBUILT)
	@cp -f $(PRIVATE_EASEL_RAMDISK_SRC_DIR)/init $(dir $@)/$(EASEL_PREBUILT)

	# Binary
	@mkdir -p $(dir $@)/$(EASEL_BIN)
	$(foreach module, $(PRIVATE_BIN_MODULES),\
		cp -f $(module) $(dir $@)/$(EASEL_BIN) &&) (true)
	$(foreach module, $(PRIVATE_PREBUILT_BIN_MODULES),\
		cp -f $(module) $(dir $@)/$(EASEL_BIN) &&) (true)

	$(TARGET_STRIP) $(dir $@)/$(EASEL_BIN)/*

	# Library
	@mkdir -p $(dir $@)/$(EASEL_LIB)

	$(foreach module, $(PRIVATE_LIB_MODULES),\
		cp -f $(module) $(dir $@)/$(EASEL_LIB);)

	@mv -f $(dir $@)/$(EASEL_LIB)/libeaselcontrolservice.so $(dir $@)/$(EASEL_LIB)/libeaselcontrol.so

	$(TARGET_STRIP) $(dir $@)/$(EASEL_LIB)/*

	# PCG
	@mkdir -p $(dir $@)/$(EASEL_PCG)
	$(foreach pcg, $(PRIVATE_PCG_MODULES),\
		cp -rf $(pcg) $(dir $@)/$(EASEL_PCG) &&) (true)

	$(PRIVATE_GEN_INITRAMFS_LIST) $(dir $@)/$(EASEL_PREBUILT) >> $(dir $@)/files.txt

	$(PRIVATE_GEN_CPIO) $(dir $@)/files.txt > $(dir $@)/initramfs.cpio

	$(PRIVATE_MKIMAGE) $(dir $@)/initramfs.cpio $@
