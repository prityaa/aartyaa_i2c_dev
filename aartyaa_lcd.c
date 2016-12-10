
/** 
    aartyaa groove lcd driver code 
    for noraml use of the lcd 
**/
#define DEBUG

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/notifier.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <linux/jiffies.h>
#include <linux/uaccess.h>

#include "aartyaa_grove_lcd.h"

static struct aartyaa_lcd_dev *get_free_i2c_dev(struct i2c_adapter *adap)
{               
        struct aartyaa_lcd_dev *i2c_dev;
 
        if ( adap->nr >= MINORS_NUM ) {
                pr_debug(KERN_ERR "i2c-dev: Out of device minors (%d)\n",
                       adap->nr);
                return ERR_PTR(-ENODEV);
        }
                
        // i2c_dev = kzalloc(sizeof(struct *aartyaa_lcd_dev), GFP_KERNEL);
        if ( !(i2c_dev = kzalloc(sizeof(struct aartyaa_lcd_dev), GFP_KERNEL) )) 
                return ERR_PTR(-ENOMEM);

        i2c_dev->adap = adap;
 
        spin_lock(&i2c_dev_list_lock);
        list_add_tail(&i2c_dev->list, &i2c_dev_list);
        spin_unlock(&i2c_dev_list_lock);
        return i2c_dev;
}

/*
 * routine for creating the device 
 */

static void return_aartyaa_lcd_dev(struct aartyaa_lcd_dev *i2c_dev)
{       
        spin_lock(&i2c_dev_list_lock);
        list_del(&i2c_dev->list);
        spin_unlock(&i2c_dev_list_lock);
        kfree(i2c_dev);
}

static struct aartyaa_lcd_dev *aartyaa_lcd_dev_get_by_minor(unsigned index)
{
        struct aartyaa_lcd_dev *i2c_dev;

        spin_lock(&i2c_dev_list_lock);
        list_for_each_entry(i2c_dev, &i2c_dev_list, list) {
                if (i2c_dev->adap->nr == index)
                        goto found;
        }
        i2c_dev = NULL;
found:
        spin_unlock(&i2c_dev_list_lock);
        return i2c_dev;
}

int aartyaa_lcd_open (struct inode *inode, struct file *file)
{
	unsigned int minor = iminor(inode);
        struct i2c_client *client;
        struct i2c_adapter *adap;
        struct aartyaa_lcd_dev *i2c_dev;

	pr_debug("aartyaa_lcd_open : minor = %d\n", minor);
        if ( !(i2c_dev = aartyaa_lcd_dev_get_by_minor(minor)) )
                return -ENODEV;

        if ( !(adap = i2c_get_adapter(i2c_dev->adap->nr)) )
                return -ENODEV;

        /* This creates an anonymous i2c_client, which may later be
         * pointed to some address using I2C_SLAVE or I2C_SLAVE_FORCE.
         *
         * This client is ** NEVER REGISTERED ** with the driver model
         * or I2C core code!!  It just holds private copies of addressing
         * information and maybe a PEC flag.
         */
        client = kzalloc(sizeof(*client), GFP_KERNEL);
        if ( !(client = kzalloc(sizeof(*client), GFP_KERNEL)) ) {
                i2c_put_adapter(adap);
                return -ENOMEM;
        }
	
        snprintf(client->name, I2C_NAME_SIZE, "aartyaa_lcd %d", adap->nr);
	pr_debug("aartyaa_lcd_open : adap->nr = %d, client->name = %s\n", adap->nr, client->name);
	pr_debug("aartyaa_lcd_open : aartyaa checling\n");

        client->adapter = adap;
        file->private_data = client;

        return 0;
}

static int aartyaa_lcd_release(struct inode *inode, struct file *file)
{
        struct i2c_client *client = file->private_data;

        i2c_put_adapter(client->adapter);
        kfree(client);
        file->private_data = NULL;

        return 0;
}

static ssize_t aartyaa_lcd_read(struct file *file, char __user *buf, 
				size_t count, loff_t *offset)
{
        char *tmp;
        int ret;

        struct i2c_client *client = file->private_data;

        pr_debug("i2cdev_read : aartyaa calling read\n");
        if (count > 8192)
                count = 8192;

        tmp = kmalloc(count, GFP_KERNEL);
        if (tmp == NULL)
                return -ENOMEM;

        pr_debug("iartyaa_lcd_read : with pr_debug , i2c-dev: i2c-%d reading %zu bytes.\n",
                iminor(file_inode(file)), count);

        if ( (ret = i2c_master_recv(client, tmp, count)) >= 0)
                ret = copy_to_user(buf, tmp, count) ? -EFAULT : ret;

        kfree(tmp);
        return ret;
}

static ssize_t aartyaa_lcd_write(struct file *file, const char __user *buf,
                size_t count, loff_t *offset)
{
        int ret;
        char *tmp;
        struct i2c_client *client = file->private_data;

        pr_debug("i2cdev_write : aartyaa calling the write fun \n");
        pr_debug("i2cdev_write : buf = %s\n", buf);

        if (count > 8192)
                count = 8192;

        tmp = memdup_user(buf, count);
        if (IS_ERR(tmp))
                return PTR_ERR(tmp);

        pr_debug("i2c-dev: i2c-%d writing %zu bytes.\n",
                iminor(file_inode(file)), count);

        ret = i2c_master_send(client, tmp, count);
        kfree(tmp);
        return ret;
}

static int i2cdev_check(struct device *dev, void *addrp)
{
        struct i2c_client *client = i2c_verify_client(dev);

        if (!client || client->addr != *(unsigned int *)addrp)
                return 0;

        return dev->driver ? -EBUSY : 0;
}

/* walk up mux tree */
static int aartyaa_lcd_check_mux_parents(struct i2c_adapter *adapter, int addr)
{
        struct i2c_adapter *parent = i2c_parent_is_i2c_adapter(adapter);
        int result; 

        result = device_for_each_child(&adapter->dev, &addr, i2cdev_check);
        if (!result && parent)
                result = aartyaa_lcd_dev_check_mux_parents(parent, addr);
        
        return result; 
}

/* walk up mux tree */
int aartyaa_lcd_dev_check_mux_parents(struct i2c_adapter *adapter, int addr)
{
        struct i2c_adapter *parent = i2c_parent_is_i2c_adapter(adapter);
        int result;
        
        result = device_for_each_child(&adapter->dev, &addr, i2cdev_check);
        if (!result && parent)
                result = aartyaa_lcd_check_mux_parents(parent, addr);

        return result;
}


static int aartyaa_lcd_dev_check(struct device *dev, void *addrp)
{
        struct i2c_client *client = i2c_verify_client(dev);

        if (!client || client->addr != *(unsigned int *)addrp)
                return 0;
 
        return dev->driver ? -EBUSY : 0;
}

/* recurse down mux tree */
static int aartyaa_lcd_dev_check_mux_children(struct device *dev, void *addrp)
{
        int result;

        if (dev->type == &i2c_adapter_type)
                result = device_for_each_child(dev, addrp,
                                                aartyaa_lcd_dev_check_mux_children);
        else    
                result = aartyaa_lcd_dev_check(dev, addrp);

        return result; 
}

/* This address checking function differs from the one in i2c-core
   in that it considers an address with a registered device, but no
   driver bound to it, as NOT busy. */
static int i2cdev_check_addr(struct i2c_adapter *adapter, unsigned int addr)
{
        struct i2c_adapter *parent = i2c_parent_is_i2c_adapter(adapter);
        int result = 0;
                        
        if (parent) {
                result = aartyaa_lcd_check_mux_parents(parent, addr);
                pr_debug("i2cdev_check_addr : parent not null \n");
        }       

        if (!result)
                result = device_for_each_child(&adapter->dev, &addr,
                                                aartyaa_lcd_dev_check_mux_children);
        pr_debug("i2cdev_check_addr : aartyaa result = %d\n", result);
        return result;
}


static noinline int aartyaa_lcd_dev_ioctl_smbus(struct i2c_client *client,
                unsigned long arg)
{
        struct i2c_smbus_ioctl_data data_arg;
        union i2c_smbus_data temp;
        int datasize, res;

        if (copy_from_user(&data_arg,
                           (struct i2c_smbus_ioctl_data __user *) arg,
                           sizeof(struct i2c_smbus_ioctl_data)))
                return -EFAULT;
        if ((data_arg.size != I2C_SMBUS_BYTE) &&
            (data_arg.size != I2C_SMBUS_QUICK) &&
            (data_arg.size != I2C_SMBUS_BYTE_DATA) &&
            (data_arg.size != I2C_SMBUS_WORD_DATA) &&
            (data_arg.size != I2C_SMBUS_PROC_CALL) &&
            (data_arg.size != I2C_SMBUS_BLOCK_DATA) &&
            (data_arg.size != I2C_SMBUS_I2C_BLOCK_BROKEN) &&
            (data_arg.size != I2C_SMBUS_I2C_BLOCK_DATA) &&
            (data_arg.size != I2C_SMBUS_BLOCK_PROC_CALL)) {
                dev_dbg(&client->adapter->dev,
                        "size out of range (%x) in ioctl I2C_SMBUS.\n",
                        data_arg.size);
                return -EINVAL;
        }
        /* Note that I2C_SMBUS_READ and I2C_SMBUS_WRITE are 0 and 1,
           so the check is valid if size==I2C_SMBUS_QUICK too. */
        if ((data_arg.read_write != I2C_SMBUS_READ) &&
            (data_arg.read_write != I2C_SMBUS_WRITE)) {
                dev_dbg(&client->adapter->dev,
                        "read_write out of range (%x) in ioctl I2C_SMBUS.\n",
                        data_arg.read_write);
                return -EINVAL;
        }

        /* Note that command values are always valid! */

        if ((data_arg.size == I2C_SMBUS_QUICK) ||
            ((data_arg.size == I2C_SMBUS_BYTE) &&
		(data_arg.read_write == I2C_SMBUS_WRITE)))
                /* These are special: we do not use data */
                return i2c_smbus_xfer(client->adapter, client->addr,
                                      client->flags, data_arg.read_write,
                                      data_arg.command, data_arg.size, NULL);

        if (data_arg.data == NULL) {
                dev_dbg(&client->adapter->dev,
                        "data is NULL pointer in ioctl I2C_SMBUS.\n");
                return -EINVAL;
        }

        if ((data_arg.size == I2C_SMBUS_BYTE_DATA) ||
            (data_arg.size == I2C_SMBUS_BYTE))
                datasize = sizeof(data_arg.data->byte);
        else if ((data_arg.size == I2C_SMBUS_WORD_DATA) ||
                 (data_arg.size == I2C_SMBUS_PROC_CALL))
                datasize = sizeof(data_arg.data->word);
        else /* size == smbus block, i2c block, or block proc. call */
                datasize = sizeof(data_arg.data->block);

        if ((data_arg.size == I2C_SMBUS_PROC_CALL) ||
            (data_arg.size == I2C_SMBUS_BLOCK_PROC_CALL) ||
            (data_arg.size == I2C_SMBUS_I2C_BLOCK_DATA) ||
            (data_arg.read_write == I2C_SMBUS_WRITE)) {
                if (copy_from_user(&temp, data_arg.data, datasize))
                        return -EFAULT;
        }
        if (data_arg.size == I2C_SMBUS_I2C_BLOCK_BROKEN) {
                /* Convert old I2C block commands to the new
                   convention. This preserves binary compatibility. */
                data_arg.size = I2C_SMBUS_I2C_BLOCK_DATA;
                if (data_arg.read_write == I2C_SMBUS_READ)
                        temp.block[0] = I2C_SMBUS_BLOCK_MAX;
        }

	pr_debug("i2cdev_ioctl_smbus : addr = %x, flags = %x, read_write = %d, command = %x, size = %d, temp = %x\n", 
		client->addr, client->flags, data_arg.read_write, data_arg.command, data_arg.size, temp.byte);	

       res = i2c_smbus_xfer(client->adapter, client->addr, client->flags,
              data_arg.read_write, data_arg.command, data_arg.size, &temp);
        if (!res && ((data_arg.size == I2C_SMBUS_PROC_CALL) ||
                     (data_arg.size == I2C_SMBUS_BLOCK_PROC_CALL) ||
                     (data_arg.read_write == I2C_SMBUS_READ))) {
                if (copy_to_user(data_arg.data, &temp, datasize))
                        return -EFAULT;
        }

        pr_debug("i2cdev_ioctl_smbus : res= %d\n", res);
        return res;
}



static long aartyaa_lcd_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
        struct i2c_client *client = file->private_data;
        unsigned long funcs;

        dev_dbg(&client->adapter->dev, "ioctl, cmd=0x%02x, arg=0x%02lx\n",
                cmd, arg);

        pr_debug("ioctl, cmd=0x%02x, arg=0x%02lx\n", cmd, arg);
        pr_debug("i2cdev_ioctl : I2C_SLAVE address = %d\n", client->addr);

        switch (cmd) {
        case I2C_SLAVE:
        case I2C_SLAVE_FORCE:
                /* NOTE:  devices set up to work with "new style" drivers
                 * can't use I2C_SLAVE, even when the device node is not
                 * bound to a driver.  Only I2C_SLAVE_FORCE will work.
                 *
                 * Setting the PEC flag here won't affect kernel drivers,
                 * which will be using the i2c_client node registered with
                 * the driver model core.  Likewise, when that client has
                 * the PEC flag already set, the i2c-dev driver won't see
                 * (or use) this setting.
                 */
                if ((arg > 0x3ff) ||
                    (((client->flags & I2C_M_TEN) == 0) && arg > 0x7f))
                        return -EINVAL;
                if (cmd == I2C_SLAVE && i2cdev_check_addr(client->adapter, arg))
                        return -EBUSY;
                /* REVISIT: address could become busy later */
                client->addr = arg;

                return 0;

	case I2C_FUNCS:
                funcs = i2c_get_functionality(client->adapter);
                return put_user(funcs, (unsigned long __user *)arg);

	case I2C_SMBUS:
                return aartyaa_lcd_dev_ioctl_smbus(client, arg);

	default:
		pr_debug("aartyaa_lcd_ioctl : gooig to the default and cmd = %x\n", cmd);
               return -ENOTTY;
        }
        return 0;
}      

/** struct for the device file operation */
static struct file_operations aartyaa_lcd_fops = {
	.owner = THIS_MODULE,	
	.open = aartyaa_lcd_open,
	.release = aartyaa_lcd_release,
	.read = aartyaa_lcd_read,
	.write = aartyaa_lcd_write,
	.unlocked_ioctl = aartyaa_lcd_ioctl,
	.llseek = no_llseek
};


/*
 * Routines to manage notifier chains for passing status changes to any 
 * interested routines
 */

static int aartyaa_lcd_attach_adapter(struct device *dev, void *dummy)
{
        struct i2c_adapter *adap;
        struct aartyaa_lcd_dev *i2c_dev;
        int res;

        if (dev->type != &i2c_adapter_type)
                return 0;
        adap = to_i2c_adapter(dev);

        i2c_dev = get_free_i2c_dev(adap);
        if (IS_ERR(i2c_dev))
                return PTR_ERR(i2c_dev);
 
        /* register this i2c device with the driver core */
        i2c_dev->dev = device_create(aartyaa_lcd_class, &adap->dev,
                                     MKDEV(MAJOR_NUM, adap->nr), NULL,
                                     "aartyaa_lcd-%d", adap->nr);
        if (IS_ERR(i2c_dev->dev)) {
                res = PTR_ERR(i2c_dev->dev);
                goto error;
        }

        pr_debug("i2c-dev: adapter [%s] registered as minor %d\n",
                 adap->name, adap->nr);
        return 0;
error:
        return_aartyaa_lcd_dev(i2c_dev);
        return res;
}

static int aartyaa_lcd_detach_adapter(struct device *dev, void *dummy)
{                      
        struct i2c_adapter *adap;
        struct aartyaa_lcd_dev *i2c_dev;
                       
        if (dev->type != &i2c_adapter_type)
                return 0;
        adap = to_i2c_adapter(dev);

        i2c_dev = aartyaa_lcd_dev_get_by_minor(adap->nr);
        if (!i2c_dev) /* attach_adapter must have failed */
                return 0;

        return_aartyaa_lcd_dev(i2c_dev);
        device_destroy(aartyaa_lcd_class, MKDEV(MAJOR_NUM, adap->nr));

        pr_debug("i2c-dev: adapter [%s] unregistered\n", adap->name);
        return 0;
}

static int aartyaa_lcd_notifier_call(struct notifier_block *nb, 
				unsigned long action, 
				void *data)
{       
        struct device *dev = data;
	
	pr_debug("aartyaa_lcd_notifier_call : action = %lu\n", action);
        switch (action) {
	       case BUS_NOTIFY_ADD_DEVICE:
        	       return aartyaa_lcd_attach_adapter(dev, NULL);
	
	      case BUS_NOTIFY_DEL_DEVICE:
        	       return aartyaa_lcd_detach_adapter(dev, NULL);
        }

        return 0; 
}

static struct notifier_block aartyaa_lcd_notifier = {
        .notifier_call = aartyaa_lcd_notifier_call,
};

static __init int aartyaa_lcd_init(void)
{
	int ret = -1;
	        int res;

        printk(KERN_INFO "aartyaa_lcd_dev_init : i2c /dev entries driver\n");

        if ( (res = register_chrdev(MAJOR_NUM, "aartyaa_lcd", &aartyaa_lcd_fops)) )
		goto out;

        pr_debug(KERN_INFO "aartyaa_lcd_init : registerd the chardev\n");

        if ( IS_ERR( aartyaa_lcd_class = class_create(THIS_MODULE, "aartyaa_lcd-dev")) ) {
                res = PTR_ERR(aartyaa_lcd_class);
                goto out_unreg_chrdev;
        }
	
	pr_debug("aartyaa_lcd_init : %s class created\n", DRIVER_NAME);       
 
	/* 
	 * Keep track of adapters which will be added or removed later.  
	 * uses to let kernel know that asynch event has been happened.
	 * The  notifier chain facility is a general mechanism provided 
	 * by the kernel.  It is designed to provide away for kernel elements 
	 * to express interest in being informed about the occurence of general
	 * asynchronous events
	 */
        
	if ( res = bus_register_notifier(&i2c_bus_type, &aartyaa_lcd_notifier) ) {
		pr_debug("aartyaa_lcd_init : falied to notify the kernel\n");
                goto out_unreg_class;
	}
	
        /* Bind to already existing adapters right away */
        i2c_for_each_dev(NULL, aartyaa_lcd_attach_adapter);

        return 0;

out_unreg_class:
        class_destroy(aartyaa_lcd_class);
out_unreg_chrdev:
        unregister_chrdev(MAJOR_NUM, DRIVER_NAME);
out:
        pr_debug(KERN_ERR "%s: Driver Initialisation failed\n", __FILE__);
        return res;

return ret;
}


static void __exit aartyaa_lcd_exit(void)
{
        bus_unregister_notifier(&i2c_bus_type, &aartyaa_lcd_notifier);
        i2c_for_each_dev(NULL, aartyaa_lcd_detach_adapter);
        class_destroy(aartyaa_lcd_class);
        unregister_chrdev(MAJOR_NUM, DRIVER_NAME);
}


module_init(aartyaa_lcd_init);
module_exit(aartyaa_lcd_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("aartyaa lcd driver");
MODULE_AUTHOR("prityaa");
