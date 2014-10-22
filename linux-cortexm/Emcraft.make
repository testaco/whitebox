# 
# This file contains common make rules for all projects.
#

MAKE		:= make

KERNEL_CONFIG	:= kernel$(if $(MCU),.$(MCU))
KERNEL_LD	:= lds$(if $(MCU),.$(MCU))
RAMFS_CONFIG	:= initramfs
KERNEL_IMAGE	:= uImage
KERNEL_VIMAGE	:= vImage
ENVM_LOG	:= .envm_log

# Path to the kernel modules directory in context of which
# these loadable modules are built
KERNELDIR	:=  $(INSTALL_ROOT)/linux

CFLAGS		:= "-Os -mcpu=cortex-m3 -mthumb -I$(INSTALL_ROOT)/A2F/root/usr/include"
LDFLAGS		:= "-mcpu=cortex-m3 -mthumb -L$(INSTALL_ROOT)/A2F/root/usr/lib"

.PHONY	: all busybox linux kmenuconfig bmenuconfig clean kclean bclean aclean $(CUSTOM_APPS) clone

all		: _do_modules linux

# For those projects that have support for loadable kernel modules
# enabled in the kernel configuration, we need to build and install
# modules in the kernel tree, as a first step in building the project.
# This is needed to allow us building an external module (or several
# such modules) as an external module from a project subdirectory
# and then to include the resultant module object in the project
# initramfs filesystem.

MODULES_ON	:= $(shell grep CONFIG_MODULES=y $(SAMPLE).$(KERNEL_CONFIG))
INSTALL_MOD_PATH:= $(INSTALL_ROOT)/linux

ifeq ($(MODULES_ON),)
_do_modules 	:
else
_do_modules	: _prepare_modules
endif

_prepare_modules:
	cp -f $(INSTALL_ROOT)/linux/initramfs-list-min.stub \
		$(INSTALL_ROOT)/linux/initramfs-list-min
	rm -f $(INSTALL_ROOT)/linux/usr/initramfs_data.cpio \
		$(INSTALL_ROOT)/linux/usr/initramfs_data.cpio.gz
	cp -f $(SAMPLE).$(KERNEL_CONFIG) $(INSTALL_ROOT)/linux/.config
	$(MAKE) -C $(INSTALL_ROOT)/linux vmlinux
	$(MAKE) -C $(INSTALL_ROOT)/linux modules

linux		: $(SAMPLE).$(KERNEL_IMAGE)

$(CUSTOM_APPS)	:
	 CFLAGS=${CFLAGS} LDFLAGS=${LDFLAGS} make -C $@ all

clean		: kclean bclean aclean
	rm -rf $(SAMPLE).$(KERNEL_IMAGE) $(SAMPLE).$(KERNEL_VIMAGE) \
		$(ENVM_LOG) busybox

kclean		:
	$(MAKE) -C $(INSTALL_ROOT)/linux clean

bclean		:
	$(MAKE) -C $(INSTALL_ROOT)/A2F/busybox clean

aclean		:
	@[ "x$(CUSTOM_APPS)" = "x" ] || \
		for i in $(CUSTOM_APPS); do \
			$(MAKE) -C $$i clean; \
		done

kmenuconfig	:
	cp -f $(SAMPLE).$(KERNEL_CONFIG) \
			$(INSTALL_ROOT)/linux/.config
	$(MAKE) -C $(INSTALL_ROOT)/linux menuconfig
	cp -f $(INSTALL_ROOT)/linux/.config \
			./$(SAMPLE).$(KERNEL_CONFIG)

bmenuconfig	:
	cp -f $(SAMPLE).busybox $(INSTALL_ROOT)/A2F/busybox/.config
	$(MAKE) -C $(INSTALL_ROOT)/A2F/busybox menuconfig
	cp -f $(INSTALL_ROOT)/A2F/busybox/.config $(SAMPLE).busybox

busybox		: $(SAMPLE).busybox
	cp -f $(SAMPLE).busybox $(INSTALL_ROOT)/A2F/busybox/.config
	$(MAKE) -C $(INSTALL_ROOT)/A2F/busybox
#	cp -f $(INSTALL_ROOT)/A2F/busybox/busybox $@

%.$(KERNEL_IMAGE) : %.$(KERNEL_CONFIG) %.$(RAMFS_CONFIG) $(CUSTOM_APPS) busybox
	cp -f $(SAMPLE).$(RAMFS_CONFIG) $(INSTALL_ROOT)/linux/initramfs-list-min
	rm -f $(INSTALL_ROOT)/linux/usr/initramfs_data.cpio \
		$(INSTALL_ROOT)/linux/usr/initramfs_data.cpio.gz
	cp -f $(SAMPLE).$(KERNEL_CONFIG) $(INSTALL_ROOT)/linux/.config
	cp -f $(INSTALL_ROOT)/linux/arch/arm/kernel/vmlinux.lds.S.good \
		$(INSTALL_ROOT)/linux/arch/arm/kernel/vmlinux.lds.S
	([ -e $(SAMPLE).$(KERNEL_LD) ] && \
		cp -f $(SAMPLE).$(KERNEL_LD) \
		$(INSTALL_ROOT)/linux/arch/arm/kernel/vmlinux.lds.S) || \
	true;
	$(MAKE) -C $(INSTALL_ROOT)/linux $(KERNEL_IMAGE) SAMPLE=${SAMPLE}
	cp -f $(INSTALL_ROOT)/linux/arch/arm/boot/$(KERNEL_IMAGE) $@
	rm -f $(ENVM_LOG)
	(grep "CONFIG_KERNEL_IN_ENVM=y" $(SAMPLE).$(KERNEL_CONFIG) > \
		$(ENVM_LOG) || true)
	([ -s $(ENVM_LOG) ] && \
	($(MAKE) -C $(INSTALL_ROOT)/linux $(KERNEL_VIMAGE) SAMPLE=${SAMPLE}; \
	cp -f $(INSTALL_ROOT)/linux/arch/arm/boot/$(KERNEL_VIMAGE) \
		$(SAMPLE).$(KERNEL_VIMAGE))) || \
	true;
	rm -f $(ENVM_LOG)

clone		:
	@[ ! -z ${new} ] || \
	(echo "Please specify the new project name (\"make clone new=...\")";\
		 exit 1);
	@[ ! -d $(INSTALL_ROOT)/projects/${new} ] || \
		(echo \
		"Project $(INSTALL_ROOT)/projects/${new} already exists!"; \
		 exit 1);
	@mkdir -p $(INSTALL_ROOT)/projects/${new}
	@cp -a .  $(INSTALL_ROOT)/projects/${new}
	@for i in ${KERNEL_CONFIG} ${RAMFS_CONFIG} busybox; do \
		mv $(INSTALL_ROOT)/projects/${new}/${SAMPLE}.$$i \
			$(INSTALL_ROOT)/projects/${new}/${new}.$$i; \
	done
	@sed 's/SAMPLE.*\:=.*/SAMPLE\t\t:= ${new}/' Makefile > \
		$(INSTALL_ROOT)/projects/${new}/Makefile
	@echo "New project created in $(INSTALL_ROOT)/projects/${new}"
