/*
 * This file is part of the coreboot project.
 *
  * Copyright (C) 2011-2012  Alexandru Gagniuc <mr.nuke.me@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Inspired from the EPIA-M700
 */
#undef CONFIG_COLLECT_TIMESTAMPS
#define CONFIG_COLLECT_TIMESTAMPS 1
#include <stdint.h>
#include <device/pci_def.h>
#include <device/pci_ids.h>
#include <arch/io.h>
#include <device/pnp_def.h>
#include <arch/io.h>
#include <arch/hlt.h>
#include <console/console.h>
#include <lib.h>
#include <cpu/x86/bist.h>
#include <string.h>
#include <timestamp.h>
#include <console/cbmem_console.h>

#include "northbridge/via/vx900/early_vx900.h"
#include "northbridge/via/vx900/raminit.h"
/* FIXME: This is the only .c include we couldn't get rid of */
#include "superio/fintek/f81865f/f81865f_early_serial.c"

#define SERIAL_DEV PNP_DEV(0x4e, 0x10)

static inline u64 tsc2u64(tsc_t tsc)
{
	return ((u64) tsc.hi << 32) | tsc.lo;
}

/* FIXME: This board comes in two flavours. This is for the faster CPU, but
 * will probably not be correct for the other CPU */
#define TSC_PER_USEC 1297
static inline u32 tsc2ms(u64 end, u64 start)
{
	return ((u32) (end - start) / TSC_PER_USEC) / 1000;
}

/* cache_as_ram.inc jumps to here. */
void main(unsigned long bist)
{
	u32 tolm;
	u64 start, end;
	tsc_t tsc_at_romstage_start = rdtsc();

	/* First thing we need to do on the VX900, before anything else */
	vx900_enable_pci_config_space();

	/* Serial console is easy to take care of */
	f81865f_enable_serial(SERIAL_DEV, CONFIG_TTYS0_BASE);
	console_init();
	print_debug("Console initialized. \n");

	vx900_cpu_bus_interface_setup();

	/* Be smart. Get this info */
	vx900_print_strapping_info();
	/* DEVEL helper */
	vx900_disable_auto_reboot();
	/* Halt if there was a built-in self test failure. */
	report_bist_failure(bist);

	/* Oh, almighty, give us the SMBUS */
	enable_smbus();

	tsc_t tsc_before_raminit = rdtsc();
	/* Now we can worry about raminit.
	 * This board only has DDR3, so no need to worry about which DRAM type
	 * to use */
	dimm_layout dimms = { {0x50, 0x51, SPD_END_LIST} };
	vx900_init_dram_ddr3(&dimms);
	tsc_t tsc_after_raminit = rdtsc();

	/* TODO: All these ram_checks are here to ensure we test most of the RAM
	 * below 4G. They should not be needed once VX900 raminit is stable */
	ram_check(0, 0x80);
	ram_check(512 << 10, 0x80);
	ram_check((1 << 20) - (1 << 10), 0x80);
	ram_check((1 << 24), 0x80);
	ram_check((512 + 256 - 1) << 20, 0x80);
	ram_check(0x80c0000, 0x80);
	tolm = ((pci_read_config16(MCU, 0x84) & 0xfff0) >> 4) << 20;
	if (tolm > (1 * (u32) GiB))
		ram_check(1024 << 10, 0x80);
	if (tolm > (2 * (u32) GiB))
		ram_check(2048 << 20, 0x80);

	print_debug("We passed RAM verify\n");
#ifdef GONFIG_EARLY_CBMEM_INIT
	/* We got RAM working, now we can write the timestamps to RAM */
	cbmem_initialize();
	timestamp_init(tsc_at_romstage_start);
	timestamp_add(TS_START_ROMSTAGE, tsc_at_romstage_start);
	timestamp_add(TS_BEFORE_INITRAM, tsc_before_raminit);
	timestamp_add(TS_AFTER_INITRAM, tsc_after_raminit);
	timestamp_add_now(TS_END_ROMSTAGE);
#endif
	/* FIXME: See if this is needed or take this out please */
	/* Disable Memcard and SDIO */
	pci_mod_config8(LPC, 0x51, 0, (1 << 7) | (1 << 4));

	/* Informative character. Could be removed at a later time. */
	start = tsc2u64(tsc_at_romstage_start);
	end = tsc2u64(tsc_before_raminit);
	printk(BIOS_INFO, "Before raminit %ums\n", tsc2ms(end, start));

	start = end;
	end = tsc2u64(tsc_after_raminit);
	printk(BIOS_INFO, "Actual Raminit %ums\n", tsc2ms(end, start));
}