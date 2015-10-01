/*
 * This file is part of the coreboot project.
 *
 * Copyright 2012 Google Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc.
 */

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>
#include <getopt.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <ctype.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <libgen.h>
#include <assert.h>

#ifdef __OpenBSD__
#include <sys/param.h>
#include <sys/sysctl.h>
#endif

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#define MAP_BYTES (1024*1024)
#define IS_ENABLED(x) (defined (x) && (x))

#include "coreboot_tables.h"

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

#include "cbmem_id.h"
#include "timestamp.h"

#define CBMEM_VERSION "1.1"

/* verbose output? */
static int verbose = 0;
#define debug(x...) if(verbose) printf(x)

/* File handle used to access /dev/mem */
static int mem_fd;

/* IMD root pointer location */
static uint64_t rootptr = 0;

/*
 * calculate ip checksum (16 bit quantities) on a passed in buffer. In case
 * the buffer length is odd last byte is excluded from the calculation
 */
static u16 ipchcksum(const void *addr, unsigned size)
{
	const u16 *p = addr;
	unsigned i, n = size / 2; /* don't expect odd sized blocks */
	u32 sum = 0;

	for (i = 0; i < n; i++)
		sum += p[i];

	sum = (sum >> 16) + (sum & 0xffff);
	sum += (sum >> 16);
	sum = ~sum & 0xffff;
	return (u16) sum;
}

/*
 * Functions to map / unmap physical memory into virtual address space. These
 * functions always maps 1MB at a time and can only map one area at once.
 */
static void *mapped_virtual;
static size_t mapped_size;

static inline size_t size_to_mib(size_t sz)
{
	return sz >> 20;
}

static void unmap_memory(void)
{
	if (mapped_virtual == NULL) {
		fprintf(stderr, "Error unmapping memory\n");
		return;
	}
	if (size_to_mib(mapped_size) == 0) {
		debug("Unmapping %zuMB of virtual memory at %p.\n",
		      size_to_mib(mapped_size), mapped_virtual);
	}
	else {
		debug("Unmapping %zuMB of virtual memory at %p.\n",
		      size_to_mib(mapped_size), mapped_virtual);
	}
	munmap(mapped_virtual, mapped_size);
	mapped_virtual = NULL;
	mapped_size = 0;
}

static void *map_memory_size(u64 physical, size_t size)
{
	void *v;
	off_t p;
	u64 page = getpagesize();
	size_t padding;

	if (mapped_virtual != NULL)
		unmap_memory();

	/* Mapped memory must be aligned to page size */
	p = physical & ~(page - 1);
	padding = physical & (page-1);
	size += padding;

	if (size_to_mib(size) == 0) {
		debug("Mapping %zuB of physical memory at 0x%jx (requested 0x%jx).\n",
		      size, (intmax_t)p, (intmax_t)physical);
	}
	else {
		debug("Mapping %zuMB of physical memory at 0x%jx (requested 0x%jx).\n",
		      size_to_mib(size), (intmax_t)p, (intmax_t)physical);
	}

	v = mmap(NULL, size, PROT_READ, MAP_SHARED, mem_fd, p);

	if (v == MAP_FAILED) {
		/* The mapped area may have overrun the upper cbmem boundary when trying to
		 * align to the page size.  Try growing down instead of up...
		 */
		p -= page;
		padding += page;
		size &= ~(page - 1);
		size = size + (page - 1);
		v = mmap(NULL, size, PROT_READ, MAP_SHARED, mem_fd, p);
		debug("  ... failed.  Mapping %zuB of physical memory at 0x%jx.\n",
		      size, (intmax_t)p);
	}

	if (v == MAP_FAILED) {
		fprintf(stderr, "Failed to mmap /dev/mem: %s\n",
			strerror(errno));
		exit(1);
	}

	/* Remember what we actually mapped ... */
	mapped_virtual = v;
	mapped_size = size;

	/* ... but return address to the physical memory that was requested */
	if (padding)
		debug("  ... padding virtual address with 0x%zx bytes.\n",
			padding);
	v += padding;

	return v;
}

static void *map_memory(u64 physical)
{
	return map_memory_size(physical, MAP_BYTES);
}

/*
 * Try finding the timestamp table and coreboot cbmem console starting from the
 * passed in memory offset.  Could be called recursively in case a forwarding
 * entry is found.
 *
 * Returns pointer to a memory buffer containg the timestamp table or zero if
 * none found.
 */

static struct lb_cbmem_ref timestamps;
static struct lb_cbmem_ref console;
static struct lb_memory_range cbmem;

/* This is a work-around for a nasty problem introduced by initially having
 * pointer sized entries in the lb_cbmem_ref structures. This caused problems
 * on 64bit x86 systems because coreboot is 32bit on those systems.
 * When the problem was found, it was corrected, but there are a lot of
 * systems out there with a firmware that does not produce the right
 * lb_cbmem_ref structure. Hence we try to autocorrect this issue here.
 */
static struct lb_cbmem_ref parse_cbmem_ref(struct lb_cbmem_ref *cbmem_ref)
{
	struct lb_cbmem_ref ret;

	ret = *cbmem_ref;

	if (cbmem_ref->size < sizeof(*cbmem_ref))
		ret.cbmem_addr = (uint32_t)ret.cbmem_addr;

	debug("      cbmem_addr = %" PRIx64 "\n", ret.cbmem_addr);

	return ret;
}

static int parse_cbtable(u64 address, size_t table_size)
{
	int i, found = 0;
	void *buf;

	debug("Looking for coreboot table at %" PRIx64 " %zd bytes.\n",
		address, table_size);
	buf = map_memory_size(address, table_size);

	/* look at every 16 bytes within 4K of the base */

	for (i = 0; i < 0x1000; i += 0x10) {
		struct lb_header *lbh;
		struct lb_record* lbr_p;
		void *lbtable;
		int j;

		lbh = (struct lb_header *)(buf + i);
		if (memcmp(lbh->signature, "LBIO", sizeof(lbh->signature)) ||
		    !lbh->header_bytes ||
		    ipchcksum(lbh, sizeof(*lbh))) {
			continue;
		}
		lbtable = buf + i + lbh->header_bytes;

		if (ipchcksum(lbtable, lbh->table_bytes) !=
		    lbh->table_checksum) {
			debug("Signature found, but wrong checksum.\n");
			continue;
		}

		found = 1;
		debug("Found!\n");

		for (j = 0; j < lbh->table_bytes; j += lbr_p->size) {
			lbr_p = (struct lb_record*) ((char *)lbtable + j);
			debug("  coreboot table entry 0x%02x\n", lbr_p->tag);
			switch (lbr_p->tag) {
			case LB_TAG_MEMORY: {
				int i = 0;
				debug("    Found memory map.\n");
				struct lb_memory *memory =
						(struct lb_memory *)lbr_p;
				while ((char *)&memory->map[i] < ((char *)lbr_p
							    + lbr_p->size)) {
					if (memory->map[i].type == LB_MEM_TABLE) {
						debug("      LB_MEM_TABLE found.\n");
						/* The last one found is CBMEM */
						cbmem = memory->map[i];
					}
					i++;
				}
				continue;
			}
			case LB_TAG_TIMESTAMPS: {
				debug("    Found timestamp table.\n");
				timestamps = parse_cbmem_ref((struct lb_cbmem_ref *) lbr_p);
				continue;
			}
			case LB_TAG_CBMEM_CONSOLE: {
				debug("    Found cbmem console.\n");
				console = parse_cbmem_ref((struct lb_cbmem_ref *) lbr_p);
				continue;
			}
			case LB_TAG_FORWARD: {
				/*
				 * This is a forwarding entry - repeat the
				 * search at the new address.
				 */
				struct lb_forward lbf_p =
					*(struct lb_forward *) lbr_p;
				debug("    Found forwarding entry.\n");
				unmap_memory();
				return parse_cbtable(lbf_p.forward, table_size);
			}
			default:
				break;
			}

		}
	}
	unmap_memory();

	return found;
}

#if defined(linux) && (defined(__i386__) || defined(__x86_64__))
/*
 * read CPU frequency from a sysfs file, return an frequency in Kilohertz as
 * an int or exit on any error.
 */
static unsigned long arch_tick_frequency(void)
{
	FILE *cpuf;
	char freqs[100];
	int  size;
	char *endp;
	u64 rv;

	const char* freq_file =
		"/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq";

	cpuf = fopen(freq_file, "r");
	if (!cpuf) {
		fprintf(stderr, "Could not open %s: %s\n",
			freq_file, strerror(errno));
		exit(1);
	}

	memset(freqs, 0, sizeof(freqs));
	size = fread(freqs, 1, sizeof(freqs), cpuf);
	if (!size || (size == sizeof(freqs))) {
		fprintf(stderr, "Wrong number of bytes(%d) read from %s\n",
			size, freq_file);
		exit(1);
	}
	fclose(cpuf);
	rv = strtoull(freqs, &endp, 10);

	if (*endp == '\0' || *endp == '\n')
		return rv;
	fprintf(stderr, "Wrong formatted value ^%s^ read from %s\n",
		freqs, freq_file);
	exit(1);
}
#elif defined(__OpenBSD__) && (defined(__i386__) || defined(__x86_64__))
static unsigned long arch_tick_frequency(void)
{
	int mib[2] = { CTL_HW, HW_CPUSPEED };
	static int value = 0;
	size_t value_len = sizeof(value);

	/* Return 1 MHz when sysctl fails. */
	if ((value == 0) && (sysctl(mib, 2, &value, &value_len, NULL, 0) == -1))
		return 1;

	return value;
}
#else
static unsigned long arch_tick_frequency(void)
{
	/* 1 MHz = 1us. */
	return 1;
}
#endif

static unsigned long tick_freq_mhz;

static void timestamp_set_tick_freq(unsigned long table_tick_freq_mhz)
{
	tick_freq_mhz = table_tick_freq_mhz;

	/* Honor table frequency. */
	if (tick_freq_mhz)
		return;

	tick_freq_mhz = arch_tick_frequency();

	if (!tick_freq_mhz) {
		fprintf(stderr, "Cannot determine timestamp tick frequency.\n");
		exit(1);
	}
}

u64 arch_convert_raw_ts_entry(u64 ts)
{
	return ts / tick_freq_mhz;
}

/*
 * Print an integer in 'normalized' form - with commas separating every three
 * decimal orders.
 */
static void print_norm(u64 v)
{
	if (v >= 1000) {
		/* print the higher order sections first */
		print_norm(v / 1000);
		printf(",%3.3u", (u32)(v % 1000));
	} else {
		printf("%u", (u32)(v % 1000));
	}
}

enum additional_timestamp_id {
	// Depthcharge entry IDs start at 1000.
	TS_DC_START = 1000,

	TS_RO_PARAMS_INIT = 1001,
	TS_RO_VB_INIT = 1002,
	TS_RO_VB_SELECT_FIRMWARE = 1003,
	TS_RO_VB_SELECT_AND_LOAD_KERNEL = 1004,

	TS_RW_VB_SELECT_AND_LOAD_KERNEL = 1010,

	TS_VB_SELECT_AND_LOAD_KERNEL = 1020,

	TS_CROSSYSTEM_DATA = 1100,
	TS_START_KERNEL = 1101
};

static const struct timestamp_id_to_name {
	u32 id;
	const char *name;
} timestamp_ids[] = {
	/* Marker to report base_time. */
	{ 0,			"1st timestamp" },
	{ TS_START_ROMSTAGE,	"start of rom stage" },
	{ TS_BEFORE_INITRAM,	"before ram initialization" },
	{ TS_AFTER_INITRAM,	"after ram initialization" },
	{ TS_END_ROMSTAGE,	"end of romstage" },
	{ TS_START_VBOOT,	"start of verified boot" },
	{ TS_END_VBOOT,		"end of verified boot" },
	{ TS_START_COPYRAM,	"starting to load ramstage" },
	{ TS_END_COPYRAM,	"finished loading ramstage" },
	{ TS_START_RAMSTAGE,	"start of ramstage" },
	{ TS_START_BOOTBLOCK,	"start of bootblock" },
	{ TS_END_BOOTBLOCK,	"end of bootblock" },
	{ TS_START_COPYROM,	"starting to load romstage" },
	{ TS_END_COPYROM,	"finished loading romstage" },
	{ TS_START_ULZMA,	"starting LZMA decompress (ignore for x86)" },
	{ TS_END_ULZMA,		"finished LZMA decompress (ignore for x86)" },
	{ TS_DEVICE_ENUMERATE,	"device enumeration" },
	{ TS_DEVICE_CONFIGURE,	"device configuration" },
	{ TS_DEVICE_ENABLE,	"device enable" },
	{ TS_DEVICE_INITIALIZE,	"device initialization" },
	{ TS_DEVICE_DONE,	"device setup done" },
	{ TS_CBMEM_POST,	"cbmem post" },
	{ TS_WRITE_TABLES,	"write tables" },
	{ TS_LOAD_PAYLOAD,	"load payload" },
	{ TS_ACPI_WAKE_JUMP,	"ACPI wake jump" },
	{ TS_SELFBOOT_JUMP,	"selfboot jump" },

	{ TS_START_COPYVER,	"starting to load verstage" },
	{ TS_END_COPYVER,	"finished loading verstage" },
	{ TS_START_TPMINIT,	"starting to initialize TPM" },
	{ TS_END_TPMINIT,	"finished TPM initialization" },
	{ TS_START_VERIFY_SLOT,	"starting to verify keyblock/preamble (RSA)" },
	{ TS_END_VERIFY_SLOT,	"finished verifying keyblock/preamble (RSA)" },
	{ TS_START_HASH_BODY,	"starting to verify body (load+SHA2+RSA) " },
	{ TS_DONE_LOADING,	"finished loading body (ignore for x86)" },
	{ TS_DONE_HASHING,	"finished calculating body hash (SHA2)" },
	{ TS_END_HASH_BODY,	"finished verifying body signature (RSA)" },

	{ TS_DC_START,		"depthcharge start" },
	{ TS_RO_PARAMS_INIT,	"RO parameter init" },
	{ TS_RO_VB_INIT,	"RO vboot init" },
	{ TS_RO_VB_SELECT_FIRMWARE,		"RO vboot select firmware" },
	{ TS_RO_VB_SELECT_AND_LOAD_KERNEL,	"RO vboot select&load kernel" },
	{ TS_RW_VB_SELECT_AND_LOAD_KERNEL,	"RW vboot select&load kernel" },
	{ TS_VB_SELECT_AND_LOAD_KERNEL,		"vboot select&load kernel" },
	{ TS_CROSSYSTEM_DATA,	"crossystem data" },
	{ TS_START_KERNEL,	"start kernel" },

	/* FSP related timestamps */
	{ TS_FSP_MEMORY_INIT_START, "calling FspMemoryInit" },
	{ TS_FSP_MEMORY_INIT_END, "returning from FspMemoryInit" },
	{ TS_FSP_TEMP_RAM_EXIT_START, "calling FspTempRamExit" },
	{ TS_FSP_TEMP_RAM_EXIT_END, "returning from FspTempRamExit" },
	{ TS_FSP_SILICON_INIT_START, "calling FspSiliconInit" },
	{ TS_FSP_SILICON_INIT_END, "returning from FspSiliconInit" },
	{ TS_FSP_BEFORE_ENUMERATE, "calling FspNotify(AfterPciEnumeration)" },
	{ TS_FSP_AFTER_ENUMERATE,
		 "returning from FspNotify(AfterPciEnumeration)" },
	{ TS_FSP_BEFORE_FINALIZE, "calling FspNotify(ReadyToBoot)" },
	{ TS_FSP_AFTER_FINALIZE, "returning from FspNotify(ReadyToBoot)" }
};

static const char *timestamp_name(uint32_t id)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(timestamp_ids); i++) {
		if (timestamp_ids[i].id == id)
			return timestamp_ids[i].name;
	}
	return "<unknown>";
}

static uint64_t timestamp_print_parseable_entry(uint32_t id, uint64_t stamp,
						uint64_t prev_stamp)
{
	const char *name;
	uint64_t step_time;

	name = timestamp_name(id);

	step_time = arch_convert_raw_ts_entry(stamp - prev_stamp);

	/* ID<tab>absolute time<tab>relative time<tab>description */
	printf("%d\t", id);
	printf("%llu\t", (long long)arch_convert_raw_ts_entry(stamp));
	printf("%llu\t", (long long)step_time);
	printf("%s\n", name);

	return step_time;
}

uint64_t timestamp_print_entry(uint32_t id, uint64_t stamp, uint64_t prev_stamp)
{
	const char *name;
	uint64_t step_time;

	name = timestamp_name(id);

	printf("%4d:", id);
	printf("%-50s", name);
	print_norm(arch_convert_raw_ts_entry(stamp));
	step_time = arch_convert_raw_ts_entry(stamp - prev_stamp);
	if (prev_stamp) {
		printf(" (");
		print_norm(step_time);
		printf(")");
	}
	printf("\n");

	return step_time;
}

/* dump the timestamp table */
static void dump_timestamps(int mach_readable)
{
	int i;
	struct timestamp_table *tst_p;
	size_t size;
	uint64_t prev_stamp;
	uint64_t total_time;

	if (timestamps.tag != LB_TAG_TIMESTAMPS) {
		fprintf(stderr, "No timestamps found in coreboot table.\n");
		return;
	}

	size = sizeof(*tst_p);
	tst_p = map_memory_size((unsigned long)timestamps.cbmem_addr, size);

	timestamp_set_tick_freq(tst_p->tick_freq_mhz);

	if (!mach_readable)
		printf("%d entries total:\n\n", tst_p->num_entries);
	size += tst_p->num_entries * sizeof(tst_p->entries[0]);

	unmap_memory();
	tst_p = map_memory_size((unsigned long)timestamps.cbmem_addr, size);

	/* Report the base time within the table. */
	prev_stamp = 0;
	if (mach_readable)
		timestamp_print_parseable_entry(0,  tst_p->base_time,
						prev_stamp);
	else
		timestamp_print_entry(0,  tst_p->base_time, prev_stamp);
	prev_stamp = tst_p->base_time;

	total_time = 0;
	for (i = 0; i < tst_p->num_entries; i++) {
		uint64_t stamp;
		const struct timestamp_entry *tse = &tst_p->entries[i];

		/* Make all timestamps absolute. */
		stamp = tse->entry_stamp + tst_p->base_time;
		if (mach_readable)
			total_time +=
				timestamp_print_parseable_entry(tse->entry_id,
							stamp, prev_stamp);
		else
			total_time += timestamp_print_entry(tse->entry_id,
							stamp, prev_stamp);
		prev_stamp = stamp;
	}

	if (!mach_readable) {
		printf("\nTotal Time: ");
		print_norm(total_time);
		printf("\n");
	}

	unmap_memory();
}

/* dump the cbmem console */
static void dump_console(void)
{
	void *console_p;
	char *console_c;
	uint32_t size;
	uint32_t cursor;

	if (console.tag != LB_TAG_CBMEM_CONSOLE) {
		fprintf(stderr, "No console found in coreboot table.\n");
		return;
	}

	console_p = map_memory_size((unsigned long)console.cbmem_addr,
					2 * sizeof(uint32_t));
	/* The in-memory format of the console area is:
	 *  u32  size
	 *  u32  cursor
	 *  char console[size]
	 * Hence we have to add 8 to get to the actual console string.
	 */
	size = ((uint32_t *)console_p)[0];
	cursor = ((uint32_t *)console_p)[1];
	/* Cursor continues to go on even after no more data fits in
	 * the buffer but the data is dropped in this case.
	 */
	if (size > cursor)
		size = cursor;
	console_c = malloc(size + 1);
	unmap_memory();
	if (!console_c) {
		fprintf(stderr, "Not enough memory for console.\n");
		exit(1);
	}

	console_p = map_memory_size((unsigned long)console.cbmem_addr,
	                            size + sizeof(size) + sizeof(cursor));
	memcpy(console_c, console_p + 8, size);
	console_c[size] = 0;
	console_c[cursor] = 0;

	printf("%s\n", console_c);
	if (size < cursor)
		printf("%d %s lost\n", cursor - size,
			(cursor - size) == 1 ? "byte":"bytes");

	free(console_c);

	unmap_memory();
}

static void hexdump(unsigned long memory, int length)
{
	int i;
	uint8_t *m;
	int all_zero = 0;

	m = map_memory_size((intptr_t)memory, length);

	if (length > MAP_BYTES) {
		printf("Truncating hex dump from %d to %d bytes\n\n",
			length, MAP_BYTES);
		length = MAP_BYTES;
	}

	for (i = 0; i < length; i += 16) {
		int j;

		all_zero++;
		for (j = 0; j < 16; j++) {
			if(m[i+j] != 0) {
				all_zero = 0;
				break;
			}
		}

		if (all_zero < 2) {
			printf("%08lx:", memory + i);
			for (j = 0; j < 16; j++)
				printf(" %02x", m[i+j]);
			printf("  ");
			for (j = 0; j < 16; j++)
				printf("%c", isprint(m[i+j]) ? m[i+j] : '.');
			printf("\n");
		} else if (all_zero == 2) {
			printf("...\n");
		}
	}

	unmap_memory();
}

static void dump_cbmem_hex(void)
{
	if (cbmem.type != LB_MEM_TABLE) {
		fprintf(stderr, "No coreboot CBMEM area found!\n");
		return;
	}

	hexdump(unpack_lb64(cbmem.start), unpack_lb64(cbmem.size));
}

/* The root region is at least DYN_CBMEM_ALIGN_SIZE . */
#define DYN_CBMEM_ALIGN_SIZE (4096)
#define ROOT_MIN_SIZE DYN_CBMEM_ALIGN_SIZE
#define CBMEM_POINTER_MAGIC 0xc0389481
#define CBMEM_ENTRY_MAGIC ~(CBMEM_POINTER_MAGIC)

struct cbmem_root_pointer {
	uint32_t magic;
	/* Relative to upper limit/offset. */
	int32_t root_offset;
} __attribute__((packed));

struct dynamic_cbmem_entry {
	uint32_t magic;
	int32_t start_offset;
	uint32_t size;
	uint32_t id;
} __attribute__((packed));

struct cbmem_root {
	uint32_t max_entries;
	uint32_t num_entries;
	uint32_t flags;
	uint32_t entry_align;
	int32_t max_offset;
	struct dynamic_cbmem_entry entries[0];
} __attribute__((packed));

#define CBMEM_MAGIC 0x434f5245
#define MAX_CBMEM_ENTRIES 16

struct cbmem_entry {
	uint32_t magic;
	uint32_t id;
	uint64_t base;
	uint64_t size;
} __attribute__((packed));

struct cbmem_id_to_name {
	uint32_t id;
	const char *name;
};
static const struct cbmem_id_to_name cbmem_ids[] = { CBMEM_ID_TO_NAME_TABLE };

void cbmem_print_entry(int n, uint32_t id, uint64_t base, uint64_t size)
{
	int i;
	const char *name;

	name = NULL;
	for (i = 0; i < ARRAY_SIZE(cbmem_ids); i++) {
		if (cbmem_ids[i].id == id) {
			name = cbmem_ids[i].name;
			break;
		}
	}

	printf("%2d. ", n);
	if (name == NULL)
		printf("%08x ", id);
	else
		printf("%s", name);
	printf("  %08" PRIx64 " ", base);
	printf("  %08" PRIx64 "\n", size);
}

static void dump_static_cbmem_toc(struct cbmem_entry *entries)
{
	int i;

	printf("CBMEM table of contents:\n");
	printf("    ID           START      LENGTH\n");

	for (i=0; i<MAX_CBMEM_ENTRIES; i++) {
		if (entries[i].magic != CBMEM_MAGIC)
			break;
		cbmem_print_entry(i, entries[i].id,
				entries[i].base, entries[i].size);
	}
}

static void dump_dynamic_cbmem_toc(struct cbmem_root *root)
{
	int i;
	debug("CBMEM: max_entries=%d num_entries=%d flags=0x%x, entry_align=0x%x, max_offset=%d\n\n",
		root->max_entries, root->num_entries, root->flags, root->entry_align, root->max_offset);

	printf("CBMEM table of contents:\n");
	printf("    ID           START      LENGTH\n");

	for (i = 0; i < root->num_entries; i++) {
		if(root->entries[i].magic != CBMEM_ENTRY_MAGIC)
			break;
		cbmem_print_entry(i, root->entries[i].id,
			rootptr + root->entries[i].start_offset, root->entries[i].size);
	}
}

static void dump_cbmem_toc(void)
{
	uint64_t start;
	void *cbmem_area;
	struct cbmem_entry *entries;

	if (cbmem.type != LB_MEM_TABLE) {
		fprintf(stderr, "No coreboot CBMEM area found!\n");
		return;
	}

	start = unpack_lb64(cbmem.start);

	cbmem_area = map_memory_size(start, unpack_lb64(cbmem.size));
	entries = (struct cbmem_entry *)cbmem_area;

	if (entries[0].magic == CBMEM_MAGIC) {
		dump_static_cbmem_toc(entries);
	} else {
		rootptr = unpack_lb64(cbmem.start) + unpack_lb64(cbmem.size);
		rootptr &= ~(DYN_CBMEM_ALIGN_SIZE - 1);
		rootptr -= sizeof(struct cbmem_root_pointer);
		unmap_memory();
		struct cbmem_root_pointer *r =
			map_memory_size(rootptr, sizeof(*r));
		if (r->magic == CBMEM_POINTER_MAGIC) {
			struct cbmem_root *root;
			uint64_t rootaddr = rootptr + r->root_offset;
			unmap_memory();
			root = map_memory_size(rootaddr, ROOT_MIN_SIZE);
			dump_dynamic_cbmem_toc(root);
		} else
			fprintf(stderr, "No valid coreboot CBMEM root pointer found.\n");
	}

	unmap_memory();
}

#define COVERAGE_MAGIC 0x584d4153
struct file {
	uint32_t magic;
	uint32_t next;
	uint32_t filename;
	uint32_t data;
	int offset;
	int len;
};

static int mkpath(char *path, mode_t mode)
{
	assert (path && *path);
	char *p;
	for (p = strchr(path+1, '/'); p; p = strchr(p + 1, '/')) {
		*p = '\0';
		if (mkdir(path, mode) == -1) {
			if (errno != EEXIST) {
				*p = '/';
				return -1;
			}
		}
		*p = '/';
	}
	return 0;
}

static void dump_coverage(void)
{
	int i, found = 0;
	uint64_t start;
	struct cbmem_entry *entries;
	void *coverage;
	unsigned long phys_offset;
#define phys_to_virt(x) ((void *)(unsigned long)(x) + phys_offset)

	if (cbmem.type != LB_MEM_TABLE) {
		fprintf(stderr, "No coreboot table area found!\n");
		return;
	}

	start = unpack_lb64(cbmem.start);

	entries = (struct cbmem_entry *)map_memory(start);

	for (i=0; i<MAX_CBMEM_ENTRIES; i++) {
		if (entries[i].magic != CBMEM_MAGIC)
			break;
		if (entries[i].id == CBMEM_ID_COVERAGE) {
			found = 1;
			break;
		}
	}

	if (!found) {
		unmap_memory();
		fprintf(stderr, "No coverage information found in"
			" CBMEM area.\n");
		return;
	}

	start = entries[i].base;
	unmap_memory();
	/* Map coverage area */
	coverage = map_memory(start);
	phys_offset = (unsigned long)coverage - (unsigned long)start;

	printf("Dumping coverage data...\n");

	struct file *file = (struct file *)coverage;
	while (file && file->magic == COVERAGE_MAGIC) {
		FILE *f;
		char *filename;

		debug(" -> %s\n", (char *)phys_to_virt(file->filename));
		filename = strdup((char *)phys_to_virt(file->filename));
		if (mkpath(filename, 0755) == -1) {
			perror("Directory for coverage data could "
				"not be created");
			exit(1);
		}
		f = fopen(filename, "wb");
		if (!f) {
			printf("Could not open %s: %s\n",
				filename, strerror(errno));
			exit(1);
		}
		if (fwrite((void *)phys_to_virt(file->data),
						file->len, 1, f) != 1) {
			printf("Could not write to %s: %s\n",
				filename, strerror(errno));
			exit(1);
		}
		fclose(f);
		free(filename);

		if (file->next)
			file = (struct file *)phys_to_virt(file->next);
		else
			file = NULL;
	}
	unmap_memory();
}

static void print_version(void)
{
	printf("cbmem v%s -- ", CBMEM_VERSION);
	printf("Copyright (C) 2012 The ChromiumOS Authors.  All rights reserved.\n\n");
	printf(
    "This program is free software: you can redistribute it and/or modify\n"
    "it under the terms of the GNU General Public License as published by\n"
    "the Free Software Foundation, version 2 of the License.\n\n"
    "This program is distributed in the hope that it will be useful,\n"
    "but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
    "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
    "GNU General Public License for more details.\n\n"
    "You should have received a copy of the GNU General Public License\n"
    "along with this program.  If not, see <http://www.gnu.org/licenses/>.\n\n");
}

static void print_usage(const char *name)
{
	printf("usage: %s [-cCltTxVvh?]\n", name);
	printf("\n"
	     "   -c | --console:                   print cbmem console\n"
	     "   -C | --coverage:                  dump coverage information\n"
	     "   -l | --list:                      print cbmem table of contents\n"
	     "   -x | --hexdump:                   print hexdump of cbmem area\n"
	     "   -t | --timestamps:                print timestamp information\n"
	     "   -T | --parseable-timestamps:      print parseable timestamps\n"
	     "   -V | --verbose:                   verbose (debugging) output\n"
	     "   -v | --version:                   print the version\n"
	     "   -h | --help:                      print this help\n"
	     "\n");
	exit(1);
}

#ifdef __arm__
static void dt_update_cells(const char *name, int *addr_cells_ptr,
			    int *size_cells_ptr)
{
	if (*addr_cells_ptr >= 0 && *size_cells_ptr >= 0)
		return;

	int buffer;
	size_t nlen = strlen(name);
	char *prop = alloca(nlen + sizeof("/#address-cells"));
	strcpy(prop, name);

	if (*addr_cells_ptr < 0) {
		strcpy(prop + nlen, "/#address-cells");
		int fd = open(prop, O_RDONLY);
		if (fd < 0 && errno != ENOENT) {
			perror(prop);
		} else if (fd >= 0) {
			if (read(fd, &buffer, sizeof(int)) < 0)
				perror(prop);
			else
				*addr_cells_ptr = ntohl(buffer);
			close(fd);
		}
	}

	if (*size_cells_ptr < 0) {
		strcpy(prop + nlen, "/#size-cells");
		int fd = open(prop, O_RDONLY);
		if (fd < 0 && errno != ENOENT) {
			perror(prop);
		} else if (fd >= 0) {
			if (read(fd, &buffer, sizeof(int)) < 0)
				perror(prop);
			else
				*size_cells_ptr = ntohl(buffer);
			close(fd);
		}
	}
}

static char *dt_find_compat(const char *parent, const char *compat,
			    int *addr_cells_ptr, int *size_cells_ptr)
{
	char *ret = NULL;
	struct dirent *entry;
	DIR *dir;

	if (!(dir = opendir(parent))) {
		perror(parent);
		return NULL;
	}

	/* Loop through all files in the directory (DT node). */
	while ((entry = readdir(dir))) {
		/* We only care about compatible props or subnodes. */
		if (entry->d_name[0] == '.' || !((entry->d_type & DT_DIR) ||
		    !strcmp(entry->d_name, "compatible")))
			continue;

		/* Assemble the file name (on the stack, for speed). */
		size_t plen = strlen(parent);
		char *name = alloca(plen + strlen(entry->d_name) + 2);

		strcpy(name, parent);
		name[plen] = '/';
		strcpy(name + plen + 1, entry->d_name);

		/* If it's a subnode, recurse. */
		if (entry->d_type & DT_DIR) {
			ret = dt_find_compat(name, compat, addr_cells_ptr,
					     size_cells_ptr);

			/* There is only one matching node to find, abort. */
			if (ret) {
				/* Gather cells values on the way up. */
				dt_update_cells(parent, addr_cells_ptr,
						size_cells_ptr);
				break;
			}
			continue;
		}

		/* If it's a compatible string, see if it's the right one. */
		int fd = open(name, O_RDONLY);
		int clen = strlen(compat);
		char *buffer = alloca(clen + 1);

		if (fd < 0) {
			perror(name);
			continue;
		}

		if (read(fd, buffer, clen + 1) < 0) {
			perror(name);
			close(fd);
			continue;
		}
		close(fd);

		if (!strcmp(compat, buffer)) {
			/* Initialize these to "unset" for the way up. */
			*addr_cells_ptr = *size_cells_ptr = -1;

			/* Can't leave string on the stack or we'll lose it! */
			ret = strdup(parent);
			break;
		}
	}

	closedir(dir);
	return ret;
}
#endif /* __arm__ */

int main(int argc, char** argv)
{
	int print_defaults = 1;
	int print_console = 0;
	int print_coverage = 0;
	int print_list = 0;
	int print_hexdump = 0;
	int print_timestamps = 0;
	int machine_readable_timestamps = 0;

	int opt, option_index = 0;
	static struct option long_options[] = {
		{"console", 0, 0, 'c'},
		{"coverage", 0, 0, 'C'},
		{"list", 0, 0, 'l'},
		{"timestamps", 0, 0, 't'},
		{"parseable-timestamps", 0, 0, 'T'},
		{"hexdump", 0, 0, 'x'},
		{"verbose", 0, 0, 'V'},
		{"version", 0, 0, 'v'},
		{"help", 0, 0, 'h'},
		{0, 0, 0, 0}
	};
	while ((opt = getopt_long(argc, argv, "cCltTxVvh?",
				  long_options, &option_index)) != EOF) {
		switch (opt) {
		case 'c':
			print_console = 1;
			print_defaults = 0;
			break;
		case 'C':
			print_coverage = 1;
			print_defaults = 0;
			break;
		case 'l':
			print_list = 1;
			print_defaults = 0;
			break;
		case 'x':
			print_hexdump = 1;
			print_defaults = 0;
			break;
		case 't':
			print_timestamps = 1;
			print_defaults = 0;
			break;
		case 'T':
			print_timestamps = 1;
			machine_readable_timestamps = 1;
			print_defaults = 0;
			break;
		case 'V':
			verbose = 1;
			break;
		case 'v':
			print_version();
			exit(0);
			break;
		case 'h':
		case '?':
		default:
			print_usage(argv[0]);
			exit(0);
			break;
		}
	}

	mem_fd = open("/dev/mem", O_RDONLY, 0);
	if (mem_fd < 0) {
		fprintf(stderr, "Failed to gain memory access: %s\n",
			strerror(errno));
		return 1;
	}

#ifdef __arm__
	int addr_cells, size_cells;
	char *coreboot_node = dt_find_compat("/proc/device-tree", "coreboot",
					     &addr_cells, &size_cells);

	if (!coreboot_node) {
		fprintf(stderr, "Could not find 'coreboot' compatible node!\n");
		return 1;
	}

	if (addr_cells < 0) {
		fprintf(stderr, "Warning: no #address-cells node in tree!\n");
		addr_cells = 1;
	}

	int nlen = strlen(coreboot_node);
	char *reg = alloca(nlen + sizeof("/reg"));

	strcpy(reg, coreboot_node);
	strcpy(reg + nlen, "/reg");
	free(coreboot_node);

	int fd = open(reg, O_RDONLY);
	if (fd < 0) {
		perror(reg);
		return 1;
	}

	int i;
	size_t size_to_read = addr_cells * 4 + size_cells * 4;
	u8 *dtbuffer = alloca(size_to_read);
	if (read(fd, dtbuffer, size_to_read) < 0) {
		perror(reg);
		return 1;
	}
	close(fd);

	/* No variable-length byte swap function anywhere in C... how sad. */
	u64 baseaddr = 0;
	for (i = 0; i < addr_cells * 4; i++) {
		baseaddr <<= 8;
		baseaddr |= *dtbuffer;
		dtbuffer++;
	}
	u64 cb_table_size = 0;
	for (i = 0; i < size_cells * 4; i++) {
		cb_table_size <<= 8;
		cb_table_size |= *dtbuffer;
		dtbuffer++;
	}

	parse_cbtable(baseaddr, cb_table_size);
#else
	int j;
	static const int possible_base_addresses[] = { 0, 0xf0000 };

	/* Find and parse coreboot table */
	for (j = 0; j < ARRAY_SIZE(possible_base_addresses); j++) {
		if (parse_cbtable(possible_base_addresses[j], MAP_BYTES))
			break;
	}
#endif

	if (print_console)
		dump_console();

	if (print_coverage)
		dump_coverage();

	if (print_list)
		dump_cbmem_toc();

	if (print_hexdump)
		dump_cbmem_hex();

	if (print_defaults || print_timestamps)
		dump_timestamps(machine_readable_timestamps);

	close(mem_fd);
	return 0;
}
