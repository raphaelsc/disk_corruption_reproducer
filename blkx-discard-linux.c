// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 1991, NeXT Computer, Inc.  All Rights Reserverd.
 *	Author:	Avadis Tevanian, Jr.
 *
 * Copyright (c) 1998-2001 Apple Computer, Inc. All rights reserved.
 *	Conrad Minshall <conrad@mac.com>
 *	Dave Jones <davej@suse.de>
 *	Zach Brown <zab@clusterfs.com>
 *	Joe Sokol, Pat Dirks, Clark Warner, Guy Harris
 *
 * Copyright (C) 2023 SUSE LLC Andrea Cervesato <andrea.cervesato@suse.com>
 *
 * Copyright (C) 2023 ScyllaDB inc. Raphael S. Carvalho <raphaelsc@scylladb.com>
 */

/*\
 * [Description]
 *
 * This is a complete rewrite of the old fsx-linux tool, created by
 * NeXT Computer, Inc. and Apple Computer, Inc. between 1991 and 2001,
 * then adapted for LTP. Test is actually a file system exerciser: we bring a
 * file and randomly write operations like read/write/map read/map write and
 * truncate, according with input parameters. Then we check if all of them
 * have been completed.
 */

#define _GNU_SOURCE

#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <limits.h>
#include <fcntl.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <linux/fs.h>

#define TINFO 0
#define TBROK 0

#define tst_brk(ttype, arg_fmt, ...) printf(arg_fmt"\n", ##__VA_ARGS__)
#define tst_res(ttype, arg_fmt, ...) printf(arg_fmt"\n", ##__VA_ARGS__)

#define SAFE_OPEN open
#define SAFE_CLOSE close

const char* FNAME = "/dev/null";


// That's the lower bound that blkx-discard can operate on, that's to avoid operating on the area reserved for blkx threads, that will run in parallel to it.
// Leaves 10GB of safe area, as upper bound of BLX threads is 10G
#define LOWER_BOUND_FOR_BLKX_DISCARD_IN_GB 20UL

static int file_desc;
static long long file_max_size = 1024 * 1024 * 1024;
static long long op_max_size = 128 * 1024;
static long long base_offset = 0;
static long long file_size;
static int op_nums = 10000;
static int page_size;

struct file_pos_t {
	long long offset;
	long long size;
};

static void op_align_pages(struct file_pos_t *pos)
{
	long long pg_mask = page_size - 1;

	pos->offset = pos->offset & ~(pg_mask);
	pos->size = (pos->size + page_size - 1) & ~(pg_mask);
}

static int op_truncate(const struct file_pos_t pos)
{
	uint64_t speculative_size = (pos.offset - base_offset) + pos.size;

	if (!file_size) {
		speculative_size = 1024*1024;
	} else if (speculative_size < 2 * file_size) {
		speculative_size = (2 * file_size > file_max_size) ? file_max_size : 2 * file_size;
	}

	file_size = speculative_size;
	uint64_t file_end_offset = file_size + base_offset;

	if (file_end_offset < pos.offset) {
		printf("file_end_offset %ld < pos offset %ld\n", file_end_offset, pos.offset);
		exit(1);
	}
	uint64_t discard_length = file_end_offset - pos.offset;

	tst_res(TINFO, "Discard %ld bytes at offset %llu, file_end_offset %llu", discard_length, pos.offset, file_end_offset);

	assert(pos.offset >= LOWER_BOUND_FOR_BLKX_DISCARD_IN_GB*1024*1024*1024UL);
	assert(discard_length <= file_max_size);

	uint64_t range[2] = { pos.offset, discard_length };
    if (ioctl(file_desc, BLKDISCARD, &range) == -1) {
		printf("BLKDISCARD failed: %s", strerror(errno));
		exit(1);
	}

	return 1;
}

static void run(void)
{
	int ret;
	int counter = 0;

	file_size = 0;

	uint64_t file_offset = base_offset;

	while (counter < op_nums) {
		struct file_pos_t pos;
		pos.offset = file_offset;
		pos.size = op_max_size;
		op_align_pages(&pos);

		ret = op_truncate(pos);
		if (ret == -1)
			break;

		file_offset += pos.size;
		if (file_offset >= file_max_size) {
			file_offset = base_offset;
		}
		counter += ret;
	}

	if (ret == -1) {
		tst_brk(TFAIL, "Some file operations failed");
		exit(1);
	}
	else
		tst_res(TPASS, "All file operations succeed");
}

static void setup(void)
{
	struct stat st;
	int r = fstat(file_desc, &st);
	if (r == -1) {
		exit(1);
	}
	if (!S_ISBLK(st.st_mode)) {
		printf("Not block device\n");
	}

	int ret = ioctl(file_desc, BLKBSZGET, &page_size);
	if (ret == -1) {
		page_size = st.st_blksize;
	}
	printf("Block size: %ld\n", page_size);

	srandom(time(NULL));

	file_desc = SAFE_OPEN(FNAME, O_RDWR | O_DIRECT);
	if (file_desc == -1) {
		printf("Unable to open file %s", strerror(errno));
		exit(1);
	}
}

static void cleanup(void)
{
	if (file_desc)
		SAFE_CLOSE(file_desc);
}

int main(int argc, char** argv) {
	if (argc < 2) {
		printf("usage: %s /path/to/device [offset_in_GB]\n", argv[0]);
		return 0;
	}
	FNAME = argv[1];
	printf("Block device: %s\n", FNAME);

	if (argc == 3) {
		long long offset_in_gb = atoi(argv[2]);
		if (offset_in_gb < LOWER_BOUND_FOR_BLKX_DISCARD_IN_GB) {
			printf("Unable to operate on area reserved for BLKX threads, the lower bound for discard is %dG\n", LOWER_BOUND_FOR_BLKX_DISCARD_IN_GB);
			exit(1);
		}
		base_offset = offset_in_gb * 1024 * 1024 * 1024ULL;
	}

	setup();
	run();
	cleanup();

	return 0;
}
