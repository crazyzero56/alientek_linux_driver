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
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/types.h>

#define KEY_CNT 1 /* 设备号个数 */
#define KEY_NAME "key"
#define KEY0VALUE 0xF0
#define INVAKEY 0x00

enum key_status {
	KEY_PRESS = 0,
	KEY_RELEASE,
	KEY_KEEP,
};

/* 设备结构体 */
struct key_dev {
	dev_t devid;			/* 设备号 */
	struct cdev cdev;		/* cdev */
	struct device_node *nd; /* 设备节点 */
	struct class *class;	/* 类 */
	struct device *device;	/* 设备 */
	int major;				/* 主设备号 */
	int minor;				/* 次设备号 */
	struct timer_list timer;
	int key_gpio; /* key 所使用的 GPIO 编号 */
	int irq_num;
	atomic_t keyvalue;
};

static struct key_dev key; /* led 设备 */

static int key_parse_dt(void)
{
	int ret;
	const char *str;

	key.nd = of_find_node_by_path("/key");
	if (key.nd == NULL) {
		printk("key node not found!\n");
		return -EINVAL;
	}

	ret = of_property_read_string(key.nd, "status", &str);
	if (ret < 0) {
		printk("key status not found!\n");
		return -EINVAL;
	}

	if (strcmp(str, "okay")) {
		printk("key status not okay status is %s!\n", str);
		return -EINVAL;
	}

	ret = of_property_read_string(key.nd, "compatible", &str);
	if (ret < 0) {
		printk("key: Failed to get compatible property\n");
		return -EINVAL;
	}

	if (strcmp(str, "alientek,key")) {
		printk("key not compatible!\n");
		return -EINVAL;
	}

	key.key_gpio = of_get_named_gpio(key.nd, "key-gpio", 0);
	if (key.key_gpio < 0) {
		printk("can't get key-gpio fail!\n");
		return -EINVAL;
	}

	printk("key-gpio num = %u\r\n", key.key_gpio);

	return 0;
}

/**
 * @brief key_open do nothing
 *
 * @param inode
 * @param file
 * @return int always return 0
 */
static int key_open(struct inode *inode, struct file *file)
{
	int ret = 0;

	file->private_data = &key;

	ret = gpio_request(key.key_gpio, "KEY0");
	printk(KERN_ERR "key: request key-gpio res %d\n", ret);
	if (ret < 0) {
		printk(KERN_ERR "key: failed to request key-gpio\n");
		return ret;
	}

	ret = gpio_direction_input(key.key_gpio);
	if (ret < 0) {
		printk(KERN_ERR "can't set gpio input!\r\n");
	}

	return 0;
}

/**
 * @brief key_read
 *
 * @param file
 * @param buff
 * @param size
 * @param offset
 * @return ssize_t
 */
static ssize_t key_read(
	struct file *file, char __user *buff, size_t size, loff_t *offset)
{
	int ret = 0;
	int value;
	struct key_dev *tmp = file->private_data;

	if (gpio_get_value(tmp->key_gpio) == 0) {
		while (!gpio_get_value(tmp->key_gpio))
			;
		atomic_set(&tmp->keyvalue, KEY0VALUE);
	} else {
		atomic_set(&tmp->keyvalue, INVAKEY);
	}

	value = atomic_read(&tmp->keyvalue);
	ret = copy_to_user(buff, &value, sizeof(int));

	return ret;
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
static ssize_t key_write(
	struct file *file, const char __user *buff, size_t cnt, loff_t *offset)
{
	/* do nothing */
	return 0;
}

static int key_release(struct inode *inode, struct file *file)
{

	struct key_dev *tmp = file->private_data;
	gpio_free(tmp->key_gpio);

	return 0;
}

static struct file_operations key_fops = {
	.owner = THIS_MODULE,
	.open = key_open,
	.read = key_read,
	.write = key_write,
	.release = key_release,
};

#if 0
	key {
		compatible = "alientek, led";
		status = "okay";
		led-gpio = <&gpioi 0 GPIO_ACTIVE_LOW>;
	};
#endif

static int __init mykey_init(void)
{
	int ret = 0;

	key.keyvalue = (atomic_t)ATOMIC_INIT(0);

	atomic_set(&key.keyvalue, INVAKEY);

	/*
		ret = gpio_request(key.key_gpio, "LED-GPIO");
		if (ret < 0) {
			printk(KERN_ERR "key: failed to request key-gpio\n");
			return ret;
		}

		ret = gpio_direction_output(key.key_gpio, 1);
		if (ret < 0) {
			printk("can't set output high!\r\n");
		}
	*/
	ret = key_parse_dt();
	if (ret < 0) {
		return ret;
	}

	if (key.major) {
		key.devid = MKDEV(key.major, 0);
		ret = register_chrdev_region(key.devid, KEY_CNT, KEY_NAME);

		if (ret < 0) {
			pr_err("can't register %s char driver [ret=%d]\n", KEY_NAME, ret);
			return -EIO;
		}
	} else {
		ret = alloc_chrdev_region(&key.devid, 0, KEY_CNT, KEY_NAME);
		if (ret < 0) {
			pr_err("can't alloc_chrdev_region %s char driver [ret=%d]\n",
				KEY_NAME, ret);
			return -EIO;
		}
		key.major = MAJOR(key.devid);
		key.minor = MINOR(key.devid);
	}
	printk("alloc chrdev key  major=%d minor=%d\n", key.major, key.minor);

	key.cdev.owner = THIS_MODULE;
	cdev_init(&key.cdev, &key_fops);

	ret = cdev_add(&key.cdev, key.devid, KEY_CNT);
	if (ret < 0) {
		pr_err("cdev_add fail\n");
		goto unregister_chrdev;
	}

	key.class = class_create(THIS_MODULE, KEY_NAME);
	if (IS_ERR(key.class)) {
		pr_err("class_create fail\n");
		goto del_cdev;
	}

	key.device = device_create(key.class, NULL, key.devid, NULL, KEY_NAME);
	if (IS_ERR(key.device)) {
		pr_err("device_create fail\n");
		goto destroy_class;
	}

	return 0;
destroy_class:
	class_destroy(key.class);
del_cdev:
	cdev_del(&key.cdev);
unregister_chrdev:
	unregister_chrdev_region(key.devid, KEY_CNT);

	return -EIO;
}

static void __exit mykey_exit(void)
{
	/* 注销字符设备驱动 */
	cdev_del(&key.cdev);
	/* 删除 cdev */
	unregister_chrdev_region(key.devid, KEY_CNT);
	device_destroy(key.class, key.devid);
	class_destroy(key.class); /* 注销类 */
}

module_init(mykey_init);
module_exit(mykey_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("ALIENTEK");
MODULE_INFO(intree, "Y");
