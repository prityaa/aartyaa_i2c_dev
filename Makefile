
obj-m = aartyaa_lcd.o

CFLAGS_aartyaa.o := -DDEBUG
CROSS_COMPILE=arm-linux-gnueabihf-
RPI_COMPILE_OPTION = ARCH=arm CROSS_COMPILE=$(CROSS_COMPILE)
APP_SOURCE_CODE = aartyaa_lcd_app.c
COMPILER = gcc
APP_EXE = AARTYAA_LCD_APP
OUTPUT_FLAG = -o

KERNEL_DIR = /home/prityaa/documents/workspace/embeded/raspbery_pi/code_base/i2c_groove_lcd/codebase/aartyaa_lcd/linux

all:
	$(MAKE) -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

rpi:
	$(MAKE) $(RPI_COMPILE_OPTION) -C $(KERNEL_DIR) M=$(PWD) modules
	$(CROSS_COMPILE)$(COMPILER) -DDEBUG $(OUTPUT_FLAG) $(APP_EXE) $(APP_SOURCE_CODE) 

rpi_clean:
	$(MAKE) $(RPI_COMPILE_OPTION) -C $(KERNEL_DIR) M=$(PWD) clean
	rm -f $(APP_EXE)

clean:
	$(MAKE) -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
        #$(MAKE) -C $(KERNEL_DIR) M=$(PWD) clean
