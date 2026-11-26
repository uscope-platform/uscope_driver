obj-m += mock_module.o
mock_module-y := mock_main.o mock_chrdev.o mock_dma.o mock_fclk.o mock_fpga.o mock_rpu.o
ccflags-y := -std=gnu99

SRC := $(shell pwd)

all:
	$(MAKE) -C $(KERNEL_SRC) M=$(SRC)

clean:
	rm -f *.o *~ core .depend .*.cmd *.ko *.mod.c *.mod
	rm -f Module.markers Module.symvers modules.order
	rm -rf .tmp_versions Modules.symvers
