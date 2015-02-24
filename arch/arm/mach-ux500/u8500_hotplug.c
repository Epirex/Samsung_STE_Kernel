/*
 * arch/arm/mach-ux500/u8500_hotplug.c
 *
 * Copyright (c) 2014, Zhao Wei Liew <zhaoweiliew@gmail.com>.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/earlysuspend.h>
#include <linux/module.h>
#include <linux/workqueue.h>
#include <linux/cpu.h>
#include <linux/init.h>
#include <linux/cpufreq.h>
#include <linux/input.h>
#include <linux/hrtimer.h>
#include <linux/slab.h>

#define DISABLED 0

static struct work_struct suspend_work;
static struct work_struct resume_work;

static unsigned int max_freq = LONG_MAX;
static bool update_freq = false;

static unsigned int suspend_max_freq = DISABLED;
module_param(suspend_max_freq, uint, 0644);

unsigned int input_boost_freq = 400000;
module_param(input_boost_freq, uint, 0644);

unsigned int input_boost_ms = 40;
module_param(input_boost_ms, uint, 0644);

u64 last_input_time;
#define MIN_INPUT_INTERVAL (150 * USEC_PER_MSEC)

static int cpufreq_callback(struct notifier_block *nfb,
		unsigned long event, void *data)
{
	struct cpufreq_policy *policy = data;

	if (event != CPUFREQ_ADJUST || !update_freq)
		return 0;

	if (policy->max == max_freq)
		return 0;

	cpufreq_verify_within_limits(policy,
		policy->min, max_freq);

	return 0;
}

static struct notifier_block cpufreq_notifier_block = {
	.notifier_call = cpufreq_callback,
};

static void max_freq_limit(bool suspend)
{
	int cpu;

	if (suspend && max_freq == suspend_max_freq)
		return;

	max_freq = suspend ? suspend_max_freq : LONG_MAX;

	update_freq = true;

	for_each_online_cpu(cpu)
		cpufreq_update_policy(cpu);

	update_freq = false;
}

static void suspend_work_fn(struct work_struct *work)
{
	int cpu;

	for_each_online_cpu(cpu) {
		if (!cpu)
			continue;

		cpu_down(cpu);
	}

	if (suspend_max_freq)
		max_freq_limit(true);

}

static void resume_work_fn(struct work_struct *work)
{
	int cpu;

	max_freq_limit(false);

	for_each_possible_cpu(cpu) {
		if (!cpu)
			continue;

		cpu_up(cpu);
	}
}

static void u8500_hotplug_suspend(struct early_suspend *handler)
{
	schedule_work(&suspend_work);
}

static void u8500_hotplug_resume(struct early_suspend *handler)
{
	schedule_work(&resume_work);
}

static struct early_suspend u8500_hotplug_early_suspend = {
	.level = EARLY_SUSPEND_LEVEL_DISABLE_FB,
	.suspend = u8500_hotplug_suspend,
	.resume = u8500_hotplug_resume,
};
static void hotplug_input_event(struct input_handle *handle,
		unsigned int type, unsigned int code, int value)
{
	u64 now;

	if (!input_boost_freq)
		return;

	now = ktime_to_us(ktime_get());
	if (now - last_input_time < MIN_INPUT_INTERVAL)
		return;

	last_input_time = ktime_to_us(ktime_get());
}

static int hotplug_input_connect(struct input_handler *handler,
		struct input_dev *dev, const struct input_device_id *id)
{
	struct input_handle *handle;
	int error;

	handle = kzalloc(sizeof(struct input_handle), GFP_KERNEL);
	if (!handle)
		return -ENOMEM;

	handle->dev = dev;
	handle->handler = handler;
	handle->name = "cpufreq";

	error = input_register_handle(handle);
	if (error)
		goto err2;

	error = input_open_device(handle);
	if (error)
		goto err1;

	return 0;
err1:
	input_unregister_handle(handle);
err2:
	kfree(handle);
	return error;
}

static void hotplug_input_disconnect(struct input_handle *handle)
{
	input_close_device(handle);
	input_unregister_handle(handle);
	kfree(handle);
}

static const struct input_device_id hotplug_ids[] = {
	/* multi-touch touchscreen */
	{
		.flags = INPUT_DEVICE_ID_MATCH_EVBIT |
			INPUT_DEVICE_ID_MATCH_ABSBIT,
		.evbit = { BIT_MASK(EV_ABS) },
		.absbit = { [BIT_WORD(ABS_MT_POSITION_X)] =
			BIT_MASK(ABS_MT_POSITION_X) |
			BIT_MASK(ABS_MT_POSITION_Y) },
	},
	/* touchpad */
	{
		.flags = INPUT_DEVICE_ID_MATCH_KEYBIT |
			INPUT_DEVICE_ID_MATCH_ABSBIT,
		.keybit = { [BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH) },
		.absbit = { [BIT_WORD(ABS_X)] =
			BIT_MASK(ABS_X) | BIT_MASK(ABS_Y) },
	},
	/* Keypad */
	{
		.flags = INPUT_DEVICE_ID_MATCH_EVBIT,
		.evbit = { BIT_MASK(EV_KEY) },
	},
	{ },
};

static struct input_handler hotplug_input_handler = {
	.event          = hotplug_input_event,
	.connect        = hotplug_input_connect,
	.disconnect     = hotplug_input_disconnect,
	.name           = "u8500_hotplug",
	.id_table       = hotplug_ids,
};

static int u8500_hotplug_init(void)
{
	int ret;

	INIT_WORK(&suspend_work, suspend_work_fn);
	INIT_WORK(&resume_work, resume_work_fn);

	register_early_suspend(&u8500_hotplug_early_suspend);

	cpufreq_register_notifier(&cpufreq_notifier_block, CPUFREQ_POLICY_NOTIFIER);

	ret = input_register_handler(&hotplug_input_handler);
	if (ret)
		pr_err("Cannot register hotplug input handler.\n");

	return ret;
}

late_initcall(u8500_hotplug_init);

static void u8500_hotplug_exit(void)
{
	unregister_early_suspend(&u8500_hotplug_early_suspend);
}

module_exit(u8500_hotplug_exit);

MODULE_AUTHOR("Zhao Wei Liew <zhaoweiliew@gmail.com>");
MODULE_DESCRIPTION("Hotplug driver for U8500");
MODULE_LICENSE("GPL");
