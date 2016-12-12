#ifndef __AARTYAA_LCD_H__
#define __AARTYAA_LCD_H__

#define DRIVER_NAME "aartyaa_lcd"
#define MAJOR_NUM	255
#define MINORS_NUM      256

struct aartyaa_lcd_dev {
        struct list_head list;
        struct i2c_adapter *adap;
        struct device *dev;
};

struct class *aartyaa_lcd_class;
static LIST_HEAD(i2c_dev_list);
static DEFINE_SPINLOCK(i2c_dev_list_lock);
int aartyaa_lcd_dev_check_mux_parents(struct i2c_adapter *adapter, int addr);


#endif
