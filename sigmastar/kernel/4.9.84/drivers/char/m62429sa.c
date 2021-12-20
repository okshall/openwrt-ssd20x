#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>       // struct file_operations
#include <linux/cdev.h>     // struct cdev
#include <linux/uaccess.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>

MODULE_DESCRIPTION("NEC/Renesas M62429 Digital Volume Control Driver");
MODULE_AUTHOR("zhaowenfu");
MODULE_LICENSE("GPL");
MODULE_VERSION("V0.0.1");

static char* name = "m62429sa";
static uint8_t chan = 0;
static struct class *cls0, *cls1;

#define CLK_GPIO (6)
#define DATA_GPIO (7)
static int data[2];
static int init_gpio(void)
{
	int ret;
	ret = gpio_request(CLK_GPIO, "clk_gpio");
	if (ret) {
		printk("request clk gpio error\r\n");
		return ret;
	}

	ret = gpio_request(DATA_GPIO, "data_gpio");
	if (ret) {
		printk("request data gpio error\r\n");
	  return ret;
	}

	ret = gpio_direction_output(CLK_GPIO, 0);
	if (ret) {
		return ret;
	}

	ret = gpio_direction_output(DATA_GPIO, 0);
	if (ret) {
		return ret;
	}
	return 0;
}

static int free_gpio(void)
{
	gpio_free(CLK_GPIO);
	gpio_free(DATA_GPIO);
	return 0;
}

static uint16_t setvolume(int8_t volume, uint8_t chan, uint8_t both)
{
	uint8_t bits; // 11 bit control
	uint16_t atten; // volume converted to attenuation
	uint16_t data; // control word is built by OR-ing in the bits

	// constrain volume to 0...100
	volume = volume < 0 ? 0 : volume > 100 ? 100 : volume;

	// convert volume 0...100 to attenuation range 0...84
	atten = ((volume * 84) / 100);

	// initialize (clear) data
	data = 0;

	data |= chan ? (1 << 0) : (0 << 0); // D0 (channel select: 0=ch1, 1=ch2)
	data |= both ? (1 << 1) : (0 << 1); // D1 (individual/both select: 0=both, 1=individual)
	data |= (atten & (0x1f << 2)); // D2...D6 (0...84 in steps of 4)
	data |= ((atten << 7) & (0x3 << 7)); // D7 & D8 (0...3)
	data |= (1 << 9); // D9 and...
	data |= (1 << 10); // ...D10 must both be 1

	for (bits = 0; bits < 11; bits++) { // send out 10 control bits
		udelay(2);
		gpio_set_value(DATA_GPIO, 0);
		udelay(2);
		gpio_set_value(CLK_GPIO, 0);
		
		udelay(2);
		if (data & (1 << bits)) {
			gpio_set_value(DATA_GPIO, 1);
		} else {
			gpio_set_value(DATA_GPIO, 0);
		}
		udelay(2);
		gpio_set_value(CLK_GPIO, 1);
	}
	// send final (11th) bit
	udelay(2);
	gpio_set_value(DATA_GPIO, 1);
	udelay(2);
	gpio_set_value(CLK_GPIO, 0);

	return data; // return bit pattern in case you want it :)
}
static int m62429_open(struct inode *inode,
			struct file *file)
{
	chan = MINOR(inode->i_rdev);
	return 0;
}
int m62429_release(struct inode *inode, struct file *filp)
{
    return 0;
}


static int m62429_close(struct inode *inode,
			struct file *file)
{
	return 0;
}
#define BUF_LEN (10)
static ssize_t m62429_read(struct file *file,
			char __user *buf,
			size_t count,
			loff_t *ppos)
{
	char m[BUF_LEN];
	ssize_t bytes = count < (BUF_LEN-(*ppos)) ? count : (BUF_LEN-(*ppos));	
	
	memset(m, 0x0, sizeof(m));
	sprintf(m, "%d %d\n", chan, data[chan]);
	if (copy_to_user(buf, m, bytes)) {
	  return -EFAULT;
	}
	*ppos += bytes;
	return bytes;
}

#define atoi(s)             simple_strtol(s, NULL, 10)
static ssize_t m62429_write(struct file *file,
			const char __user *buf,
			size_t count,
			loff_t *ppos)
{
	char m[32];
	int8_t volume;

	memset(m, 0x0, sizeof(m));
	if(copy_from_user(m, buf, count)) {
	  return -EFAULT;

	} else {
		*ppos += count;
	}
	volume = atoi(m);
	setvolume(volume, chan, 1);
	
	data[chan] = volume;
	return count;
}



static struct file_operations m62429_fops = {
	.owner = THIS_MODULE,
	.open = m62429_open,     // 打开设备
	.release = m62429_close,  // 关闭设备
	.write = m62429_write,
	.read = m62429_read,
	.release = m62429_release
};

static struct cdev m62429_cdev;

static dev_t dev;
static int major;
#define DEV0_NAME "m62429-0"
#define DEV1_NAME "m62429-1"
static int __init m62429_init(void)
{
	int rc = -1;

	rc = alloc_chrdev_region(&dev, 0, 2, name);
	if (rc != 0) {
		return -1;
	}
	major = MAJOR(dev);

	// 自动创建 device0
	cls0 = class_create(THIS_MODULE, DEV0_NAME);
	device_create(cls0, NULL, MKDEV(major, 0), NULL, DEV0_NAME);

	// 自动创建 device1
	cls1 = class_create(THIS_MODULE, DEV1_NAME);
	device_create(cls1, NULL, MKDEV(major, 1), NULL, DEV1_NAME);


	// 初始化字符设备对象
	cdev_init(&m62429_cdev, &m62429_fops);

	// 注册字符设备对象
	cdev_add(&m62429_cdev, dev, 2);
	
	init_gpio();
	
	return 0;
}

static void __exit m62429_exit(void)
{
	free_gpio();
	// 删除设备文件
	device_destroy(cls0, MKDEV(major, 0));
	// 删除设备类
	class_destroy(cls0);
	// 删除设备文件
	device_destroy(cls1, MKDEV(major, 1));
	// 删除设备类
	class_destroy(cls1);
	// 卸载字符设备对象
	cdev_del(&m62429_cdev);

	// 释放设备号
	unregister_chrdev_region(dev, 2);
}

module_init(m62429_init);
module_exit(m62429_exit);

