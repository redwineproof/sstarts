obj-m += sstarts.o

all:
	$(MAKE) -C $(@D) ARCH=$(BR2_ARCH) CROSS_COMPILE=$(TARGET_CROSS) \
        KERNELDIR=$(LINUX_DIR) modules
	
clean:
	$(MAKE) -C $(@D) ARCH=$(BR2_ARCH) CROSS_COMPILE=$(TARGET_CROSS) \
        KERNELDIR=$(LINUX_DIR) clean