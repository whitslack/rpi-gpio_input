#include <asm/io.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#define MODNAME "gpio_input"

enum {
	PIN = 4
};

/* System Timer Counter Lower 32 bits */
static volatile u32 __iomem * const ST_CLO = (volatile u32 __iomem *) IO_ADDRESS(ST_BASE + 0x04);

/* GPIO Pin Level 0..1 */
static volatile u32 __iomem * const GPIO_GPLEV = (volatile u32 __iomem *) IO_ADDRESS(GPIO_BASE + 0x34);
/* GPIO Pin Event Detect Status 0..1 */
static volatile u32 __iomem * const GPIO_GPEDS = (volatile u32 __iomem *) IO_ADDRESS(GPIO_BASE + 0x40);
/* GPIO Pin Rising Edge Detect Enable 0..1 */
static volatile u32 __iomem * const GPIO_GPREN = (volatile u32 __iomem *) IO_ADDRESS(GPIO_BASE + 0x4C);
/* GPIO Pin Falling Edge Detect Enable 0..1 */
static volatile u32 __iomem * const GPIO_GPFEN = (volatile u32 __iomem *) IO_ADDRESS(GPIO_BASE + 0x58);

static void gpio_input_tasklet_func(unsigned long data) {
	struct input_dev *dev = (struct input_dev *) data;
	input_sync(dev);
}

static irqreturn_t gpio_input_handler(int irq, void *dev_id) {
	if (!(ioread32(GPIO_GPEDS + BIT_WORD(PIN)) & BIT_MASK(PIN))) {
		return IRQ_NONE;
	}
	iowrite32(BIT_MASK(PIN), GPIO_GPEDS + BIT_WORD(PIN));
	int level = ioread32(GPIO_GPLEV + BIT_WORD(PIN)) & BIT_MASK(PIN);
	struct input_dev *dev = dev_id;
	if (dev->num_vals <= dev->max_vals - 4) {
		if (dev->num_vals > 0) {
			static const struct input_value input_value_sync = { EV_SYN, SYN_REPORT, 0 };
			dev->vals[dev->num_vals++] = input_value_sync;
		}
		struct input_value *val = &dev->vals[dev->num_vals++];
		val->type = EV_MSC;
		val->code = MSC_TIMESTAMP;
		val->value = ioread32(ST_CLO);
		val = &dev->vals[dev->num_vals++];
		val->type = EV_SW;
		val->code = SW_MAX;
		val->value = level;
	}
	else {
		if (dev->num_vals % 3 != 0) {
			static const struct input_value input_value_dropped = { EV_SYN, SYN_DROPPED, 0 };
			dev->vals[dev->num_vals++] = input_value_dropped;
		}
		++dev->vals[dev->num_vals - 1].value;
	}
	struct tasklet_struct *tasklet = input_get_drvdata(dev);
	tasklet_hi_schedule(tasklet);
	return IRQ_HANDLED;
}

static int gpio_input_open(struct input_dev *dev) {
	int ret;
	struct tasklet_struct *tasklet;
	if ((tasklet = kmalloc(sizeof *tasklet, GFP_KERNEL))) {
		tasklet_init(tasklet, gpio_input_tasklet_func, (unsigned long) dev);
		input_set_drvdata(dev, tasklet);
		int irq = platform_get_irq(to_platform_device(dev->dev.parent), 0);
		if ((ret = request_irq(irq, &gpio_input_handler, IRQF_SHARED, MODNAME, dev)) == 0) {
			iowrite32(ioread32(GPIO_GPREN + BIT_WORD(PIN)) | BIT_MASK(PIN),
					GPIO_GPREN + BIT_WORD(PIN));
			iowrite32(ioread32(GPIO_GPFEN + BIT_WORD(PIN)) | BIT_MASK(PIN),
					GPIO_GPFEN + BIT_WORD(PIN));
			return 0;
		}
		printk(KERN_ERR MODNAME ": failed to claim IRQ %d\n", irq);
		kfree(tasklet);
		input_set_drvdata(dev, NULL);
	}
	else {
		ret = -ENOMEM;
	}
	return ret;
}

static void gpio_input_close(struct input_dev *dev) {
	iowrite32(ioread32(GPIO_GPREN + BIT_WORD(PIN)) & ~BIT_MASK(PIN),
			GPIO_GPREN + BIT_WORD(PIN));
	iowrite32(ioread32(GPIO_GPFEN + BIT_WORD(PIN)) & ~BIT_MASK(PIN),
			GPIO_GPFEN + BIT_WORD(PIN));
	free_irq(platform_get_irq(to_platform_device(dev->dev.parent), 0), dev);
	struct tasklet_struct *tasklet = input_get_drvdata(dev);
	tasklet_kill(tasklet);
	kfree(tasklet);
	input_set_drvdata(dev, NULL);
}

static int __init gpio_input_probe(struct platform_device *dev) {
	int ret;
	struct input_dev *gpio_input_dev;
	if ((gpio_input_dev = input_allocate_device())) {
		gpio_input_dev->name = "GPIO Input";
		gpio_input_dev->dev.parent = &dev->dev;
		gpio_input_dev->evbit[BIT_WORD(EV_MSC)] = BIT_MASK(EV_MSC);
		gpio_input_dev->evbit[BIT_WORD(EV_SW)] |= BIT_MASK(EV_SW);
		gpio_input_dev->mscbit[BIT_WORD(MSC_TIMESTAMP)] = BIT_MASK(MSC_TIMESTAMP);
		gpio_input_dev->swbit[BIT_WORD(SW_MAX)] = BIT_MASK(SW_MAX);
		gpio_input_dev->hint_events_per_packet = 4096;
		gpio_input_dev->open = gpio_input_open;
		gpio_input_dev->close = gpio_input_close;
		if ((ret = input_register_device(gpio_input_dev)) == 0) {
			platform_set_drvdata(dev, gpio_input_dev);
			return 0;
		}
		input_free_device(gpio_input_dev);
	}
	else {
		ret = -ENOMEM;
	}
	return ret;
}

static int gpio_input_remove(struct platform_device *dev)
{
	struct input_dev *gpio_input_dev = platform_get_drvdata(dev);
	input_unregister_device(gpio_input_dev);
	platform_set_drvdata(dev, NULL);
	return 0;
}

static struct platform_driver gpio_input_driver = {
	.driver = {
		.name = MODNAME,
		.owner = THIS_MODULE,
	},
	.remove = gpio_input_remove,
};

static struct platform_device *gpio_input_device;

static int __init gpio_input_init(void) {
	int ret;
	static const struct resource gpio_input_resources[] = {
		{
			.start = IRQ_GPIO0,
			.end = IRQ_GPIO0,
			.name = "gpio_irq",
			.flags = IORESOURCE_IRQ,
		},
	};
	if (!IS_ERR(gpio_input_device = platform_device_register_simple(MODNAME, -1,
			gpio_input_resources, ARRAY_SIZE(gpio_input_resources)))) {
		if ((ret = platform_driver_probe(&gpio_input_driver, gpio_input_probe)) == 0) {
			return 0;
		}
		platform_device_unregister(gpio_input_device);
	}
	else {
		ret = PTR_ERR(gpio_input_device);
	}
	gpio_input_device = NULL;
	return ret;
}

static void __exit gpio_input_exit(void) {
	platform_device_unregister(gpio_input_device), gpio_input_device = NULL;
	platform_driver_unregister(&gpio_input_driver);
}

module_init(gpio_input_init);
module_exit(gpio_input_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Matt Whitlock");
MODULE_DESCRIPTION("Exposes a GPIO pin via the input subsystem");
