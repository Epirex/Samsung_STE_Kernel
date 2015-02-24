/*
 * arch/arm/mach-ux500/u8500_hotplug.h
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

#ifndef __U8500_HOTPLUG_H
#define __U8500_HOTPLUG_H

#ifdef CONFIG_U8500_HOTPLUG
extern u64 last_input_time;
extern unsigned int input_boost_ms;
extern unsigned int input_boost_freq;
#else
static u64 last_input_time = 0;
static unsigned int input_boost_ms = 0;
static unsigned int input_boost_freq = 0;
#endif
#endif
