#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/ide.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/gpio.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>

#define MISCBEEP_NAME "miscbeep"
#define MISCBEEP_MINOR 144
#define BEEPON         1
#define BEEPOFF        0


struct miscbeep_dev{
	dev_t devid;
	struct cdev cdev;
	struct class *class;
	struct device *device;
	int beep_gpio;
};

struct miscbeep_dev miscbeep;


int miscbeep_open(struct inode *inode, struct file *filp){
	return 0;
}

ssize_t miscbeep_write(struct file *filp, const char __user *buff, size_t cnt, loff_t *offt){
	int ret;
	unsigned char databuff[1];
	unsigned char beepstat;

	ret = copy_from_user(databuff,buff,cnt);
	if(ret < 0){
		pr_err("kernel write fail\n");
		return -EFAULT;
	}

	beepstat = databuff[0];
	if(beepstat == BEEPON){
		gpio_set_value(miscbeep.beep_gpio, 0);
	}else if(beepstat == BEEPOFF){
		gpio_set_value(miscbeep.beep_gpio, 1);
	}

	return 0;
}

static int beep_gpio_init(struct device_node *node){
	int ret = 0;
	miscbeep.beep_gpio = of_get_named_gpio(node, "beep-gpio", 0);
	if (!gpio_is_valid(miscbeep.beep_gpio)) {
		pr_err("parsing dts beep-gpio fail!\n");
		return -EINVAL;
	}

	ret = gpio_request(miscbeep.beep_gpio, "beep");
	if(ret < 0){
		pr_err("gpio request fail");
		return ret;
	}

	ret = gpio_direction_output(miscbeep.beep_gpio, 1);
	if(ret < 0){
		pr_err("gpio_direction fail");
		return ret;
	}
	return ret;
}

struct file_operations miscbeep_fops = {
	.owner = THIS_MODULE,
	.open = miscbeep_open,
	.write = miscbeep_write,
};

static struct miscdevice beep_miscdev = {
	.minor = MISCBEEP_MINOR,
	.name = MISCBEEP_NAME,
	.fops = &miscbeep_fops,
};

static int beep_probe(struct platform_device *pdev){
	int ret = 0;

	/* gpio init*/
	ret = beep_gpio_init(pdev->dev.of_node);
	if(ret){
		pr_err("gpio init fail \n");
	}

	/* register misc device */
	ret = misc_register(&beep_miscdev);
	if(ret <0){
		pr_err("misc_register fail \n");
		goto free_gpio;
	}
	return 0;

free_gpio:
	gpio_free(miscbeep.beep_gpio);
	return -EINVAL;
}

static int beep_remove(struct platform_device *pdev){
	/* turn off the beep */
	gpio_set_value(miscbeep.beep_gpio, 1);
	/* release beep gpio */
	gpio_free(miscbeep.beep_gpio);

	misc_deregister(&beep_miscdev);
	return 0;
}


static struct of_device_id led_of_match[] = {
	{ .compatible = "alientek,beep" },
	{}
};

static struct platform_driver beep_driver = {
	.driver = {
		.name = "stm32mp1-beep", /* driver name, compare to device name*/
		.of_match_table = led_of_match,
	},
	.probe = beep_probe,
	.remove = beep_remove,
};

static int __init miscbeep_init(void){
	return platform_driver_register(&beep_driver);
}

static void __exit miscbeep_exit(void)
{
	platform_driver_unregister(&beep_driver);
}

module_init(miscbeep_init);
module_exit(miscbeep_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("ALIENTEK");
MODULE_INFO(intree, "Y");