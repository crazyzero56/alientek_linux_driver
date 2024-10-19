#include <linux/module.h>
#include <linux/errno.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/i2c.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/ide.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/uaccess.h>
#include "ap3216c.h"

/* ap3216c driver have original file path : drivers/char/ap3216c.c */

#define AP3216C_CNT		1
#define AP3216C_NAME	"ap3216c"

struct ap3216c_dev {
	dev_t devid;				/* device id */
	struct cdev my_cdev;		/* character dev */
	struct class *my_class;		/* class */
	struct device *my_device;	/* device */
	struct device_node *nd; 	/* device node */
	struct i2c_client *client;	/* i2c slave */
	unsigned short ir,als,ps;	/* ap3216c data */
};

static int ap3216c_read_regs(struct ap3216c_dev *dev, u8 reg, u8 *val, int len)
{
	int ret = 0;
	struct i2c_client *client = dev->client;

	struct i2c_msg msg[2] = {
		[0] = {
			.addr = client->addr,
			.flags = 0,
			.len = 1,
			.buf = &reg,
		},
		[1] = {
			.addr = client->addr,
			.flags = I2C_M_RD,
			.len = len,
			.buf = val,
		}
	};

	ret = i2c_transfer(client->adapter, msg, 2);
	if (ret == 2)
	{
		ret = 0;
	}
	else
	{
		dev_err(dev->my_device,"i2c rd failed=%d reg = %06x len=%d\n", ret, reg, len);
		ret = -EREMOTEIO;
	}
	return ret;
}

static unsigned char ap3216c_read_reg(struct ap3216c_dev *dev, u8 reg)
{
	u8 data = 0;
	ap3216c_read_regs(dev, reg, &data, 1);
	return data;
}

static void ap3216c_readdata(struct ap3216c_dev *dev)
{
	int i = 0;
	unsigned char buf[6];
	for(i = 0 ; i < 6 ; i++)
	{
		buf[i] = ap3216c_read_reg(dev, AP3216C_IRDATALOW + i);
	}

	// IR overflow handle
	if(buf[0] & 0x80)
		dev->ir = 0;
	else
		dev->ir = ((unsigned short) buf[1] << 2) | (buf[0] & 0x3);

	dev->als = ((unsigned short)buf[3] << 8) | buf[2];

	// IR overflow handle
	if(buf[4] & 0x40)
		dev->ps = 0;
	else
		dev->ps = ((unsigned short)(buf[5] & 0x3F ) << 4) | (buf[4] & 0x0F);
}

static ssize_t ap3216c_read(struct file *file, char __user *buff, size_t size, loff_t *offset)
{
	unsigned short data[3];
	long err = 0;
	struct cdev *cdev = file->f_path.dentry->d_inode->i_cdev;
	struct ap3216c_dev *dev = container_of(cdev, struct ap3216c_dev, my_cdev);

	ap3216c_readdata(dev);
	data[0] = dev->ir;
	data[1] = dev->als;
	data[2] = dev->ps;
	err = copy_to_user(buff, data, sizeof(data));

	return 0;
}


static int ap3216c_write_regs(struct i2c_client *client, u8 reg, u8 data)
{
	u8 buf[2] = {reg, data};

	struct i2c_msg msg = {
		.addr  = client->addr,
		.flags = 0,
		.len   = 2,
		.buf   = buf,
	};

	if (1 != i2c_transfer(client->adapter, &msg, 1))
	{
		return -1;
	}
	return 0;
}

static int ap3216c_write_reg(struct ap3216c_dev *dev, u8 reg, u8 data)
{
	struct i2c_client *client = dev->client;

	if (ap3216c_write_regs(client,reg,data) < 0)
	{
		dev_err(dev->my_device,"ap3216c_write_regs error");
	}
	return -EIO;
}

static struct of_device_id ap3216c_of_match[] = {
	{.compatible = "alientek,ap3216c"},
	{}
};

static int ap3216c_open(struct inode *inode, struct file *file)
{
	// get ap3216c address.
	// So Crazy. Big Brain!!!
	struct cdev *cdev = file->f_path.dentry->d_inode->i_cdev;
	struct ap3216c_dev *dev = container_of(cdev, struct ap3216c_dev, my_cdev);
	int res = 0;
	/* initial AP3216C */
	/* SW reset */
	res = ap3216c_write_reg(dev,AP3216C_SYSTEMCONG, 0x04);
	if(res < 0)
		return -EIO;

	mdelay(50);

	/* ALS and PS + IR function active */
	res = ap3216c_write_reg(dev,AP3216C_SYSTEMCONG, 0x03);
	if(res < 0)
		return -EIO;

	return 0;
}

static const struct file_operations ap3216c_ops = {
	.owner = THIS_MODULE,
	.open = ap3216c_open,
	.read = ap3216c_read,
};

static int ap3216c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int ret = 0;
	struct ap3216c_dev *ap3216cdev;
	ap3216cdev = devm_kzalloc(&client->dev,sizeof(struct ap3216c_dev),GFP_KERNEL);
	if (!ap3216cdev)
		return -ENOMEM;

	ret = alloc_chrdev_region(&ap3216cdev->devid, 0, AP3216C_CNT, AP3216C_NAME);
	if (ret < 0) {
		pr_err("can't alloc_chrdev_region %s char driver [ret=%d]\n",
			AP3216C_NAME, ret);
		return -ENOMEM;
	}
	//ap3216cdev->cdev.owner = THIS_MODULE;
	cdev_init(&ap3216cdev->my_cdev, &ap3216c_ops);

	ret = cdev_add(&ap3216cdev->my_cdev, ap3216cdev->devid, AP3216C_CNT);
	if (ret < 0) {
		pr_err("cdev_add fail\n");
		goto unregister_chrdev;
	}

	ap3216cdev->my_class = class_create(THIS_MODULE, AP3216C_NAME);
	if (IS_ERR(ap3216cdev->my_class)) {
		pr_err("class_create fail\n");
		goto del_cdev;
	}

	ap3216cdev->my_device = device_create(ap3216cdev->my_class, NULL, ap3216cdev->devid, NULL, AP3216C_NAME);
	if (IS_ERR(ap3216cdev->my_device)) {
		pr_err("device_create fail\n");
		goto destroy_class;
	}
	ap3216cdev->client = client;

	i2c_set_clientdata(client,ap3216cdev);

	return 0;

	destroy_class:
		device_destroy(ap3216cdev->my_class, ap3216cdev->devid);
	del_cdev:
		cdev_del(&ap3216cdev->my_cdev);
	unregister_chrdev:
		unregister_chrdev_region(ap3216cdev->devid,AP3216C_CNT);
	return -EIO;
}

static int ap3216c_remove(struct i2c_client *client)
{
	struct ap3216c_dev *ap3216cdev = i2c_get_clientdata(client);
	cdev_del(&ap3216cdev->my_cdev);
	unregister_chrdev_region(ap3216cdev->devid,AP3216C_CNT);
	device_destroy(ap3216cdev->my_class, ap3216cdev->devid);
	class_destroy(ap3216cdev->my_class);
	return 0;
}

static struct i2c_driver ap3216c_driver = {
	.driver = {
		.name = "ap3216c",
		.of_match_table = ap3216c_of_match,
	},
	.probe = ap3216c_probe,
	.remove = ap3216c_remove,
	//.id_table = ap3216c_id,
};

static int __init ap3216c_init(void)
{
	return i2c_add_driver(&ap3216c_driver);
}

static void __exit ap3216c_exit(void)
{
	i2c_del_driver(&ap3216c_driver);
}

module_init(ap3216c_init);
module_exit(ap3216c_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("david.hsieh0516@gmail.com");
