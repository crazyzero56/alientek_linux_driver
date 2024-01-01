#include <asm/io.h>
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/fcntl.h>
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
#include <linux/poll.h>
#include <linux/types.h>

#define KEY_CNT 1
#define KEY_NAME "key"

enum key_status {
	KEY_PRESS = 0,
	KEY_RELEASE,
	KEY_KEEP,
};

struct key_dev {
	dev_t devid;
	struct cdev cdev;
	struct device_node *nd;
	struct class *class;
	struct device *device;
	int major;
	int minor;
	struct timer_list timer;
	int key_gpio;
	int irq_num;
	atomic_t status;
	wait_queue_head_t r_wait;
	struct fasync_struct *async_queue;
};

static struct key_dev key;

static irqreturn_t key_interrupt(int irq, void *dev_id)
{
	mod_timer(&key.timer, jiffies + msecs_to_jiffies(15));
	return IRQ_HANDLED;
}

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
		printk("key get gpio fail!\n");
		return -EINVAL;
	}

	key.irq_num = irq_of_parse_and_map(key.nd, 0);
	if (!key.irq_num) {
		printk("get irq_num fail!\n");
		return -EINVAL;
	}
	printk("key-gpio num = %u\r\n", key.key_gpio);

	return 0;
}

static int key_gpio_init(void)
{
	int ret = -1;
	unsigned long irq_flags;

	ret = gpio_request(key.key_gpio, "USER-KEY0");

	if (ret < 0) {
		printk(KERN_ERR "key: failed to request key-gpio\n");
		return ret;
	}

	ret = gpio_direction_input(key.key_gpio);
	if (ret < 0) {
		printk(KERN_ERR "can't set gpio input!\r\n");
	}
	irq_flags = irq_get_trigger_type(key.irq_num);
	if (irq_flags == IRQF_TRIGGER_NONE)
		irq_flags = IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING;

	ret = request_irq(key.irq_num, key_interrupt, irq_flags, "Key0_IRQ", NULL);
	if (ret) {
		printk(KERN_ERR "request_irq fail!\r\n");
		gpio_free(key.key_gpio);
		return ret;
	}
	return 0;
}

static void key_timer_function(struct timer_list *arg)
{
	static int last_val = 1;
	int current_val;

	current_val = gpio_get_value(key.key_gpio);

	if ((current_val == 0) && last_val) {
		atomic_set(&key.status, KEY_PRESS);
		wake_up_interruptible(&key.r_wait);
		if (key.async_queue)
			kill_fasync(&key.async_queue, SIGIO, POLL_IN);
	} else if ((current_val == 1) && !last_val) {
		atomic_set(&key.status, KEY_RELEASE);
		wake_up_interruptible(&key.r_wait);
		if (key.async_queue)
			kill_fasync(&key.async_queue, SIGIO, POLL_IN);
	} else {
		atomic_set(&key.status, KEY_KEEP);
	}

	last_val = current_val;
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
	/* do nothing */
	return 0;
}

/**
 * @brief key_read do nothing
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
	pr_err("key_read\n");
	if (file->f_flags & O_NONBLOCK) {
		pr_err("nonblocking read\n");
		if (atomic_read(&key.status) == KEY_KEEP) {
			return -EAGAIN;
		}

	} else {
		pr_err("blocking read\n");
		ret = wait_event_interruptible(
			key.r_wait, (atomic_read(&key.status) != KEY_KEEP));
	}
	if (ret)
		return ret;

	ret = copy_to_user(buff, &key.status, sizeof(int));

	atomic_set(&key.status, KEY_KEEP);

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

static int key_fasync(int fd, struct file *filp, int on)
{
	return fasync_helper(fd, filp, on, &key.async_queue);
}

static int key_release(struct inode *inode, struct file *file)
{
	return key_fasync(-1, file, 0);
}

static unsigned int key_poll(struct file *filp, poll_table *wait)
{
	unsigned int mask = 0;
	pr_err("key_poll\n");
	poll_wait(filp, &key.r_wait, wait);
	if (atomic_read(&key.status) != KEY_KEEP)
		mask = POLLIN | POLLRDNORM;

	return mask;
}

static struct file_operations key_fops = {
	.owner = THIS_MODULE,
	.open = key_open,
	.read = key_read,
	.write = key_write,
	.release = key_release,
	.poll = key_poll,
	.fasync = key_fasync,
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

	init_waitqueue_head(&key.r_wait);

	atomic_set(&key.status, KEY_KEEP);
	ret = key_parse_dt();
	if (ret != 0) {
		printk(KERN_ERR "fail to parse_dt \n");
		return ret;
	}

	ret = key_gpio_init();
	if (ret != 0) {
		printk(KERN_ERR "key_gpio_init fail \n");
		return ret;
	}

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
	if (key.major) {
		key.devid = MKDEV(key.major, 0);
		ret = register_chrdev_region(key.devid, KEY_CNT, KEY_NAME);

		if (ret < 0) {
			pr_err("can't register %s char driver [ret=%d]\n", KEY_NAME, ret);
			goto free_gpio;
		}
	} else {
		ret = alloc_chrdev_region(&key.devid, 0, KEY_CNT, KEY_NAME);
		if (ret < 0) {
			pr_err("can't alloc_chrdev_region %s char driver [ret=%d]\n",
				KEY_NAME, ret);
			goto free_gpio;
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

	timer_setup(&key.timer, key_timer_function, 0);

	return 0;
destroy_class:
	class_destroy(key.class);
del_cdev:
	cdev_del(&key.cdev);
unregister_chrdev:
	unregister_chrdev_region(key.devid, KEY_CNT);
free_gpio:
	gpio_free(key.key_gpio);
	return -EIO;
}

static void __exit mykey_exit(void)
{
	/* 注销字符设备驱动 */
	cdev_del(&key.cdev);
	/* 删除 cdev */
	unregister_chrdev_region(key.devid, KEY_CNT);
	del_timer_sync(&key.timer);
	device_destroy(key.class, key.devid);
	class_destroy(key.class); /* 注销类 */
	free_irq(key.irq_num, NULL);
	gpio_free(key.key_gpio);
}

module_init(mykey_init);
module_exit(mykey_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("ALIENTEK");
MODULE_INFO(intree, "Y");
