#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/ide.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/gpio.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/of_gpio.h>
#include <linux/semaphore.h>
#include <linux/timer.h>
#include <linux/irq.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/fs.h>
#include <linux/fcntl.h>
#include <linux/platform_device.h>
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>

#define LEDDEV_CNT 1
#define LEDDEV_NAME "dtsplatled"
#define LEDOFF 0
#define LEDON 1

struct leddev_dev{
	dev_t devid;
	struct cdev cdev;
	struct class *class;
	struct device *device;
	int major;
	int minor;
	struct device_node *node;
	int gpio_led;
};
struct leddev_dev leddev;

int led_open(struct inode *inode, struct file *file){
	return 0;
}

ssize_t led_write(struct file *filp, const char __user *buf, size_t cnt, loff_t *offt)
{
	int retvalue;
	unsigned char databuf[1];
	unsigned char ledstat;

	retvalue = copy_from_user(databuf, buf, cnt);
	if (retvalue < 0)
	{
		printk("kernel write failed!\r\n");
		return -EFAULT;
	}

	/* get default value */
	ledstat = databuf[0];
	if (ledstat == LEDON)
	{
		gpio_set_value(leddev.gpio_led, 0);
	}
	else if (ledstat == LEDOFF)
	{
		gpio_set_value(leddev.gpio_led, 1);
	}

	return 0;
}

static int led_gpio_init(struct device_node *nd){

	int ret = 0;
	leddev.gpio_led = of_get_named_gpio(nd, "led-gpio", 0);
	if (!gpio_is_valid(leddev.gpio_led)) {
		printk("parsing dts led-gpio fail!\n");
		return -EINVAL;
	}

	ret = gpio_request(leddev.gpio_led, "LED0");
	if (ret < 0)
	{
		printk(KERN_ERR "leddriver : failed to request led-gpio\n");
		return ret;
	}

	ret = gpio_direction_output(leddev.gpio_led, 1);
	if (ret < 0)
	{
		printk("can't set output high!\r\n");
		return ret;
	}

	return ret;
}

struct file_operations gpioled_fops={
	.owner = THIS_MODULE,
	.open = led_open,
	.write = led_write,
};

static int led_probe(struct platform_device *pdev){

	int ret = -1;

	ret = led_gpio_init(pdev->dev.of_node);
	if(ret){
		printk("led_gpio_init fail !\n");
	}

	if (leddev.major)
	{
		leddev.devid = MKDEV(leddev.major, 0);
		printk("register_chrdev_region !\n");
		ret = register_chrdev_region(leddev.devid, LEDDEV_CNT, LEDDEV_NAME);

		if (ret < 0)
		{
			pr_err(
				"can't register %s char driver [ret=%d]\n", LEDDEV_NAME, ret);
			goto free_gpio;
		}
	}
	else
	{
		printk("alloc_chrdev_region !\n");
		ret = alloc_chrdev_region(&leddev.devid, 0, LEDDEV_CNT, LEDDEV_NAME);
		if (ret < 0)
		{
			pr_err("can't alloc_chrdev_region %s char driver [ret=%d]\n",
				   LEDDEV_NAME, ret);
			goto free_gpio;
		}
		leddev.major = MAJOR(leddev.devid);
		leddev.minor = MINOR(leddev.devid);
	}
	printk("alloc chrdev leddev  major=%d minor=%d\n", leddev.major,
		   leddev.minor);

	leddev.cdev.owner = THIS_MODULE;
	cdev_init(&leddev.cdev, &gpioled_fops);

	ret = cdev_add(&leddev.cdev, leddev.devid, LEDDEV_CNT);
	if (ret < 0)
	{
		pr_err("cdev_add fail\n");
		goto unregister_chrdev;
	}

	leddev.class = class_create(THIS_MODULE, LEDDEV_NAME);
	if (IS_ERR(leddev.class))
	{
		pr_err("class_create fail\n");
		goto del_cdev;
	}

	leddev.device =
		device_create(leddev.class, NULL, leddev.devid, NULL, LEDDEV_NAME);
	if (IS_ERR(leddev.device))
	{
		pr_err("device_create fail\n");
		goto destroy_class;
	}
	return 0;

destroy_class:
	class_destroy(leddev.class);
del_cdev:
	cdev_del(&leddev.cdev);
unregister_chrdev:
	unregister_chrdev_region(leddev.devid, LEDDEV_CNT);
free_gpio:
	gpio_free(leddev.gpio_led);
	return -EIO;
}

/*
struct of_device_id {
	char	name[32];
	char	type[32];
	char	compatible[128];
	const void *data;
};
*/

int led_remove(struct platform_device *pdev)
{
	gpio_set_value(leddev.gpio_led, 1);
	gpio_free(leddev.gpio_led);

	cdev_del(&leddev.cdev);
	unregister_chrdev_region(leddev.devid, LEDDEV_CNT);
	device_destroy(leddev.class, leddev.devid);
	class_destroy(leddev.class);
	return 0;
}

static struct of_device_id led_of_match[] = {
	{ .compatible = "alientek,led" },
	{}
};

static struct platform_driver led_driver = {
	.driver = {
		.name = "stm32mp1-led", /* driver name, compare to device name*/
		.of_match_table = led_of_match,
	},
	.probe = led_probe,
	.remove = led_remove,
};


static int __init leddriver_init(void)
{
	return platform_driver_register(&led_driver);
}

static void __exit leddriver_exit(void)
{
	platform_driver_unregister(&led_driver);
}

module_init(leddriver_init);
module_exit(leddriver_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("ALIENTEK");
MODULE_INFO(intree, "Y");