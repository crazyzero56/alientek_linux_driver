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
#include <linux/semaphore.h>
#include <linux/types.h>

#define GPIOLED_CNT 1
#define GPIOLED_NAME "gpioled"
#define LEDOFF 0
#define LEDON 1

/* gpioled设备结构体 */
struct gpioled_dev {
	dev_t devid;			/* 设备号 	 */
	struct cdev cdev;		/* cdev 	*/
	struct class *class;	/* 类 		*/
	struct device *device;	/* 设备 	 */
	int major;				/* 主设备号	  */
	int minor;				/* 次设备号   */
	struct device_node *nd; /* 设备节点 */
	int led_gpio;			/* led所使用的GPIO编号		*/
	struct mutex lock;		/* 互斥体 */
};

static struct gpioled_dev gpioled; /* led设备 */

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
 * @brief led_write do copy_from_user. make sure the app command.
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
	struct gpioled_dev *dev = file->private_data;

	if ((sizeof(databuf) / sizeof(unsigned char)) < cnt) {
		printk("error : input data size greater than buffer!!\r\n");
		return -EFAULT;
	}

	retvalue = copy_from_user(databuf, buff, cnt);
	if (retvalue < 0) {
		printk("kernel write failed!\r\n");
		return -EFAULT;
	}

	ledstat = databuf[0];
	switch (ledstat) {
	case LEDON:
		gpio_set_value(dev->led_gpio, 0);
		break;
	case LEDOFF:
		gpio_set_value(dev->led_gpio, 1);
		break;
	default:
		printk("command value not define, do noting! \r\n");
		break;
	}

	return 0;
}

/**
 * @brief led_open request a spinlock to multi access at a same time.
 *
 *
 * @param inode
 * @param file
 * @return int return BUSY if dev is used. otherwise return zero.
 */
static int led_open(struct inode *inode, struct file *file)
{
	file->private_data = &gpioled;
#if 1
	if (mutex_lock_interruptible(&gpioled.lock)) {
		return -ERESTARTSYS;
	}
#else
	mutex_lock(&bpioled.lock);
#endif

	return 0;
}

/**
 * @brief led_release request a spinlock to maintain status in critial section.
 *
 * @param inode
 * @param file
 * @return int
 */
static int led_release(struct inode *inode, struct file *file)
{
	struct gpioled_dev *dev = file->private_data;
	mutex_unlock(&dev->lock);
	return 0;
}

static struct file_operations gpioled_fops = {
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

	mutex_init(&gpioled.lock);
	gpioled.nd = of_find_node_by_path("/gpioled");
	if (gpioled.nd == NULL) {
		printk("gpioled node not found\n");
		return -EINVAL;
	}
	ret = of_property_read_string(gpioled.nd, "status", &str);
	if (ret < 0) {
		printk("gpioled status not found!\n");
		return -EINVAL;
	}

	if (strcmp(str, "okay")) {
		printk("gpioled status not okay status is %s!\n", str);
		return -EINVAL;
	}

	ret = of_property_read_string(gpioled.nd, "compatible", &str);
	if (ret < 0) {
		printk("gpioled : fail to get commpatible property\n");
		return -EINVAL;
	}
	if (strcmp(str, "alientek, led")) {
		printk("gpioled: compatible match failed\n");
		return -EINVAL;
	}
	gpioled.led_gpio = of_get_named_gpio(gpioled.nd, "led-gpio", 0);
	if (gpioled.led_gpio < 0) {
		printk("gpioled get gpio fail!\n");
		return -EINVAL;
	}
	printk("led-gpio num = %u\r\n", gpioled.led_gpio);

	ret = gpio_request(gpioled.led_gpio, "LED-GPIO");
	if (ret < 0) {
		printk(KERN_ERR "gpioled: failed to request led-gpio\n");
		return ret;
	}

	ret = gpio_direction_output(gpioled.led_gpio, 1);
	if (ret < 0) {
		printk("can't set output high!\r\n");
	}

	if (gpioled.major) {
		gpioled.devid = MKDEV(gpioled.major, 0);
		ret = register_chrdev_region(gpioled.devid, GPIOLED_CNT, GPIOLED_NAME);

		if (ret < 0) {
			pr_err(
				"can't register %s char driver [ret=%d]\n", GPIOLED_NAME, ret);
			goto free_gpio;
		}
	} else {
		ret = alloc_chrdev_region(&gpioled.devid, 0, GPIOLED_CNT, GPIOLED_NAME);
		if (ret < 0) {
			pr_err("can't alloc_chrdev_region %s char driver [ret=%d]\n",
				GPIOLED_NAME, ret);
			goto free_gpio;
		}
		gpioled.major = MAJOR(gpioled.devid);
		gpioled.minor = MINOR(gpioled.devid);
	}
	printk("alloc chrdev gpioled  major=%d minor=%d\n", gpioled.major,
		gpioled.minor);

	gpioled.cdev.owner = THIS_MODULE;
	cdev_init(&gpioled.cdev, &gpioled_fops);

	ret = cdev_add(&gpioled.cdev, gpioled.devid, GPIOLED_CNT);
	if (ret < 0) {
		pr_err("cdev_add fail\n");
		goto unregister_chrdev;
	}

	gpioled.class = class_create(THIS_MODULE, GPIOLED_NAME);
	if (IS_ERR(gpioled.class)) {
		pr_err("class_create fail\n");
		goto del_cdev;
	}

	gpioled.device =
		device_create(gpioled.class, NULL, gpioled.devid, NULL, GPIOLED_NAME);
	if (IS_ERR(gpioled.device)) {
		pr_err("device_create fail\n");
		goto destroy_class;
	}

	mutex_init(&gpioled.lock);

	return 0;
destroy_class:
	class_destroy(gpioled.class);
del_cdev:
	cdev_del(&gpioled.cdev);
unregister_chrdev:
	unregister_chrdev_region(gpioled.devid, GPIOLED_CNT);
free_gpio:
	gpio_free(gpioled.led_gpio);
	return -EIO;
}

static void __exit led_exit(void)
{
	/* 注销字符设备驱动 */
	cdev_del(&gpioled.cdev);
	/* 删除 cdev */
	unregister_chrdev_region(gpioled.devid, GPIOLED_CNT);
	device_destroy(gpioled.class, gpioled.devid);
	class_destroy(gpioled.class); /* 注销类 */
	gpio_free(gpioled.led_gpio);
}

module_init(led_init);
module_exit(led_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("ALIENTEK");
MODULE_INFO(intree, "Y");