/***
 * Copyright 2026 HAProxy Technologies, Miroslav Zagorac <mzagorac@haproxy.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "test-util.h"

#ifndef OTELC_DBG_MEM

int main(void)
{
	OTELC_LOG(stderr, "ERROR: the memory allocator tests need the debug build of the library");

	return EX_UNAVAILABLE;
}

#else

/***
 * The tests drive the debug allocator through the public OTELC_* macros and
 * check the outcome on the tracker structure they installed themselves, so
 * they do not depend on the wording of the diagnostics.  Every test installs
 * a fresh table, so the tests are independent of each other.
 *
 * A double free of a block above the mmap threshold of the C library cannot
 * be covered: the first free unmaps the memory and the header probe of the
 * allocator faults before any diagnostic can run.  Blocks of the C library
 * may be released and reallocated through the wrapper, and that mixing is
 * covered; the other direction is not, because a block of the wrapper is not
 * an address the C library handed out, so free() or realloc() of the C
 * library abort on it in the debug build.
 */
#define MEM_TABLE_MAX     4096
#define MEM_TABLE_SMALL   8
#define MEM_FILL_A        0x5a
#define MEM_FILL_B        0xa5
#define MEM_TCACHE_FILL   9
#ifdef USE_THREADS
#  define MEM_THREADS     8
#  define MEM_ROUNDS      5000
#endif

static struct otelc_dbg_mem_data table[MEM_TABLE_MAX];
static struct otelc_dbg_mem      tracker;
static size_t                    hdr_size;


/***
 * NAME
 *   mem_setup - installs a fresh tracker table
 *
 * SYNOPSIS
 *   static int mem_setup(size_t count)
 *
 * ARGUMENTS
 *   count - number of records the new table holds
 *
 * DESCRIPTION
 *   Disables the tracker that is currently installed and initializes the test
 *   tracker with the first count records of the static table, so every test
 *   starts from zeroed counters and unused records.
 *
 * RETURN VALUE
 *   Returns TEST_PASS if the tracker was installed, TEST_FAIL otherwise.
 */
static int mem_setup(size_t count)
{
	otelc_dbg_mem_disable();

	if (otelc_dbg_mem_init(&tracker, table, count) == OTELC_RET_ERROR)
		return TEST_FAIL;

	return TEST_PASS;
}


/***
 * NAME
 *   mem_used_records - counts the records that describe a live block
 *
 * SYNOPSIS
 *   static size_t mem_used_records(void)
 *
 * ARGUMENTS
 *   This function takes no arguments.
 *
 * DESCRIPTION
 *   Walks the installed table and counts the records whose used flag is set.
 *
 * RETURN VALUE
 *   Returns the number of used records.
 */
static size_t mem_used_records(void)
{
	size_t i, retval = 0;

	for (i = 0; i < tracker.count; i++)
		if (tracker.data[i].used)
			retval++;

	return retval;
}


/***
 * NAME
 *   mem_consistent - checks the tracker against its records
 *
 * SYNOPSIS
 *   static bool mem_consistent(size_t used, uint64_t alloc, uint64_t realloc_cnt, uint64_t free_cnt)
 *
 * ARGUMENTS
 *   used        - expected number of used records
 *   alloc       - expected value of the allocation counter
 *   realloc_cnt - expected value of the reallocation counter
 *   free_cnt    - expected value of the free counter
 *
 * DESCRIPTION
 *   Compares the counters of the tracker with the expected values and checks
 *   that the byte total kept by the tracker equals the sum of the sizes of the
 *   used records.
 *
 * RETURN VALUE
 *   Returns true if every value matches, false otherwise.
 */
static bool mem_consistent(size_t used, uint64_t alloc, uint64_t realloc_cnt, uint64_t free_cnt)
{
	uint64_t size = 0;
	size_t   i;

	for (i = 0; i < tracker.count; i++)
		if (tracker.data[i].used)
			size += tracker.data[i].size;

	if ((mem_used_records() != used) || (tracker.size != size))
		return false;

	return (tracker.op_cnt[OTELC_DBG_MEM_OP_ALLOC] == alloc)
	    && (tracker.op_cnt[OTELC_DBG_MEM_OP_REALLOC] == realloc_cnt)
	    && (tracker.op_cnt[OTELC_DBG_MEM_OP_FREE] == free_cnt);
}


/***
 * NAME
 *   mem_record - finds the record of a block
 *
 * SYNOPSIS
 *   static struct otelc_dbg_mem_data *mem_record(const void *ptr)
 *
 * ARGUMENTS
 *   ptr - payload address of the block
 *
 * DESCRIPTION
 *   Searches the installed table for the record whose real base lies hdr_size
 *   bytes below the payload address.  The record of a released block is found
 *   as well, because a release only clears the used flag.
 *
 * RETURN VALUE
 *   Returns the record, or NULL if the block is not in the table.
 */
static struct otelc_dbg_mem_data *mem_record(const void *ptr)
{
	const uint8_t *base = (const uint8_t *)ptr - hdr_size;
	size_t         i;

	for (i = 0; i < tracker.count; i++)
		if (tracker.data[i].ptr == base)
			return tracker.data + i;

	return NULL;
}


/***
 * NAME
 *   mem_filled - checks that a buffer holds a single byte value
 *
 * SYNOPSIS
 *   static bool mem_filled(const void *ptr, size_t size, int value)
 *
 * ARGUMENTS
 *   ptr   - buffer to check
 *   size  - number of bytes to check
 *   value - expected byte value
 *
 * DESCRIPTION
 *   Compares every byte of the buffer with the expected value.
 *
 * RETURN VALUE
 *   Returns true if all bytes match, false otherwise.
 */
static bool mem_filled(const void *ptr, size_t size, int value)
{
	const uint8_t *data = ptr;
	size_t         i;

	for (i = 0; i < size; i++)
		if (data[i] != (uint8_t)value)
			return false;

	return true;
}


/***
 * NAME
 *   test_header_size - measures the header the allocator keeps below a block
 *
 * SYNOPSIS
 *   static void test_header_size(void)
 *
 * ARGUMENTS
 *   This function takes no arguments.
 *
 * DESCRIPTION
 *   Allocates one block on a fresh table and derives the header size from the
 *   distance between the payload and the real base stored in the first record.
 *   The other tests use the result to look up records by payload address.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_header_size(void)
{
	const uint8_t *base;
	char          *p;
	int            result = TEST_FAIL;

	if (mem_setup(MEM_TABLE_SMALL) == TEST_PASS) {
		p = OTELC_MALLOC(__func__, __LINE__, 1);
		base = tracker.data[0].ptr;
		if (_nNULL(p) && tracker.data[0].used && _nNULL(base) && (base < (const uint8_t *)p)) {
			hdr_size = (size_t)((const uint8_t *)p - base);

			if ((hdr_size > 0) && (mem_record(p) == tracker.data))
				result = TEST_PASS;
		}
		OTELC_SFREE(p);
	}

	test_report("record base and payload address are related by a fixed header", result);
}


/***
 * NAME
 *   test_pre_init_blocks - allocates before a tracker exists
 *
 * SYNOPSIS
 *   static void test_pre_init_blocks(void)
 *
 * ARGUMENTS
 *   This function takes no arguments.
 *
 * DESCRIPTION
 *   Allocates, fills and reallocates blocks while no tracker is installed, then
 *   installs one and releases the blocks through it.  The untracked blocks must
 *   keep their contents and their release must leave the counters untouched.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_pre_init_blocks(void)
{
	char *m, *c, *s, *n, *d;
	int   result = TEST_FAIL;

	otelc_dbg_mem_disable();

	m = OTELC_MALLOC(__func__, __LINE__, 10);
	c = OTELC_CALLOC(__func__, __LINE__, 4, 8);
	s = OTELC_STRDUP(__func__, __LINE__, "pre-init");
	n = OTELC_STRNDUP(__func__, __LINE__, "pre-init", 3);
	d = OTELC_MEMDUP(__func__, __LINE__, "pre-init", 8);

	if (_nNULL(m) && _nNULL(c) && _nNULL(s) && _nNULL(n) && _nNULL(d)) {
		(void)memset(m, MEM_FILL_A, 10);
		m = OTELC_REALLOC(__func__, __LINE__, m, 5000);

		if (_nNULL(m) && mem_filled(m, 10, MEM_FILL_A) && mem_filled(c, 32, 0)
		    && (strcmp(s, "pre-init") == 0) && (strcmp(n, "pre") == 0) && (memcmp(d, "pre-init\0", 9) == 0)) {
			(void)memset(m, MEM_FILL_B, 5000);

			if ((mem_setup(MEM_TABLE_SMALL) == TEST_PASS) && mem_filled(m, 5000, MEM_FILL_B))
				result = TEST_PASS;
		}
	}

	OTELC_SFREE(m);
	OTELC_SFREE(c);
	OTELC_SFREE(s);
	OTELC_SFREE(n);
	OTELC_SFREE(d);

	if ((result == TEST_PASS) && !mem_consistent(0, 0, 0, 0))
		result = TEST_FAIL;

	test_report("blocks allocated before the tracker stay valid and untracked", result);
}


/***
 * NAME
 *   test_alloc_records - checks the records of every allocation kind
 *
 * SYNOPSIS
 *   static void test_alloc_records(void)
 *
 * ARGUMENTS
 *   This function takes no arguments.
 *
 * DESCRIPTION
 *   Allocates one block with each allocation macro, checks the contents and
 *   the size recorded for each block, prints the memory info while the blocks
 *   are live, and checks that releasing them empties the table.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_alloc_records(void)
{
	const struct otelc_dbg_mem_data *r;
	char                            *m, *c, *s, *e, *n, *d, *z;
	int                              result = TEST_FAIL;

	if (mem_setup(MEM_TABLE_SMALL) != TEST_PASS) {
		test_report("each allocation kind is recorded with its size", result);

		return;
	}

	m = OTELC_MALLOC(__func__, __LINE__, 100);
	c = OTELC_CALLOC(__func__, __LINE__, 10, 20);
	s = OTELC_STRDUP(__func__, __LINE__, "a string");
	e = OTELC_STRDUP(__func__, __LINE__, "");
	n = OTELC_STRNDUP(__func__, __LINE__, "truncate me please", 8);
	d = OTELC_MEMDUP(__func__, __LINE__, "memdup!!", 8);
	z = OTELC_MEMDUP(__func__, __LINE__, "x", 0);

	if (_nNULL(m) && _nNULL(c) && _nNULL(s) && _nNULL(e) && _nNULL(n) && _nNULL(d) && _nNULL(z)) {
		result = TEST_PASS;

		if (!mem_filled(c, 200, 0) || (strcmp(s, "a string") != 0) || (strcmp(e, "") != 0)
		    || (strcmp(n, "truncate") != 0) || (memcmp(d, "memdup!!\0", 9) != 0) || (strcmp(z, "") != 0))
			result = TEST_FAIL;

		r = mem_record(m);
		if (_NULL(r) || !r->used || (r->size != 100)
		    || (strncmp(r->func, __func__, strlen(__func__)) != 0) || (r->func[strlen(__func__)] != ':'))
			result = TEST_FAIL;
		r = mem_record(c);
		if (_NULL(r) || !r->used || (r->size != 200))
			result = TEST_FAIL;
		r = mem_record(s);
		if (_NULL(r) || !r->used || (r->size != 9))
			result = TEST_FAIL;
		r = mem_record(e);
		if (_NULL(r) || !r->used || (r->size != 1))
			result = TEST_FAIL;
		r = mem_record(n);
		if (_NULL(r) || !r->used || (r->size != 9))
			result = TEST_FAIL;
		r = mem_record(d);
		if (_NULL(r) || !r->used || (r->size != 9))
			result = TEST_FAIL;
		r = mem_record(z);
		if (_NULL(r) || !r->used || (r->size != 1))
			result = TEST_FAIL;

		if (!mem_consistent(7, 7, 0, 0) || (tracker.assigned != 7))
			result = TEST_FAIL;

		OTELC_MEMINFO();
	}

	OTELC_SFREE(m);
	OTELC_SFREE(c);
	OTELC_SFREE(s);
	OTELC_SFREE(e);
	OTELC_SFREE(n);
	OTELC_SFREE(d);
	OTELC_SFREE(z);

	if ((result == TEST_PASS) && !mem_consistent(0, 7, 0, 7))
		result = TEST_FAIL;

	test_report("each allocation kind is recorded with its size", result);
}


/***
 * NAME
 *   test_null_and_limits - checks the null and out-of-range arguments
 *
 * SYNOPSIS
 *   static void test_null_and_limits(void)
 *
 * ARGUMENTS
 *   This function takes no arguments.
 *
 * DESCRIPTION
 *   Passes null pointers and sizes that cannot be satisfied to the allocation
 *   macros and checks that they return NULL without creating a record, that a
 *   null pointer may be freed, and that a zero size still yields a block.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_null_and_limits(void)
{
	const struct otelc_dbg_mem_data *r;
	char                            *p = NULL, *q;
	int                              result = TEST_FAIL;

	if (mem_setup(MEM_TABLE_SMALL) != TEST_PASS) {
		test_report("null and out-of-range arguments are rejected without a record", result);

		return;
	}

	OTELC_LOG(stdout, "  (an 'allocation failed' diagnostic is expected for the size the C library refuses)");

	if (_NULL(OTELC_STRDUP(__func__, __LINE__, NULL)) && _NULL(OTELC_STRNDUP(__func__, __LINE__, NULL, 3))
	    && _NULL(OTELC_MEMDUP(__func__, __LINE__, NULL, 3)) && _NULL(OTELC_MALLOC(__func__, __LINE__, SIZE_MAX))
	    && _NULL(OTELC_CALLOC(__func__, __LINE__, SIZE_MAX / 2, 4)) && _NULL(OTELC_REALLOC(__func__, __LINE__, NULL, SIZE_MAX))
	    && _NULL(OTELC_MEMDUP(__func__, __LINE__, "x", SIZE_MAX)) && _NULL(OTELC_MALLOC(__func__, __LINE__, SIZE_MAX / 2))) {
		OTELC_FREE(__func__, __LINE__, NULL);
		OTELC_SFREE(p);
		OTELC_SFREE_CLEAR(p);

		if (_NULL(p) && mem_consistent(0, 0, 0, 0))
			result = TEST_PASS;
	}

	q = OTELC_STRNDUP(__func__, __LINE__, "abc", 0);
	p = OTELC_MALLOC(__func__, __LINE__, 0);
	if (_NULL(q) || (strcmp(q, "") != 0) || _NULL(p)) {
		result = TEST_FAIL;
	} else {
		r = mem_record(q);
		if (_NULL(r) || (r->size != 1))
			result = TEST_FAIL;
		r = mem_record(p);
		if (_NULL(r) || (r->size != 0))
			result = TEST_FAIL;
	}

	OTELC_SFREE(q);
	OTELC_SFREE_CLEAR(p);

	if ((result == TEST_PASS) && (_nNULL(p) || !mem_consistent(0, 2, 0, 2)))
		result = TEST_FAIL;

	test_report("null and out-of-range arguments are rejected without a record", result);
}


/***
 * NAME
 *   test_realloc - checks reallocation in every direction
 *
 * SYNOPSIS
 *   static void test_realloc(void)
 *
 * ARGUMENTS
 *   This function takes no arguments.
 *
 * DESCRIPTION
 *   Grows, shrinks and empties a tracked block and checks that the contents
 *   survive, that the single record follows the block and that a reallocation
 *   from a null pointer counts as an allocation.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_realloc(void)
{
	const struct otelc_dbg_mem_data *r;
	char                            *p, *q = NULL;
	int                              result = TEST_FAIL;

	if (mem_setup(MEM_TABLE_SMALL) != TEST_PASS) {
		test_report("reallocation keeps the contents and moves the record", result);

		return;
	}

	p = OTELC_MALLOC(__func__, __LINE__, 100);
	if (_nNULL(p)) {
		(void)memset(p, MEM_FILL_A, 100);

		p = OTELC_REALLOC(__func__, __LINE__, p, 5000);
		r = _NULL(p) ? NULL : mem_record(p);
		if (_nNULL(r) && r->used && (r->size == 5000) && mem_filled(p, 100, MEM_FILL_A) && mem_consistent(1, 1, 1, 0)) {
			(void)memset(p, MEM_FILL_B, 5000);

			p = OTELC_REALLOC(__func__, __LINE__, p, 10);
			r = _NULL(p) ? NULL : mem_record(p);
			if (_nNULL(r) && r->used && (r->size == 10)
			    && mem_filled(p, 10, MEM_FILL_B) && mem_consistent(1, 1, 2, 0)) {
				p = OTELC_REALLOC(__func__, __LINE__, p, 0);
				r = _NULL(p) ? NULL : mem_record(p);
				if (_nNULL(r) && r->used && (r->size == 0) && mem_consistent(1, 1, 3, 0))
					result = TEST_PASS;
			}
		}
	}

	q = OTELC_REALLOC(__func__, __LINE__, q, 50);
	if (_NULL(q) || !mem_consistent(2, 2, 3, 0))
		result = TEST_FAIL;

	OTELC_SFREE(p);
	OTELC_SFREE(q);

	if ((result == TEST_PASS) && !mem_consistent(0, 2, 3, 2))
		result = TEST_FAIL;

	test_report("reallocation keeps the contents and moves the record", result);
}


/***
 * NAME
 *   test_foreign_blocks - passes blocks of the C library through the wrapper
 *
 * SYNOPSIS
 *   static void test_foreign_blocks(void)
 *
 * ARGUMENTS
 *   This function takes no arguments.
 *
 * DESCRIPTION
 *   Frees and reallocates blocks that came from the C library directly and
 *   checks that the wrapper passes them through without touching the tracker.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_foreign_blocks(void)
{
	char *q, *r;
	int   result = TEST_FAIL;

	if (mem_setup(MEM_TABLE_SMALL) != TEST_PASS) {
		test_report("foreign blocks pass through untracked", result);

		return;
	}

	q = malloc(50);
	if (_nNULL(q)) {
		OTELC_FREE(__func__, __LINE__, q);

		q = malloc(10);
		if (_nNULL(q)) {
			(void)memset(q, MEM_FILL_A, 10);

			r = OTELC_REALLOC(__func__, __LINE__, q, 200);
			if (_nNULL(r)) {
				(void)memset(r + 10, MEM_FILL_B, 190);

				if (mem_filled(r, 10, MEM_FILL_A) && mem_consistent(0, 0, 0, 0))
					result = TEST_PASS;

				free(r);
			} else {
				free(q);
			}
		}
	}

	test_report("foreign blocks pass through untracked", result);
}


/***
 * NAME
 *   test_libc_mixing - mixes blocks of the C library with the wrapper calls
 *
 * SYNOPSIS
 *   static void test_libc_mixing(void)
 *
 * ARGUMENTS
 *   This function takes no arguments.
 *
 * DESCRIPTION
 *   Allocates and releases tracked blocks first, so the table holds records of
 *   released blocks, then releases and reallocates blocks that came from
 *   malloc(), calloc() and strdup() through the wrapper, with the tracker
 *   enabled and then disabled.  Every block of the C library must pass through
 *   with its contents intact, must not be mistaken for a released block of the
 *   tracker, and must leave the counters untouched.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_libc_mixing(void)
{
	char   *tracked[MEM_TABLE_SMALL], *m, *c, *s;
	size_t  i;
	int     result = TEST_PASS;

	if (mem_setup(MEM_TABLE_SMALL) != TEST_PASS) {
		test_report("blocks of the C library mix with the wrapper calls untouched", TEST_FAIL);

		return;
	}

	/* Leave released records behind, so the base lookup has work to do. */
	for (i = 0; i < MEM_TABLE_SMALL; i++)
		tracked[i] = OTELC_MALLOC(__func__, __LINE__, 24);
	for (i = 0; i < MEM_TABLE_SMALL; i++)
		OTELC_SFREE(tracked[i]);

	if (!mem_consistent(0, MEM_TABLE_SMALL, 0, MEM_TABLE_SMALL))
		result = TEST_FAIL;

	m = malloc(40);
	c = calloc(4, 16);
	s = strdup("libc string");
	if (_NULL(m) || _NULL(c) || _NULL(s)) {
		result = TEST_FAIL;
	} else {
		(void)memset(m, MEM_FILL_A, 40);

		m = OTELC_REALLOC(__func__, __LINE__, m, 400);
		if (_NULL(m) || !mem_filled(m, 40, MEM_FILL_A) || !mem_filled(c, 64, 0) || (strcmp(s, "libc string") != 0))
			result = TEST_FAIL;

		OTELC_SFREE_CLEAR(m);
		OTELC_FREE(__func__, __LINE__, c);
		c = NULL;
		OTELC_SFREE_CLEAR(s);
		if (_nNULL(s))
			result = TEST_FAIL;
	}
	free(m);
	free(c);
	free(s);

	if (!mem_consistent(0, MEM_TABLE_SMALL, 0, MEM_TABLE_SMALL))
		result = TEST_FAIL;

	otelc_dbg_mem_disable();

	m = malloc(40);
	s = strdup("libc string");
	if (_NULL(m) || _NULL(s)) {
		result = TEST_FAIL;
	} else {
		(void)memset(m, MEM_FILL_B, 40);

		m = OTELC_REALLOC(__func__, __LINE__, m, 80);
		if (_NULL(m) || !mem_filled(m, 40, MEM_FILL_B))
			result = TEST_FAIL;

		OTELC_SFREE_CLEAR(m);
		OTELC_SFREE_CLEAR(s);
	}
	free(m);
	free(s);

	if (!mem_consistent(0, MEM_TABLE_SMALL, 0, MEM_TABLE_SMALL))
		result = TEST_FAIL;

	test_report("blocks of the C library mix with the wrapper calls untouched", result);
}


/***
 * NAME
 *   test_table_overflow - fills the table and reuses released records
 *
 * SYNOPSIS
 *   static void test_table_overflow(void)
 *
 * ARGUMENTS
 *   This function takes no arguments.
 *
 * DESCRIPTION
 *   Allocates more blocks than the table holds, checks that the surplus blocks
 *   stay usable while untracked, releases everything and checks that the next
 *   allocations reuse the released records.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_table_overflow(void)
{
	char   *blocks[MEM_TABLE_SMALL + 4];
	size_t  i;
	int     result = TEST_PASS;

	if (mem_setup(MEM_TABLE_SMALL) != TEST_PASS) {
		test_report("a full table leaves the surplus blocks usable and reuses records", TEST_FAIL);

		return;
	}

	OTELC_LOG(stdout, "  (an 'alloc overflow' diagnostic is expected for each surplus block)");

	for (i = 0; i < OTELC_TABLESIZE(blocks); i++) {
		blocks[i] = OTELC_MALLOC(__func__, __LINE__, 32);
		if (_NULL(blocks[i]))
			result = TEST_FAIL;
	}

	if (!mem_consistent(MEM_TABLE_SMALL, MEM_TABLE_SMALL, 0, 0) || (tracker.assigned != MEM_TABLE_SMALL))
		result = TEST_FAIL;

	if (_nNULL(blocks[MEM_TABLE_SMALL])) {
		(void)memset(blocks[MEM_TABLE_SMALL], MEM_FILL_A, 32);

		blocks[MEM_TABLE_SMALL] = OTELC_REALLOC(__func__, __LINE__, blocks[MEM_TABLE_SMALL], 4000);
		if (_NULL(blocks[MEM_TABLE_SMALL]) || !mem_filled(blocks[MEM_TABLE_SMALL], 32, MEM_FILL_A))
			result = TEST_FAIL;
		else
			(void)memset(blocks[MEM_TABLE_SMALL], MEM_FILL_B, 4000);
	}

	for (i = 0; i < OTELC_TABLESIZE(blocks); i++)
		OTELC_SFREE(blocks[i]);

	if (!mem_consistent(0, MEM_TABLE_SMALL, 0, MEM_TABLE_SMALL))
		result = TEST_FAIL;

	for (i = 0; i < MEM_TABLE_SMALL; i++) {
		blocks[i] = OTELC_MALLOC(__func__, __LINE__, 8);
		if (_NULL(blocks[i]))
			result = TEST_FAIL;
	}

	if (!mem_consistent(MEM_TABLE_SMALL, 2 * MEM_TABLE_SMALL, 0, MEM_TABLE_SMALL)
	    || (tracker.assigned != MEM_TABLE_SMALL) || (tracker.reused != MEM_TABLE_SMALL))
		result = TEST_FAIL;

	for (i = 0; i < MEM_TABLE_SMALL; i++)
		OTELC_SFREE(blocks[i]);

	if (!mem_consistent(0, 2 * MEM_TABLE_SMALL, 0, 2 * MEM_TABLE_SMALL))
		result = TEST_FAIL;

	test_report("a full table leaves the surplus blocks usable and reuses records", result);
}


/***
 * NAME
 *   test_double_free - frees a block twice
 *
 * SYNOPSIS
 *   static void test_double_free(const char *name, size_t size, size_t count)
 *
 * ARGUMENTS
 *   name  - name of the test case
 *   size  - size of the blocks
 *   count - number of blocks to allocate and release before the double free
 *
 * DESCRIPTION
 *   Allocates and releases count blocks of the given size and then releases
 *   the last one again.  The size and count select the free list of the C
 *   library the block lands in, which decides how much of the header survives
 *   the first release.  The second release must be rejected: the process
 *   survives, the free counter does not move and the record stays released.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_double_free(const char *name, size_t size, size_t count)
{
	const struct otelc_dbg_mem_data *r;
	char                            *blocks[MEM_TCACHE_FILL];
	size_t                           i;
	int                              result = TEST_FAIL;

	if ((count > OTELC_TABLESIZE(blocks)) || (mem_setup(MEM_TABLE_SMALL + MEM_TCACHE_FILL) != TEST_PASS)) {
		test_report(name, result);

		return;
	}

	for (i = 0; i < count; i++)
		blocks[i] = OTELC_MALLOC(__func__, __LINE__, size);

	for (i = 0; i < count; i++)
		OTELC_FREE(__func__, __LINE__, blocks[i]);

	r = _NULL(blocks[count - 1]) ? NULL : mem_record(blocks[count - 1]);
	if (_nNULL(r) && !r->used && mem_consistent(0, count, 0, count)) {
		OTELC_LOG(stdout, "  (a diagnostic of the rejected release is expected below)");
		OTELC_FREE(__func__, __LINE__, blocks[count - 1]);

		if (!r->used && mem_consistent(0, count, 0, count))
			result = TEST_PASS;
	}

	test_report(name, result);
}


/***
 * NAME
 *   test_realloc_released - reallocates a released block
 *
 * SYNOPSIS
 *   static void test_realloc_released(void)
 *
 * ARGUMENTS
 *   This function takes no arguments.
 *
 * DESCRIPTION
 *   Releases a small block and reallocates it afterwards.  The reallocation
 *   must be rejected with a null result while the counters and the record stay
 *   as the release left them.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_realloc_released(void)
{
	const struct otelc_dbg_mem_data *r;
	char                            *p, *q;
	int                              result = TEST_FAIL;

	if (mem_setup(MEM_TABLE_SMALL) != TEST_PASS) {
		test_report("reallocation of a released block is rejected", result);

		return;
	}

	p = OTELC_MALLOC(__func__, __LINE__, 40);
	if (_nNULL(p)) {
		OTELC_FREE(__func__, __LINE__, p);

		r = mem_record(p);
		if (_nNULL(r) && !r->used) {
			OTELC_LOG(stdout, "  (a diagnostic of the rejected reallocation is expected below)");
			q = OTELC_REALLOC(__func__, __LINE__, p, 80);

			if (_NULL(q) && !r->used && mem_consistent(0, 1, 0, 1))
				result = TEST_PASS;
		}
	}

	test_report("reallocation of a released block is rejected", result);
}


/***
 * NAME
 *   test_header_lost - frees a live block whose header was overwritten
 *
 * SYNOPSIS
 *   static void test_header_lost(void)
 *
 * ARGUMENTS
 *   This function takes no arguments.
 *
 * DESCRIPTION
 *   Clears the header below a live block, as a heap underflow in the caller
 *   would, and releases the block.  The release must be rejected and leave the
 *   record in use; the test then returns the real base to the C library itself.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_header_lost(void)
{
	struct otelc_dbg_mem_data *r;
	char                      *p;
	int                        result = TEST_FAIL;

	if (mem_setup(MEM_TABLE_SMALL) != TEST_PASS) {
		test_report("a live block with a lost header is reported and kept", result);

		return;
	}

	p = OTELC_MALLOC(__func__, __LINE__, 64);
	r = _NULL(p) ? NULL : mem_record(p);
	if (_nNULL(r) && r->used) {
		(void)memset(r->ptr, 0, hdr_size);

		OTELC_LOG(stdout, "  (a diagnostic of the rejected release is expected below)");
		OTELC_FREE(__func__, __LINE__, p);

		if (r->used && mem_consistent(1, 1, 0, 0))
			result = TEST_PASS;

		free(r->ptr);
		r->used = false;
	}

	test_report("a live block with a lost header is reported and kept", result);
}


/***
 * NAME
 *   test_tracking_macro - adopts blocks of the C library into the tracker
 *
 * SYNOPSIS
 *   static void test_tracking_macro(void)
 *
 * ARGUMENTS
 *   This function takes no arguments.
 *
 * DESCRIPTION
 *   Re-duplicates a string of the C library through OTELC_DBG_MEM_TRACKING()
 *   and formats strings with otelc_sprintf(), which adopts its result the same
 *   way.  Every result must be tracked with the right size, an empty result
 *   included, and a replaced result must be released.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_tracking_macro(void)
{
	const struct otelc_dbg_mem_data *r;
	char                            *s, *out = NULL;
	int                              result = TEST_FAIL;

	if (mem_setup(MEM_TABLE_SMALL) != TEST_PASS) {
		test_report("adopted blocks are tracked with their size", result);

		return;
	}

	s = strdup("hello");
	if (_nNULL(s)) {
		OTELC_DBG_MEM_TRACKING(s, strlen(s));

		r = mem_record(s);
		if (_nNULL(r) && r->used && (r->size == 6) && (strcmp(s, "hello") == 0) && mem_consistent(1, 1, 0, 0)) {
			OTELC_SFREE_CLEAR(s);

			if (_NULL(s) && !r->used && mem_consistent(0, 1, 0, 1))
				result = TEST_PASS;
		}
	}

	if ((otelc_sprintf(&out, "%d-%s", 42, "x") != 4) || _NULL(out) || (strcmp(out, "42-x") != 0))
		result = TEST_FAIL;
	r = _NULL(out) ? NULL : mem_record(out);
	if (_NULL(r) || !r->used || (r->size != 5) || !mem_consistent(1, 2, 0, 1))
		result = TEST_FAIL;

	if ((otelc_sprintf(&out, "%s", "") != 0) || _NULL(out) || (strcmp(out, "") != 0))
		result = TEST_FAIL;
	r = _NULL(out) ? NULL : mem_record(out);
	if (_NULL(r) || !r->used || (r->size != 1) || !mem_consistent(1, 3, 0, 2))
		result = TEST_FAIL;

	OTELC_SFREE(out);

	if (!mem_consistent(0, 3, 0, 3))
		result = TEST_FAIL;

	test_report("adopted blocks are tracked with their size", result);
}


/***
 * NAME
 *   test_disable - releases tracked blocks after the tracker was disabled
 *
 * SYNOPSIS
 *   static void test_disable(void)
 *
 * ARGUMENTS
 *   This function takes no arguments.
 *
 * DESCRIPTION
 *   Disables the tracker while a tracked block is live, allocates and releases
 *   blocks in the disabled state and checks that the disabled tracker does not
 *   change any more and that a new tracker can be installed afterwards.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_disable(void)
{
	char *a, *b;
	int   result = TEST_FAIL;

	if (mem_setup(MEM_TABLE_SMALL) != TEST_PASS) {
		test_report("a disabled tracker is left alone and can be replaced", result);

		return;
	}

	a = OTELC_MALLOC(__func__, __LINE__, 10);
	if (_nNULL(a) && mem_consistent(1, 1, 0, 0)) {
		otelc_dbg_mem_disable();

		b = OTELC_MALLOC(__func__, __LINE__, 10);
		if (_nNULL(b)) {
			(void)memset(b, MEM_FILL_A, 10);
			b = OTELC_REALLOC(__func__, __LINE__, b, 300);
		}

		OTELC_SFREE(a);
		OTELC_SFREE(b);
		OTELC_MEMINFO();

		if (mem_consistent(1, 1, 0, 0) && (mem_setup(MEM_TABLE_SMALL) == TEST_PASS) && mem_consistent(0, 0, 0, 0))
			result = TEST_PASS;
	}

	test_report("a disabled tracker is left alone and can be replaced", result);
}


#ifdef USE_THREADS

/***
 * NAME
 *   mem_worker - allocation workload of one thread
 *
 * SYNOPSIS
 *   static void *mem_worker(void *arg)
 *
 * ARGUMENTS
 *   arg - thread index, used to vary the block sizes
 *
 * DESCRIPTION
 *   Repeats a malloc, realloc, strndup and free sequence with varying block
 *   sizes and checks the contents after every step.
 *
 * RETURN VALUE
 *   Returns NULL on success, or the thread argument if a check failed.
 */
static void *mem_worker(void *arg)
{
	size_t  seed = (size_t)arg, n;
	char   *p, *s;
	int     i;

	for (i = 0; i < MEM_ROUNDS; i++) {
		n = (((size_t)i + 1) * 7919 + seed) % 3000 + 1;

		p = OTELC_MALLOC(__func__, __LINE__, n);
		if (_NULL(p))
			return arg;
		(void)memset(p, MEM_FILL_A, n);

		p = OTELC_REALLOC(__func__, __LINE__, p, n * 2);
		if (_NULL(p) || !mem_filled(p, n, MEM_FILL_A))
			return arg;

		s = OTELC_STRNDUP(__func__, __LINE__, "hello world", 5);
		if (_NULL(s) || (strcmp(s, "hello") != 0))
			return arg;

		OTELC_SFREE(s);
		OTELC_SFREE(p);
	}

	return NULL;
}


/***
 * NAME
 *   test_threads - runs the allocation workload on several threads
 *
 * SYNOPSIS
 *   static void test_threads(void)
 *
 * ARGUMENTS
 *   This function takes no arguments.
 *
 * DESCRIPTION
 *   Runs mem_worker() on several threads at once and checks that every thread
 *   passed its own checks and that the tracker ends empty with counters that
 *   match the workload.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_threads(void)
{
	pthread_t  tid[MEM_THREADS];
	void      *rc;
	size_t     i;
	int        result = TEST_PASS;

	if (mem_setup(MEM_TABLE_MAX) != TEST_PASS) {
		test_report("concurrent workload leaves an empty and consistent tracker", TEST_FAIL);

		return;
	}

	for (i = 0; i < MEM_THREADS; i++)
		if (pthread_create(&(tid[i]), NULL, mem_worker, (void *)(i + 1)) != 0) {
			tid[i] = pthread_self();
			result = TEST_FAIL;
		}

	for (i = 0; i < MEM_THREADS; i++)
		if (!pthread_equal(tid[i], pthread_self())) {
			if ((pthread_join(tid[i], &rc) != 0) || _nNULL(rc))
				result = TEST_FAIL;
		}

	if (!mem_consistent(0, 2 * MEM_THREADS * MEM_ROUNDS, MEM_THREADS * MEM_ROUNDS, 2 * MEM_THREADS * MEM_ROUNDS))
		result = TEST_FAIL;

	test_report("concurrent workload leaves an empty and consistent tracker", result);
}


/***
 * NAME
 *   test_disable_under_load - disables the tracker while threads allocate
 *
 * SYNOPSIS
 *   static void test_disable_under_load(void)
 *
 * ARGUMENTS
 *   This function takes no arguments.
 *
 * DESCRIPTION
 *   Starts the allocation workload on several threads, disables the tracker
 *   while they run and waits for them.  Every thread must finish its checks;
 *   the tracker counters are indeterminate and are not checked.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void test_disable_under_load(void)
{
	pthread_t  tid[MEM_THREADS];
	void      *rc;
	size_t     i;
	int        result = TEST_PASS;

	if (mem_setup(MEM_TABLE_MAX) != TEST_PASS) {
		test_report("disabling the tracker under load is safe", TEST_FAIL);

		return;
	}

	for (i = 0; i < MEM_THREADS; i++)
		if (pthread_create(&(tid[i]), NULL, mem_worker, (void *)(i + 1)) != 0) {
			tid[i] = pthread_self();
			result = TEST_FAIL;
		}

	otelc_nsleep(0, 20000000);
	otelc_dbg_mem_disable();

	for (i = 0; i < MEM_THREADS; i++)
		if (!pthread_equal(tid[i], pthread_self())) {
			if ((pthread_join(tid[i], &rc) != 0) || _nNULL(rc))
				result = TEST_FAIL;
		}

	test_report("disabling the tracker under load is safe", result);
}

#endif /* USE_THREADS */


/***
 * NAME
 *   main - program entry point
 *
 * SYNOPSIS
 *   int main(int argc, char **argv)
 *
 * ARGUMENTS
 *   argc - number of command-line arguments
 *   argv - array of command-line argument strings
 *
 * DESCRIPTION
 *   Initializes the test environment, limits the debug output to the error
 *   and info levels so the diagnostics of the allocator stay visible, runs the
 *   memory allocator tests and prints the summary.
 *
 * RETURN VALUE
 *   Returns EX_OK if all tests passed, EX_SOFTWARE otherwise.
 */
int main(int argc, char **argv)
{
	const char *cfg_file;
	int         retval;

	retval = test_init(argc, argv, "memory allocator tests", &cfg_file);
	if (retval >= 0)
		return retval;

	otelc_dbg_level = (1 << OTELC_DBG_LEVEL_ERROR) | (1 << OTELC_DBG_LEVEL_INFO);

	OTELC_LOG(stdout, "");
	OTELC_LOG(stdout, "[records]");
	test_header_size();
	test_pre_init_blocks();
	test_alloc_records();
	test_null_and_limits();
	test_realloc();
	test_foreign_blocks();
	test_libc_mixing();
	test_table_overflow();
	test_tracking_macro();

	OTELC_LOG(stdout, "[misuse]");
	test_double_free("double free of a small block is rejected", 40, 1);
	test_double_free("double free of a small block behind a full fast list is rejected", 40, MEM_TCACHE_FILL);
	test_double_free("double free of a medium block is rejected", 40000, 1);
	test_realloc_released();
	test_header_lost();

	OTELC_LOG(stdout, "[lifecycle]");
	test_disable();
#ifdef USE_THREADS
	test_threads();
	test_disable_under_load();
#endif

	return test_summary(EX_OK);
}

#endif /* OTELC_DBG_MEM */

/*
 * Local variables:
 *  c-indent-level: 8
 *  c-basic-offset: 8
 * End:
 *
 * vi: noexpandtab shiftwidth=8 tabstop=8
 */
