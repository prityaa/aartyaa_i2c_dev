#include <errno.h>
#include <stddef.h>
// #include <i2c/smbus.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
// #include <i2c/smbus.h>

#define DISPLAY_RGB_ADDR	0x62
#define DISPLAY_TEXT_ADDR	0x3e

#ifdef DEBUG
	#define pr_debug	printf
#else
	#define pr_debug 
#endif

int aartyaa_lcd_write_byte_data(int file, char read_write, __u8 command, 
		int size, char const byte, char const addr)
{

	union i2c_smbus_data data;
	struct i2c_smbus_ioctl_data aartyaa_ioctl_data;
	pr_debug("aartyaa_lcd_write_byte_data : file = %d, read_write = %d, command = %d, size = %d, byte = %x, addr = %x\n", 
			file, read_write, command, size, byte, addr);

	data.byte = byte;	

 	aartyaa_ioctl_data.read_write = read_write;
	aartyaa_ioctl_data.command = command;
	aartyaa_ioctl_data.size = size;
	aartyaa_ioctl_data.data = &data;
	
	if ( ioctl(file, I2C_SLAVE, addr) < 0 ) {
		fprintf(stderr, "Error: could not check address = 0x62"
                        "functionality matrix: %s\n", strerror(errno));
                close(file);
                exit(1);		
	}
	
	return ioctl(file, I2C_SMBUS, &aartyaa_ioctl_data);
}

int main()
{
	int file = -1, ret = -1;
	unsigned long funcs;
	union i2c_smbus_data data;
	char *filename = "/dev/aartyaa_lcd-1";

	if ( (file = open("/dev/aartyaa_lcd-1", O_RDWR ) ) <= 0 ) {
		perror("file :"); 
		return -1;
	}
	pr_debug("main : file = %d\n", file);
	
	if (ioctl(file, I2C_FUNCS, &funcs) < 0) {
                fprintf(stderr, "Error: Could not get the adapter "
                        "functionality matrix: %s\n", strerror(errno));
                close(file);
                exit(1);
        }

	pr_debug("calling first time file  = %d\n", file);
	if ( (ret = aartyaa_lcd_write_byte_data(file, 0, 0, 2, 0, DISPLAY_RGB_ADDR) < 0) ) {
		pr_debug("aartyaa_lcd_write_byte_data failed\n");
		return -1;
 	}
	
	pr_debug("main : aartyaa caaling 2nd write \n" );
	if ( (ret = aartyaa_lcd_write_byte_data(file, 0, 1, 2, 0, DISPLAY_RGB_ADDR) < 0) ) {
		pr_debug("aartyaa_lcd_write_byte_data failed\n");
		return -1;
 	}

	if ( (ret = aartyaa_lcd_write_byte_data(file, 0, 8, 2, 170, DISPLAY_RGB_ADDR) < 0) ) {
		pr_debug("aartyaa_lcd_write_byte_data failed\n");
		return -1;
 	}
	
	if ( (ret = aartyaa_lcd_write_byte_data(file, 0, 4, 2, 0, DISPLAY_RGB_ADDR) < 0) ) {
		pr_debug("aartyaa_lcd_write_byte_data failed\n");
		return -1;
 	}
	
	if ( (ret = aartyaa_lcd_write_byte_data(file, 0, 3, 2, 255, DISPLAY_RGB_ADDR) < 0) ) {
		pr_debug("aartyaa_lcd_write_byte_data failed\n");
		return -1;
 	}

	if ( (ret = aartyaa_lcd_write_byte_data(file, 0, 2, 2, 0, DISPLAY_RGB_ADDR) < 0) ) {
                pr_debug("aartyaa_lcd_write_byte_data failed\n");
                return -1;
        }
		
	if (file > 0)
		close(file);
	
	return 0;
}
	

