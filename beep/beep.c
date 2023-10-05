#include <asm/io.h>
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/gpio.h>
#include <linux/ide.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>
#include <linux/types.h>

#define BEEP_CNT 1 /* 设备号个数 */

#define BEEP_NAME "beep" /* 名字 */
#define BEEPOFF 0		 /* 关灯 */
#define BEEPON 1		 /* 开灯 */

/* beep 设备结构体 */
struct beep_dev {
	dev_t devid;			/* 设备号 */
	struct cdev cdev;		/* cdev */
	struct class *class;	/* 类 */
	struct device *device;	/* 设备 */
	int major;				/* 主设备号 */
	int minor;				/* 次设备号 */
	struct device_node *nd; /* 设备节点 */
	int beep_gpio;			/* led 所使用的 GPIO 编号 */
};
struct beep_dev beep; /* led 设备 */

/**
 * @brief led_open do nothing
 *
 * @param inode
 * @param file
 * @return int always return 0
 */
static int led_open(struct inode *inode, struct file *file)
{
	file->private_data = &beep;
	return 0;
}

/**
 * @brief led_read do nothing
 *
 * @param file
 * @param buff
 * @param size
 * @param offset
 * @return ssize_t
 */
static ssize_t led_read(
	struct file *file, char __user *buff, size_t size, loff_t *offset)
{

	return 0;
}

/**
 * @brief
 *
 * @param file
 * @param buff
 * @param size
 * @param offset
 * @return ssize_t
 */
static ssize_t led_write(
	struct file *file, const char __user *buff, size_t cnt, loff_t *offset)
{
	int retvalue;
	unsigned char databuf[1];
	unsigned char ledstat;
	struct beep_dev *dev = file->private_data;
	retvalue = copy_from_user(databuf, buff, cnt);
	if (retvalue < 0) {
		printk("kernel write failed!\r\n");
		return -EFAULT;
	}
	ledstat = databuf[0];

	if (ledstat == BEEPON) {
		gpio_set_value(dev->beep_gpio, 0);
	} else if (ledstat == BEEPOFF) {
		gpio_set_value(dev->beep_gpio, 1);
	}

	return 0;
}

static int led_release(struct inode *inode, struct file *file) { return 0; }

static struct file_operations beep_fops = {
	.owner = THIS_MODULE,
	.open = led_open,
	.read = led_read,
	.write = led_write,
	.release = led_release,
};

static int __init led_init(void)
{
	int ret = 0;
	const char *str;

	beep.nd = of_find_node_by_path("/beep");
	if (beep.nd == NULL) {
		printk("beep node not found!\n");
		return -EINVAL;
	}

	ret = of_property_read_string(beep.nd, "status", &str);
	if (ret < 0) {
		printk("beep status not found!\n");
		return -EINVAL;
	}

	if (strcmp(str, "okay")) {
		printk("beep status not okay status is %s!\n", str);
		return -EINVAL;
	}

	ret = of_property_read_string(beep.nd, "compatible", &str);
	if (ret < 0) {
		printk("beep: Failed to get compatible property\n");
		return -EINVAL;
	}

	if (strcmp(str, "alientek,beep")) {
		printk("beep not compatible!\n");
		return -EINVAL;
	}

	beep.beep_gpio = of_get_named_gpio(beep.nd, "beep-gpio", 0);
	if (beep.beep_gpio < 0) {
		printk("beep get gpio fail!\n");
		return -EINVAL;
	}
	printk("beep-gpio num = %u\r\n", beep.beep_gpio);

	ret = gpio_request(beep.beep_gpio, "BEEP-GPIO");
	if (ret < 0) {
		printk(KERN_ERR "beep: failed to request beep-gpio\n");
		return ret;
	}

	ret = gpio_direction_output(beep.beep_gpio, 1);
	if (ret < 0) {
		printk("can't set output high!\r\n");
	}

	if (beep.major) {
		beep.devid = MKDEV(beep.major, 0);
		ret = register_chrdev_region(beep.devid, BEEP_CNT, BEEP_NAME);

		if (ret < 0) {
			pr_err("can't register %s char driver [ret=%d]\n", BEEP_NAME, ret);
			goto free_gpio;
		}
	} else {
		ret = alloc_chrdev_region(&beep.devid, 0, BEEP_CNT, BEEP_NAME);
		if (ret < 0) {
			pr_err("can't alloc_chrdev_region %s char driver [ret=%d]\n",
				BEEP_NAME, ret);
			goto free_gpio;
		}
		beep.major = MAJOR(beep.devid);
		beep.minor = MINOR(beep.devid);
	}
	printk("alloc chrdev beep  major=%d minor=%d\n", beep.major, beep.minor);

	beep.cdev.owner = THIS_MODULE;
	cdev_init(&beep.cdev, &beep_fops);

	ret = cdev_add(&beep.cdev, beep.devid, BEEP_CNT);
	if (ret < 0) {
		pr_err("cdev_add fail\n");
		goto unregister_chrdev;
	}

	beep.class = class_create(THIS_MODULE, BEEP_NAME);
	if (IS_ERR(beep.class)) {
		pr_err("class_create fail\n");
		goto del_cdev;
	}

	beep.device = device_create(beep.class, NULL, beep.devid, NULL, BEEP_NAME);
	if (IS_ERR(beep.device)) {
		pr_err("device_create fail\n");
		goto destroy_class;
	}
	return 0;
destroy_class:
	class_destroy(beep.class);
del_cdev:
	cdev_del(&beep.cdev);
unregister_chrdev:
	unregister_chrdev_region(beep.devid, BEEP_CNT);
free_gpio:
	gpio_free(beep.beep_gpio);
	return -EIO;
}

static void __exit led_exit(void)
{
	/* 注销字符设备驱动 */
	cdev_del(&beep.cdev);
	/* 删除 cdev */
	unregister_chrdev_region(beep.devid, BEEP_CNT);
	device_destroy(beep.class, beep.devid);
	class_destroy(beep.class); /* 注销类 */
}

module_init(led_init);
module_exit(led_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("ALIENTEK");
MODULE_INFO(intree, "Y");
