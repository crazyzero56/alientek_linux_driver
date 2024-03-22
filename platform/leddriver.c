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
#define LEDDEV_NAME "platled"
#define LEDOFF 0
#define LEDON 1

static void __iomem *MPU_AHB4_PERIPH_RCC_PI;
static void __iomem *GPIOI_MODER_PI;
static void __iomem *GPIOI_OTYPER_PI;
static void __iomem *GPIOI_OSPEEDR_PI;
static void __iomem *GPIOI_PUPDR_PI;
static void __iomem *GPIOI_BSRR_PI;

struct gpioled_dev
{
	dev_t devid;
	struct cdev cdev;
	struct class *class;
	struct device *device;
	int major;
	int minor;
	struct device_node *nd;
	int led_gpio;
	atomic_t lock;
};
struct gpioled_dev leddev;

void led_switch(u8 sta)
{
	if (sta == LEDON)
		gpio_set_value(leddev.led_gpio, 0);
	else if (sta == LEDOFF)
		gpio_set_value(leddev.led_gpio, 1);
}

static int led_open(struct inode *inode, struct file *filp)
{
	return 0;
}

static void led_unmap(void)
{
	iounmap(MPU_AHB4_PERIPH_RCC_PI);
	iounmap(GPIOI_MODER_PI);
	iounmap(GPIOI_OTYPER_PI);
	iounmap(GPIOI_OSPEEDR_PI);
	iounmap(GPIOI_PUPDR_PI);
	iounmap(GPIOI_BSRR_PI);
}

static ssize_t led_write(struct file *filp, const char __user *buf, size_t cnt, loff_t *offt)
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
		led_switch(LEDON);
	}
	else if (ledstat == LEDOFF)
	{
		led_switch(LEDOFF);
	}

	return 0;
}

static struct file_operations gpioled_fops = {
	.owner = THIS_MODULE,
	.open = led_open,
	//.read = led_read,
	.write = led_write,
	//.release = led_release,
};

static int led_probe(struct platform_device *dev)
{
	struct resource *led_resource[6];
	int ressize[6];
	u32 val = 0;
	int ret = 0;
	int it = 0;
	for (it = 0; it < 6; it++)
	{
		led_resource[it] = platform_get_resource(dev, IORESOURCE_MEM, it);
		if (!led_resource[it])
		{
			dev_err(&dev->dev, "No MEM resource for always on\n");
			return -ENXIO;
		}
		ressize[it] = resource_size(led_resource[it]);
	}

	MPU_AHB4_PERIPH_RCC_PI = ioremap(led_resource[0]->start, ressize[0]);
	GPIOI_MODER_PI = ioremap(led_resource[1]->start, ressize[1]);
	GPIOI_OTYPER_PI = ioremap(led_resource[2]->start, ressize[2]);
	GPIOI_OSPEEDR_PI = ioremap(led_resource[3]->start, ressize[3]);
	GPIOI_PUPDR_PI = ioremap(led_resource[4]->start, ressize[4]);
	GPIOI_BSRR_PI = ioremap(led_resource[5]->start, ressize[5]);

	/* config the gpio led. See more detail in data sheet. */

	/* enable clock */
	val = readl(MPU_AHB4_PERIPH_RCC_PI);
	val &= ~(0X1 << 8);
	val |= (0X1 << 8);
	writel(val, MPU_AHB4_PERIPH_RCC_PI);

	/* output mode */
	val = readl(GPIOI_MODER_PI);
	val &= ~(0X3 << 0); /* bit0:1清零 */
	val |= (0X1 << 0);	/* bit0:1设置01 */
	writel(val, GPIOI_MODER_PI);

	/* push-pull */
	val = readl(GPIOI_OTYPER_PI);
	val &= ~(0X1 << 0);
	writel(val, GPIOI_OTYPER_PI);

	/* high speed*/
	val = readl(GPIOI_OSPEEDR_PI);
	val &= ~(0X3 << 0);
	val |= (0x2 << 0);
	writel(val, GPIOI_OSPEEDR_PI);

	/* pull up resistor */
	val = readl(GPIOI_PUPDR_PI);
	val &= ~(0X3 << 0);
	val |= (0x1 << 0);
	writel(val, GPIOI_PUPDR_PI);

	/* led set defalut is off */
	val = readl(GPIOI_BSRR_PI);
	val |= (0x1 << 0);
	writel(val, GPIOI_BSRR_PI);

	if (leddev.major)
	{
		leddev.devid = MKDEV(leddev.major, 0);
		ret = register_chrdev_region(leddev.devid, LEDDEV_CNT, LEDDEV_NAME);

		if (ret < 0)
		{
			pr_err(
				"can't register %s char driver [ret=%d]\n", LEDDEV_NAME, ret);
			goto fail_map;
		}
	}
	else
	{
		ret = alloc_chrdev_region(&leddev.devid, 0, LEDDEV_CNT, LEDDEV_NAME);
		if (ret < 0)
		{
			pr_err("can't alloc_chrdev_region %s char driver [ret=%d]\n",
				   LEDDEV_NAME, ret);
			goto fail_map;
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
fail_map:
	led_unmap();
	return -EIO;
}

int led_remove(struct platform_device *pdev)
{
	led_unmap();
	cdev_del(&leddev.cdev);
	unregister_chrdev_region(leddev.devid, LEDDEV_CNT);

	device_destroy(leddev.class, leddev.devid);
	class_destroy(leddev.class);
	return 0;
}

static struct platform_driver led_driver = {
	.driver = {
		.name = "stm32mp1-led", /* driver name, compare to device name*/
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