#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/gpio.h>
#include <linux/ide.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>

#define DEVCNT 1
#define DRIVER_NAME "chrdev"

#define PERIPH_BASE (0x40000000)
#define MPU_AHB4_PERIPH_BASE (PERIPH_BASE + 0x10000000)
#define RCC_BASE (MPU_AHB4_PERIPH_BASE + 0x0000)
#define RCC_MP_AHB4ENSETR (RCC_BASE + 0XA28)
#define GPIOI_BASE (MPU_AHB4_PERIPH_BASE + 0xA000)
#define GPIOI_MODER (GPIOI_BASE + 0x0000)
#define GPIOI_OTYPER (GPIOI_BASE + 0x0004)
#define GPIOI_OSPEEDR (GPIOI_BASE + 0x0008)
#define GPIOI_PUPDR (GPIOI_BASE + 0x000C)
#define GPIOI_BSRR (GPIOI_BASE + 0x0018)
#define LEDON 1
#define LEDOFF 0
struct mychr {
	dev_t devId;
	int major;
	int minor;
	struct class *mychrclass;
	struct cdev mychrcdev;
	struct device *mychrdev;
};

static struct mychr mychrdev;
static void __iomem *MPU_AHB4_PERIPH_RCC_PI;
static void __iomem *GPIOI_MODER_PI;
static void __iomem *GPIOI_OTYPER_PI;
static void __iomem *GPIOI_OSPEEDR_PI;
static void __iomem *GPIOI_PUPDR_PI;
static void __iomem *GPIOI_BSRR_PI;

#if 0
int (*open) (struct inode *, struct file *);
ssize_t (*read) (struct file *, char __user *, size_t, loff_t *);
ssize_t (*write) (struct file *, const char __user *, size_t, loff_t *);
int (*release) (struct inode *, struct file *);
#endif

static void led_unmap(void)
{
	/* 取消映射 */
	iounmap(MPU_AHB4_PERIPH_RCC_PI);
	iounmap(GPIOI_MODER_PI);
	iounmap(GPIOI_OTYPER_PI);
	iounmap(GPIOI_OSPEEDR_PI);
	iounmap(GPIOI_PUPDR_PI);
	iounmap(GPIOI_BSRR_PI);
}

static void led_switch(u8 sta)
{
	u32 val = 0;
	if (sta == LEDON) {
		val = readl(GPIOI_BSRR_PI);
		val |= (1 << 16);
		writel(val, GPIOI_BSRR_PI);
	} else if (sta == LEDOFF) {
		val = readl(GPIOI_BSRR_PI);
		val |= (1 << 0);
		writel(val, GPIOI_BSRR_PI);
	}
}
static int chrtest_open(struct inode *inode, struct file *file)
{
	return 0;
}

static ssize_t chrtest_read(struct file *filp, char __user *buf, size_t cnt, loff_t *offt)
{
	return 0;
}

static ssize_t chrtest_write(struct file *filp, const char __user *buf, size_t cnt, loff_t *offt)
{
	int retvalue;
	unsigned char databuf[1];
	unsigned char ledstat;
	retvalue = copy_from_user(databuf, buf, cnt);
	if (retvalue < 0) {
		printk(KERN_ERR "kernel write error\n");
		return -EFAULT;
	}

	ledstat = databuf[0];
	if (ledstat == LEDON) {
		led_switch(LEDON);
	} else if (ledstat == LEDOFF) {
		led_switch(LEDOFF);
	}

	return 0;
}

static int chrtest_release(struct inode *inode, struct file *filp)
{
	return 0;
}

static struct file_operations test_fops = {
	.owner = THIS_MODULE,
	.open = chrtest_open,
	.read = chrtest_read,
	.write = chrtest_write,
	.release = chrtest_release,
};

#if 0
static int __init xxx_init(void)
{
	int ret = 0;
	ret = register_chrdev(200,"chrtest",&test_fops);
	if(ret){
		while(1)
		{

		}
	}
	return 0;
}

static void __exit xxx_exit(void)
{
	unregister_chrdev(200,"chrtest");
}
#else
static int __init xxx_init(void)
{
	int ret = -1;
	u32 val = 0;

	MPU_AHB4_PERIPH_RCC_PI = ioremap(RCC_MP_AHB4ENSETR, 4);
	GPIOI_MODER_PI = ioremap(GPIOI_MODER, 4);
	GPIOI_OTYPER_PI = ioremap(GPIOI_OTYPER, 4);
	GPIOI_OSPEEDR_PI = ioremap(GPIOI_OSPEEDR, 4);
	GPIOI_PUPDR_PI = ioremap(GPIOI_PUPDR, 4);
	GPIOI_BSRR_PI = ioremap(GPIOI_BSRR, 4);

	// GPIOI peripheral clocks enable : bit8
	val = readl(MPU_AHB4_PERIPH_RCC_PI);
	val &= ~(0x1 << 8);
	val |= (0x1 << 8);
	writel(val, MPU_AHB4_PERIPH_RCC_PI);

	val = readl(GPIOI_MODER_PI);
	val &= ~(0x3 << 0);
	val |= (0x1 << 0);
	writel(val, GPIOI_MODER_PI);

	val = readl(GPIOI_OTYPER_PI);
	val &= ~(0x1 << 0);
	writel(val, GPIOI_OTYPER_PI);

	val = readl(GPIOI_OSPEEDR_PI);
	val &= ~(0x3 << 0);
	val |= (0x2 << 0);
	writel(val, GPIOI_OSPEEDR_PI);

	val = readl(GPIOI_PUPDR_PI);
	val &= ~(0x3 << 0);
	val |= (0x1 << 0);
	writel(val, GPIOI_PUPDR_PI);

	val = readl(GPIOI_BSRR_PI);
	val |= (0x1 << 0);
	writel(val, GPIOI_BSRR_PI);

	ret = alloc_chrdev_region(&mychrdev.devId, 0, DEVCNT, DRIVER_NAME);
	mychrdev.major = MAJOR(mychrdev.devId);
	mychrdev.minor = MINOR(mychrdev.devId);
	if (ret < 0) {
		printk(KERN_ERR "alloc_chrdev_region() failed for chrdevbase\n");
		led_unmap();
		return ret;
	}
	printk(KERN_ERR "alloc_chrdev_region() allocated, major:%d,minor:%d", mychrdev.major, mychrdev.minor);

	/*register cdev*/
	mychrdev.mychrcdev.owner = THIS_MODULE;
	cdev_init(&mychrdev.mychrcdev, &test_fops);
	cdev_add(&mychrdev.mychrcdev, mychrdev.devId, DEVCNT);

	/*register class*/
	mychrdev.mychrclass = class_create(THIS_MODULE, DRIVER_NAME);
	if (IS_ERR(mychrdev.mychrclass)) {
		led_unmap();
		return PTR_ERR(mychrdev.mychrclass);
	}
	/*register device*/
	mychrdev.mychrdev = device_create(mychrdev.mychrclass, NULL, mychrdev.devId, NULL, DRIVER_NAME);
	if (IS_ERR(mychrdev.mychrdev)) {
		led_unmap();
		return PTR_ERR(mychrdev.mychrdev);
	}
	return 0;
}

static void __exit xxx_exit(void)
{
	led_unmap();
	cdev_del(&mychrdev.mychrcdev);
	unregister_chrdev_region(mychrdev.devId, DEVCNT);

	device_destroy(mychrdev.mychrclass, mychrdev.devId);
	class_destroy(mychrdev.mychrclass);
}
#endif

module_init(xxx_init);
module_exit(xxx_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("davidhsieh");
MODULE_INFO(author, "crazyzero56@gmail.com");