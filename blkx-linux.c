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
#include <stdlib.h>
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
#define SAFE_FTRUNCATE ftruncate
#define SAFE_CLOSE close
#define SAFE_LSEEK lseek
#define SAFE_MALLOC(...) aligned_alloc(4096, ##__VA_ARGS__)
#define SAFE_READ pread
#define SAFE_WRITE pwrite

const char* FNAME = "/dev/null";

// That's the upper bound that blkx can write into, that's to avoid operating on the area reserved for BLKDISCARD threads, that will run in parallel to it.
#define UPPER_BOUND_FOR_BLKX_IN_GB 10UL

static int file_desc;
static long long file_max_size = 1024 * 1024 * 1024;
static long long op_max_size = 128 * 1024;
static long long base_offset = 0;
static long long file_size;
static int op_write_align = 1;
static int op_read_align = 1;
static int op_trunc_align = 1;
static int op_nums = 10000;
static int page_size;

static char *file_buff;
static char *temp_buff;

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

static void op_file_position(
	const long long fsize,
	const int align,
	struct file_pos_t *pos)
{
	long long diff;

	pos->offset = random() % (fsize - op_max_size) + base_offset;
	pos->size = op_max_size;

	diff = pos->offset % align;

	if (diff) {
		pos->offset -= diff;
		pos->size += diff;
	}

	if (!pos->size)
		pos->size = 1;

	op_align_pages(pos);
	assert(pos->size <= op_max_size);
	assert(pos->offset + op_max_size <= file_max_size + base_offset);
}

static void update_file_size(struct file_pos_t const *pos)
{
	if (pos->offset + pos->size > file_size) {
		file_size = pos->offset + pos->size;

		tst_res(TINFO, "File size changed: %llu", file_size);
	}
}

static int memory_compare(
	const char *a,
	const char *b,
	const long long offset,
	const long long size)
{
	int diff;

	assert(size <= op_max_size);
	for (long long i = 0; i < size; i++) {
		diff = a[i] - b[i];
		if (diff) {
			tst_res(TINFO,
				"File memory differs at offset=%llu ('%c' != '%c')",
				offset + i, a[i], b[i]);
			break;
		}
	}

	return diff;
}

static int op_read(const struct file_pos_t pos)
{
	if (!file_size) {
		tst_res(TINFO, "Skipping zero size read");
		return 0;
	}


	tst_res(TINFO,
		"Reading at offset=%llu, size=%llu",
		pos.offset,
		pos.size);

	memset(temp_buff, 0, op_max_size);

	SAFE_READ(file_desc, temp_buff, pos.size, pos.offset);

	int ret = memory_compare(
		file_buff,
		temp_buff,
		pos.offset,
		pos.size);

	if (ret)
		return -1;

	return 1;
}

static int op_write(const struct file_pos_t pos)
{
	if (file_size >= file_max_size + base_offset) {
		assert(0);
	}

	char data;

	assert(pos.size <= op_max_size);
	for (long long i = 0; i < pos.size; i++) {
		data = random() % 10 + 'a';

		file_buff[i] = data;
		temp_buff[i] = data;
	}

	tst_res(TINFO,
		"Writing at offset=%llu, size=%llu",
		pos.offset,
		pos.size);

	SAFE_WRITE(file_desc, temp_buff, pos.size, pos.offset);

	update_file_size(&pos);

	return 1;
}

static void run(void)
{
	int op;
	int ret;
	int counter = 0;

	file_size = 0;

	memset(file_buff, 0, op_max_size);
	memset(temp_buff, 0, op_max_size);

	SAFE_FTRUNCATE(file_desc, 0);

	while (counter < op_nums) {
		struct file_pos_t pos;
		op_file_position(file_max_size, op_trunc_align, &pos);

		assert(pos.size <= op_max_size);
		assert(pos.offset >= base_offset);
		assert(pos.offset + pos.size < (UPPER_BOUND_FOR_BLKX_IN_GB*1024UL*1024*1024));

		ret = op_write(pos);
		if (ret == -1) {
			break;
		}
		ret = op_read(pos);
		if (ret == -1) {
			break;
		}
		counter += ret;
	}

	if (counter != op_nums) {
		tst_brk(TFAIL, "Some file operations failed");
		exit(1);
	} else
		tst_res(TPASS, "All file operations succeed");
}

 #define max(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })

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
		page_size = max(st.st_blksize, (int)sysconf(_SC_PAGESIZE));
	}
	printf("Block size: %ld\n", page_size);

	srandom(time(NULL));

	file_desc = SAFE_OPEN(FNAME, O_RDWR | O_DIRECT);
	if (file_desc == -1) {
		printf("Unable to open file %s", strerror(errno));
		exit(1);
	}

	file_buff = SAFE_MALLOC(op_max_size);
	temp_buff = SAFE_MALLOC(op_max_size);
}

static void cleanup(void)
{
	if (file_buff)
		free(file_buff);

	if (temp_buff)
		free(temp_buff);

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
		if (offset_in_gb >= UPPER_BOUND_FOR_BLKX_IN_GB) {
			printf("Unable to operate on area reserved for BLKDISCARD, which is above %dGB\n", UPPER_BOUND_FOR_BLKX_IN_GB);
			exit(1);
		}
		base_offset = offset_in_gb * 1024 * 1024 * 1024ULL;
	}

	setup();
	run();
	cleanup();

	return 0;
}
