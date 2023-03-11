# Makefile for driver
# Derived from: http://www.opensourceforu.com/2010/12/writing-your-first-linux-driver/
# with some settings from Robert Nelson's BBB kernel build script
# modified by Brian Fraser

# if KERNELRELEASE is defined, we've been invoked from the
# kernel build system and can use its variables.

ifneq (${KERNELRELEASE},)
	obj-m := morse-code.o

# Otherwise we were called directly from the command line.
# Invoke the kernel build system.
else
	KERNEL_SOURCE := ~/cmpt433/work/linux/
	PWD := $(shell pwd)
	CC=arm-linux-gnueabihf-

	CORES=4
	PUBLIC_DRIVER_PWD=~/cmpt433/public/drivers

default:
	# Trigger kernel build for this module
	${MAKE} -C ${KERNEL_SOURCE} M=${PWD} -j${CORES} ARCH=arm CROSS_COMPILE=${CC} ${address} ${image} modules
	#
	# copy result to public folder
	mkdir -p ${PUBLIC_DRIVER_PWD}
	cp *.ko ${PUBLIC_DRIVER_PWD}
	cp *.sh ${PUBLIC_DRIVER_PWD}

all: default

clean:
	${MAKE} -C ${KERNEL_SOURCE} M=${PWD} clean

endif
