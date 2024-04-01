#include <linux/module.h>
#include <linux/errno.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/of_gpio.h>
#include <linux/input.h>
#include <linux/timer.h>
#include <linux/of_irq.h>
#include <linux/interrupt.h>

#define KEYINPUT_NAME		"keyinput"

struct key_dev {
	struct input_dev *idev;
	struct timer_list timer;
	int gpio_key;
	int irq_key;
};

static struct key_dev key;

static irqreturn_t key_interrupt(int irq, void *dev_id)
{
	if(key.irq_key != irq)
		return IRQ_NONE;

	/* should not call disable_irq() to disable interrupt in the isr */
	//disable_irq(irq);

	disable_irq_nosync(irq);
	mod_timer(&key.timer, jiffies + msecs_to_jiffies(15));

	return IRQ_HANDLED;
}

static void key_timer_function(struct timer_list *arg)
{
	int val;

	val = gpio_get_value(key.gpio_key);
	input_report_key(key.idev, KEY_0, !val);
	input_sync(key.idev);

	enable_irq(key.irq_key);
}

static int gpio_init(struct device_node *nd){
	int ret = 0;
	unsigned long irq_flags;

	key.gpio_key = of_get_named_gpio(nd, "key-gpio",0);
	if(!gpio_is_valid(key.gpio_key)) {
		pr_err("parsing dts key-gpio fail\n");
		return -EINVAL;
	}

	ret = gpio_request(key.gpio_key,"KEY0");
	if (ret) {
		pr_err("gpio_request fail\n");
		return ret;
	}
	gpio_direction_input(key.gpio_key);

	/* register interrupt */
	key.irq_key = irq_of_parse_and_map(nd,0);
	if (!key.irq_key){
		pr_err("irq_of_parse_and_map fail\n");
		return ret;
	}
	pr_debug("get key.irq_key %d\n",key.irq_key);

	irq_flags = irq_get_trigger_type(key.irq_key);
	if ( irq_flags == IRQF_TRIGGER_NONE){
		pr_err("irq_get_trigger_type wired \n");
		irq_flags = IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING;
	}

	ret = request_irq(key.irq_key, key_interrupt,irq_flags,"Key0_IRQ", NULL);
	if (ret) {
		pr_err("request_irq fail\n");
		return ret;
	}

	return 0;
}

static int atk_key_remove(struct platform_device *pdev){
	/* unregister irq and gpio */
	free_irq(key.irq_key,NULL);
	gpio_free(key.gpio_key);
	del_timer_sync(&key.timer);

	/* unregister input dev */
	input_unregister_device(key.idev);

	return 0;
}


static int atk_key_probe(struct platform_device *pdev){
	int ret = 0;

	/* gpio init */
	ret = gpio_init(pdev->dev.of_node);
	if(ret < 0 ) {
		pr_err("gpio_init fail\n");
		return ret;
	}

	timer_setup(&key.timer, key_timer_function, 0);

	/* register input dev */
	key.idev = input_allocate_device();
	key.idev->name = KEYINPUT_NAME;

#if 0

	__set_bit(EV_KEY, key.idev->evbit);
	__set_bit(EV_REP, key.idev->evbit);

	__set_bit(KEY_0, key.idev->keybit);
#endif

#if 0
	key.idev->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_REP);
	key.idev->keybit[BIT_WORD(KEY_0)] |= BIT_MASK(KEY_0);
#endif

	key.idev->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_REP);
	input_set_capability(key.idev, EV_KEY, KEY_0);

	ret = input_register_device(key.idev);
	if(ret){
		pr_err("gpio_init fail\n");
		goto free_gpio;
	}

	return 0;

free_gpio:
	free_irq(key.irq_key,NULL);
	gpio_free(key.gpio_key);
	del_timer_sync(&key.timer);
	return -EIO;
}

static struct of_device_id key_of_match[] = {
	{.compatible = "alientek,key"},
	{}
};
#if 0
static struct platform_driver atk_key_driver = {
	.driver = {
		.name = "stm32mp1-key",
		.of_match_table = key_of_match,
	},
	.probe = atk_key_probe,
	.remove = atk_key_remove,
};
module_platform_driver(atk_key_driver);
#else
static struct platform_driver atk_key_driver = {
	.driver = {
		.name = "stm32mp1-key",
		.of_match_table = key_of_match,
	},
	.remove = atk_key_remove,
};
module_platform_driver_probe(atk_key_driver,atk_key_probe);
#endif

MODULE_LICENSE("GPL");
MODULE_AUTHOR("ALIENTEK");
MODULE_INFO(intree, "Y");