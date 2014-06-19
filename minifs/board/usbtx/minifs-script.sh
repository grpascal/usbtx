#!/bin/bash

. "$CONF_BASE"/arch/mxs.sh

TARGET_KERNEL_NAME=zImage

# this file .dts must exist either in this directory (board)
# or in the linux arch/$TARGET_ARCH/boot/dts/
TARGET_KERNEL_DTB=${TARGET_KERNEL_DTB:-imx23-olinuxino-usbtx}
#TARGET_KERNEL_CMDLINE=${TARGET_KERNEL_CMDLINE:-"console=ttyAMA0,115200 rdinit=/linuxrc  quiet"}
TARGET_KERNEL_CMDLINE=${TARGET_KERNEL_CMDLINE:-"console=ttyAMA0,115200 root=/dev/mmcblk0p2 ro rootwait quiet spidev.bufsiz=65535"}

board_set_versions() {
	mxs-set-versions
	TARGET_FS_SQUASH=0
	TARGET_FS_EXT2=1
	TARGET_SHARED=1
	TARGET_INITRD=0
}

board_prepare() {
	TARGET_PACKAGES+=" gdbserver strace"

	TARGET_PACKAGES+=" targettools"
	hset targettools tools "spitalk waitfor_uevent"
	hset targettools depends ""
	
	TARGET_PACKAGES+=" usbtx"

	TARGET_PACKAGES+=" linux-dtb elftosb"
}

imx233-deploy-sharedlibs() {
	deploy-sharedlibs
	mkdir -p "$ROOTFS"/rw
}


BSI_SRC=/opt/bsiuk/src

PACKAGES+=" mptools"
hset mptools url "none"
hset mptools dir "$BSI_SRC/mptools"
hset mptools destdir "$STAGING_USR"
hset mptools depends "libtalloc"

configure-mptools() {
	configure echo Done
}
compile-mptools() {
	rm -f ._compile_mptools*
	local save=$MAKE_ARGUMENTS
	MAKE_ARGUMENTS=
	compile-generic \
		CROSS_COMPILE="$TARGET_FULL_ARCH"- \
		CC="$CC" \
		LDFLAGS="$LDFLAGS_RLINK" \
		CFLAGS="$CFLAGS" V=1
	MAKE_ARGUMENTS=$save
}
install-mptools() {
	install-generic \
		CROSS_COMPILE="$TARGET_FULL_ARCH"- \
		CC="$CC"
}
deploy-mptools() {
	deploy deploy_binaries
}

PACKAGES+=" usbtx"
hset usbtx url "none"
hset usbtx dir "$BSI_SRC/usbtx"
hset usbtx destdir "$STAGING_USR"
hset usbtx depends "mptools libalsa"

configure-usbtx() {
	configure echo Done
}
compile-usbtx() {
	rm -f ._compile_*
	compile-generic \
		CROSS_COMPILE="$TARGET_FULL_ARCH"- \
		CC="$CC" \
		LDFLAGS="$LDFLAGS_RLINK" \
		CFLAGS="$CFLAGS"
}
install-usbtx() {
	install-generic \
		CROSS_COMPILE="$TARGET_FULL_ARCH"- \
		CC="$CC"
}
deploy-usbtx() {
	deploy deploy_binaries
	mkdir -p "$ROOTFS"/etc "$ROOTFS"/www "$ROOTFS"/rw
	#cp -r www/* "$ROOTFS"/www/
	#cp -r etc/* "$ROOTFS"/etc/
#	ln -sf ../tmp/resolv.conf "$ROOTFS"/etc/resolv.conf
}




bard_local() {
	# how to use kexec from the board to launch a new kernel
	tftp -g -r linux -l /tmp//linux 192.168.2.129 &&	kexec --append="$(cat /proc/cmdline)" --force /tmp/linux
	tftp -g -r linux -l /tmp/linux 192.168.2.129 &&	kexec --append="console=ttyAMA0,115200 root=/dev/mmcblk0p2 ro rootwait ssp1=mmc" --force --no-ifdown /tmp/linux

	# how to launch this in qemu
	./arm-softmmu/qemu-system-arm  -M imx233o -m 64M -kernel /opt/bsiuk/src/minifs/build-usbtx/vmlinuz-bare.dtb -monitor telnet::4444,server,nowait -serial stdio -display none -usb -snapshot


	./arm-softmmu/qemu-system-arm  -M imx233o -m 64M -kernel /opt/bsiuk/src/minifs/build-usbtx/vmlinuz-bare.dtb -monitor telnet::4444,server,nowait -serial stdio -display none -usb -snapshot -sd /opt/olimex/basic.img -usbdevice host:05e3:0610

}


