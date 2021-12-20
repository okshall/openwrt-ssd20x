/*
 * Driver for watchdog device controlled through GPIO-line
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/watchdog.h>

#define SOFT_TIMEOUT_MIN	1
#define SOFT_TIMEOUT_DEF	20
#define SOFT_TIMEOUT_MAX	0xffff
#define HW_MARGIN (1000) //1000ms
#define WTD_EN (13) //gpio13 
#define WTD_OUT (86) //gpio86

struct gpio_wdt_priv {
	int			gpio_en;
	int			gpio_out;
	bool			active_hight;
	bool			state;
	bool			always_running;
	bool			armed;
	unsigned int		hw_algo;
	unsigned int		hw_margin;
	unsigned long		last_jiffies;
	struct timer_list	timer;
	struct watchdog_device	wdd;
};

static void gpio_wdt_disable(struct gpio_wdt_priv *priv)
{
	gpio_set_value_cansleep(priv->gpio_en, !priv->active_hight);
}

static void gpio_wdt_hwping(unsigned long data)
{
	struct watchdog_device *wdd = (struct watchdog_device *)data;
	struct gpio_wdt_priv *priv = watchdog_get_drvdata(wdd);

	if (priv->armed && time_after(jiffies, priv->last_jiffies +
				      msecs_to_jiffies(wdd->timeout * 1000))) {
		dev_crit(wdd->parent,
			 "Timer expired. System will reboot soon!\n");
		return;
	}

	/* Restart timer */
	mod_timer(&priv->timer, jiffies + priv->hw_margin);

	gpio_set_value_cansleep(priv->gpio_out, priv->active_hight);
	mdelay(50);
	gpio_set_value_cansleep(priv->gpio_out, !priv->active_hight);
}

static void gpio_wdt_start_impl(struct gpio_wdt_priv *priv)
{
	priv->state = priv->active_hight;
	gpio_direction_output(priv->gpio_en, priv->state);
	gpio_direction_output(priv->gpio_out, priv->state);
	priv->last_jiffies = jiffies;
	gpio_wdt_hwping((unsigned long)&priv->wdd);
}

static int gpio_wdt_start(struct watchdog_device *wdd)
{
	struct gpio_wdt_priv *priv = watchdog_get_drvdata(wdd);
	gpio_wdt_start_impl(priv);
	priv->armed = true;

	return 0;
}

static int gpio_wdt_stop(struct watchdog_device *wdd)
{
	struct gpio_wdt_priv *priv = watchdog_get_drvdata(wdd);

	priv->armed = false;
	if (!priv->always_running) {
		mod_timer(&priv->timer, 0);
		gpio_wdt_disable(priv);
	}

	return 0;
}

static int gpio_wdt_ping(struct watchdog_device *wdd)
{
	struct gpio_wdt_priv *priv = watchdog_get_drvdata(wdd);

	priv->last_jiffies = jiffies;

	return 0;
}

static int gpio_wdt_set_timeout(struct watchdog_device *wdd, unsigned int t)
{
	wdd->timeout = t;

	return gpio_wdt_ping(wdd);
}

static const struct watchdog_info gpio_wdt_ident = {
	.options	= WDIOF_MAGICCLOSE | WDIOF_KEEPALIVEPING |
			  WDIOF_SETTIMEOUT,
	.identity	= "cn706t watchdog",
};

static const struct watchdog_ops gpio_wdt_ops = {
	.owner		= THIS_MODULE,
	.start		= gpio_wdt_start,
	.stop		= gpio_wdt_stop,
	.ping		= gpio_wdt_ping,
	.set_timeout	= gpio_wdt_set_timeout,
};
static int gpio_wdt_probe(struct platform_device *pdev)
{
	struct gpio_wdt_priv *priv;
	unsigned int hw_margin;
	int ret;

	priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	platform_set_drvdata(pdev, priv);


	priv->gpio_en = WTD_EN;
	if (!gpio_is_valid(priv->gpio_en))
		return priv->gpio_en;

	priv->gpio_out = WTD_OUT;
	if (!gpio_is_valid(priv->gpio_out))
		return priv->gpio_out;

	priv->active_hight = GPIOF_OUT_INIT_HIGH;
	
	ret = gpio_request(priv->gpio_en, "wtd_en");
	if (ret)
		return ret;
	ret = gpio_request(priv->gpio_out, "wtd_out");
	if (ret)
		return ret;

	hw_margin = HW_MARGIN;

	/* Disallow values lower than 2 and higher than 65535 ms */
	if (hw_margin < 2 || hw_margin > 65535)
		return -EINVAL;

	/* Use safe value (1/2 of real timeout) */
	priv->hw_margin = msecs_to_jiffies(hw_margin / 2);

	priv->always_running = 0;

	watchdog_set_drvdata(&priv->wdd, priv);

	priv->wdd.info		= &gpio_wdt_ident;
	priv->wdd.ops		= &gpio_wdt_ops;
	priv->wdd.min_timeout	= SOFT_TIMEOUT_MIN;
	priv->wdd.max_timeout	= SOFT_TIMEOUT_MAX;

	if (watchdog_init_timeout(&priv->wdd, 0, &pdev->dev) < 0)
		priv->wdd.timeout = SOFT_TIMEOUT_DEF;

	setup_timer(&priv->timer, gpio_wdt_hwping, (unsigned long)&priv->wdd);


	ret = watchdog_register_device(&priv->wdd);
	if (ret)
		return ret;

	if (priv->always_running)
		gpio_wdt_start_impl(priv);

	return 0;
}

static int gpio_wdt_remove(struct platform_device *pdev)
{
	struct gpio_wdt_priv *priv = platform_get_drvdata(pdev);

	del_timer_sync(&priv->timer);
	watchdog_unregister_device(&priv->wdd);

	return 0;
}

static const struct of_device_id gpio_wdt_dt_ids[] = {
	{ .compatible = "linux,cn706t-wdt", },
	{ }
};
MODULE_DEVICE_TABLE(of, gpio_wdt_dt_ids);

static struct platform_driver gpio_wdt_driver = {
	.driver	= {
		.name		= "cn706t-wdt",
		.of_match_table	= gpio_wdt_dt_ids,
	},
	.probe	= gpio_wdt_probe,
	.remove	= gpio_wdt_remove,
};
module_platform_driver(gpio_wdt_driver);
MODULE_AUTHOR("zhaowenfu");
MODULE_DESCRIPTION("n76e003 Watchdog");
MODULE_LICENSE("GPL");

