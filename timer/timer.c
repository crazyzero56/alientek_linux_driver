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

#define TIMER_CNT 1
#define TIMER_NAME "timer"
#define CLOSE_CMD (_IO(0XEF, 0x1))
#define OPEN_CMD (_IO(0XEF, 0x2))
#define SETPERIOD_CMD (_IO(0XEF, 0x3))
#define LEDON 1
#define LEDOFF 0

struct timer_dev {
	dev_t devid;
	struct cdev cdev;
	struct device_node *nd;
	struct class *class;
	struct device *device;
	int major;
	int minor;
	struct timer_list timer;
	int led_gpio;
	int timerperiod;
	spinlock_t lock;
};

struct timer_dev timerdev;

void timer_function(struct timer_list *arg)
{
	struct timer_dev *dev = from_timer(dev, arg, timer);
	static int sta = 1;
	int timerperiod;
	unsigned long flags;

	/* revsese the gpio value, led may twinkling */
	sta = !sta;
	gpio_set_value(dev->led_gpio, sta);

	spin_lock_irqsave(&dev->lock, flags);
	timerperiod = dev->timerperiod;
	spin_unlock_irqrestore(&dev->lock, flags);
	mod_timer(&dev->timer, jiffies + msecs_to_jiffies(dev->timerperiod));
}

static int led_parse_dt(void)
{
	int ret;
	const char *str;

	timerdev.nd = of_find_node_by_path("/gpioled");
	if (timerdev.nd == NULL) {
		printk("timerdev: node not found!\n");
		return -EINVAL;
	}

	ret = of_property_read_string(timerdev.nd, "status", &str);
	if (ret < 0) {
		printk("timerdev status not found!\n");
		return -EINVAL;
	}

	if (strcmp(str, "okay")) {
		printk("timerdev: status not okay status is %s!\n", str);
		return -EINVAL;
	}

	ret = of_property_read_string(timerdev.nd, "compatible", &str);
	if (ret < 0) {
		printk("timerdev: Failed to get compatible property\n");
		return -EINVAL;
	}

	if (strcmp(str, "alientek, led")) {
		printk("timerdev: not compatible!\n");
		return -EINVAL;
	}

	timerdev.led_gpio = of_get_named_gpio(timerdev.nd, "led-gpio", 0);
	if (timerdev.led_gpio < 0) {
		printk("timerdev: get gpio fail!\n");
		return -EINVAL;
	}

	printk("led-gpio num = %u\r\n", timerdev.led_gpio);

	return 0;
}

static int led_gpio_init(void)
{
	int ret = 0;
	ret = gpio_request(timerdev.led_gpio, "led");
	if (ret < 0) {
		printk(KERN_ERR "cat't get led-gpio\n");
		return ret;
	}

	/* set gpio PI0 output, high level(turn off) */
	ret = gpio_direction_output(timerdev.led_gpio, 1);
	if (ret < 0) {
		printk(KERN_ERR "cat't set gpio fail\n");
		return ret;
	}
	return 0;
}

static int timer_open(struct inode *inode, struct file *filp)
{
	int ret = 0;
	filp->private_data = &timerdev;
	timerdev.timerperiod = 1000;

	ret = led_parse_dt();
	if (ret < 0) {
		printk(KERN_ERR "led_parse_dt fail \n");
		return ret;
	}
	ret = led_gpio_init();
	if (ret < 0) {
		printk(KERN_ERR "led_gpio_init fail \n");
		return ret;
	}

	return 0;
}

static int led_release(struct inode *inode, struct file *filp)
{
	struct timer_dev *dev = filp->private_data;
	gpio_set_value(dev->led_gpio, 1); /* APP结束的时候关闭LED */
	gpio_free(dev->led_gpio);		  /* 释放LED				*/
	del_timer_sync(&dev->timer);	  /* 关闭定时器 			*/

	return 0;
}

static long timer_unlocked_ioctl(
	struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct timer_dev *dev = (struct timer_dev *)filp->private_data;
	int timerperiod;
	unsigned long flags;

	switch (cmd) {
	case CLOSE_CMD:
		del_timer_sync(&dev->timer);
		break;
	case OPEN_CMD:
		spin_lock_irqsave(&dev->lock, flags);
		timerperiod = dev->timerperiod;
		spin_unlock_irqrestore(&dev->lock, flags);
		mod_timer(&dev->timer, jiffies + msecs_to_jiffies(timerperiod));
		break;
	case SETPERIOD_CMD:
		spin_lock_irqsave(&dev->lock, flags);
		timerperiod = arg;
		spin_unlock_irqrestore(&dev->lock, flags);
		mod_timer(&dev->timer, jiffies + msecs_to_jiffies(arg));
	default:
		break;
	}
	return 0;
}

static struct file_operations timer_fops = {
	.owner = THIS_MODULE,
	.open = timer_open,
	.unlocked_ioctl = timer_unlocked_ioctl,
	.release = led_release,
};

static int __init timer_init(void)
{
	int ret = 0;

	spin_lock_init(&timerdev.lock);

	ret = led_parse_dt();

	/*
		ret = led_gpio_init();
		if (ret != 0) {
			printk(KERN_ERR "led_gpio_init fail \n");
			goto free_gpio;
		}
	*/
	if (timerdev.major) {
		timerdev.devid = MKDEV(timerdev.major, 0);
		ret = register_chrdev_region(timerdev.devid, TIMER_CNT, TIMER_NAME);

		if (ret < 0) {
			pr_err("can't register %s char driver [ret=%d]\n", TIMER_NAME, ret);
			goto free_gpio;
		}
	} else {
		ret = alloc_chrdev_region(&timerdev.devid, 0, TIMER_CNT, TIMER_NAME);
		if (ret < 0) {
			pr_err("can't alloc_chrdev_region %s char driver [ret=%d]\n",
				TIMER_NAME, ret);
			goto free_gpio;
		}
		timerdev.major = MAJOR(timerdev.devid);
		timerdev.minor = MINOR(timerdev.devid);
	}
	printk("alloc chrdev timer major=%d minor=%d\n", timerdev.major,
		timerdev.minor);

	timerdev.cdev.owner = THIS_MODULE;
	cdev_init(&timerdev.cdev, &timer_fops);

	ret = cdev_add(&timerdev.cdev, timerdev.devid, TIMER_CNT);
	if (ret < 0) {
		pr_err("cdev_add fail\n");
		goto unregister_chrdev;
	}

	timerdev.class = class_create(THIS_MODULE, TIMER_NAME);
	if (IS_ERR(timerdev.class)) {
		pr_err("class_create fail\n");
		goto del_cdev;
	}

	timerdev.device =
		device_create(timerdev.class, NULL, timerdev.devid, NULL, TIMER_NAME);
	if (IS_ERR(timerdev.device)) {
		pr_err("device_create fail\n");
		goto destroy_class;
	}

	timer_setup(&timerdev.timer, timer_function, 0);

	return 0;
destroy_class:
	class_destroy(timerdev.class);
del_cdev:
	cdev_del(&timerdev.cdev);
unregister_chrdev:
	unregister_chrdev_region(timerdev.devid, TIMER_CNT);
free_gpio:
	gpio_free(timerdev.led_gpio);
	return -EIO;
}

static void __exit timer_exit(void)
{

	del_timer_sync(&timerdev.timer);
	cdev_del(&timerdev.cdev);
	unregister_chrdev_region(timerdev.devid, TIMER_CNT);

	device_destroy(timerdev.class, timerdev.devid);
	class_destroy(timerdev.class);
}

module_init(timer_init);
module_exit(timer_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("ALIENTEK");
MODULE_INFO(intree, "Y");
