// Reads CLIDR_EL1 and the CCSIDR_EL1 of every cache level on cpu0 (Silver) and cpu7 (Gold). Read-only; it writes only CSSELR_EL1, and
// returns -EAGAIN from init so nothing stays loaded. Build against the kernel tree that runs on the phone (vermagic must match):
//   make -C <linux> O=<obj> M=<dir with this file and a Makefile "obj-m := ccsidr_dump.o"> ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- modules
// Load as root, read the result with dmesg | grep ccsidr. Used on 2026-11-24 for the cache sizes in kernel/mainline/dts (NOTES.md).
#include <linux/module.h>
#include <linux/smp.h>
#include <linux/cpumask.h>
static void rd(void *arg)
{
	u64 clidr, mm2, v, sel;
	int lvl, ind, ctype, ccidx;
	asm volatile("mrs %0, clidr_el1" : "=r"(clidr));
	asm volatile("mrs %0, id_aa64mmfr2_el1" : "=r"(mm2));
	ccidx = (mm2 >> 20) & 0xf;
	pr_info("ccsidr cpu%d clidr=%#llx ccidx=%d\n", smp_processor_id(), clidr, ccidx);
	for (lvl = 0; lvl < 7; lvl++) {
		ctype = (clidr >> (3 * lvl)) & 7;
		if (!ctype)
			break;
		for (ind = 0; ind < 2; ind++) {
			if (ind == 1 && ctype != 1 && ctype != 3)
				continue; /* the I side exists only for ctype 1 and 3 */
			if (ind == 0 && ctype == 1)
				continue; /* an I-only level has no D side */
			sel = ((u64)lvl << 1) | ind;
			asm volatile("msr csselr_el1, %0\n isb" : : "r"(sel));
			asm volatile("mrs %0, ccsidr_el1" : "=r"(v));
			{
				u64 line = 1ULL << ((v & 7) + 4), assoc, sets;
				if (ccidx) { assoc = ((v >> 3) & 0x1fffff) + 1; sets = ((v >> 32) & 0xffffff) + 1; }
				else { assoc = ((v >> 3) & 0x3ff) + 1; sets = ((v >> 13) & 0x7fff) + 1; }
				pr_info("ccsidr cpu%d L%d %s raw=%#llx line=%llu ways=%llu sets=%llu size=%llu KB\n", smp_processor_id(), lvl + 1,
					ind ? "I" : (ctype == 4 ? "U" : "D"), v, line, assoc, sets, line * assoc * sets / 1024);
			}
		}
	}
}
static int __init m_init(void)
{
	smp_call_function_single(0, rd, NULL, 1);
	smp_call_function_single(7, rd, NULL, 1);
	return -EAGAIN; /* nothing stays loaded */
}
module_init(m_init);
MODULE_LICENSE("GPL");
