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
#include "include.h"


static std::atomic<struct otelc_dbg_mem *> dbg_mem{nullptr};


/***
 * NAME
 *   otelc_dbg_set_metadata - sets metadata for a memory allocation
 *
 * SYNOPSIS
 *   static void otelc_dbg_set_metadata(void *ptr, struct otelc_dbg_mem_data *data)
 *
 * ARGUMENTS
 *   ptr  - the real address of the allocated data
 *   data - pointer to the metadata for the allocation
 *
 * DESCRIPTION
 *   Associates metadata with a memory allocation for debugging purposes.  This
 *   allows the memory debugger to track information about each allocation, such
 *   as its size and the location where it was allocated.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void otelc_dbg_set_metadata(void *ptr, struct otelc_dbg_mem_data *data)
{
	struct otelc_dbg_mem_metadata *metadata;

	OTELC_FUNC_EX(MEM, "%p, %p", ptr, data);

	if (OTEL_NULL(ptr))
		OTELC_RETURN();

	metadata        = OTEL_CAST_TYPEOF(metadata, ptr);
	metadata->data  = OTEL_NULL(data) ? OTEL_CAST_TYPEOF(data, metadata) : data;
	metadata->magic = DBG_MEM_MAGIC;

	OTELC_RETURN();
}


/***
 * NAME
 *   otelc_dbg_is_wrapper_block - tells whether a block carries valid metadata
 *
 * SYNOPSIS
 *   static DBG_MEM_NO_ASAN bool otelc_dbg_is_wrapper_block(const struct otelc_dbg_mem_metadata *metadata)
 *
 * ARGUMENTS
 *   metadata - candidate metadata header located below a payload pointer
 *
 * DESCRIPTION
 *   Reads the candidate header and reports whether it was produced by these
 *   wrappers.  This is the probe that reaches below the payload of a block the
 *   caller may have obtained elsewhere, so it is the one place that touches
 *   memory outside any known allocation; it carries DBG_MEM_NO_ASAN for that
 *   reason and must stay free of any other memory access.
 *
 * RETURN VALUE
 *   Returns true if the header carries the magic value and a record pointer,
 *   false otherwise.
 */
static DBG_MEM_NO_ASAN bool otelc_dbg_is_wrapper_block(const struct otelc_dbg_mem_metadata *metadata)
{
	if (OTEL_NULL(metadata) || OTEL_NULL(metadata->data) || (metadata->magic != DBG_MEM_MAGIC))
		return false;

	return true;
}


/***
 * NAME
 *   otelc_dbg_is_table_record - tells whether a record pointer lies inside the tracking table
 *
 * SYNOPSIS
 *   static bool otelc_dbg_is_table_record(const struct otelc_dbg_mem *mem, const struct otelc_dbg_mem_data *data)
 *
 * ARGUMENTS
 *   mem  - a pointer to the memory debugger state structure
 *   data - the record pointer taken from a block header
 *
 * DESCRIPTION
 *   A block header is stored inside the block itself, and the C library reuses
 *   those bytes for its own free lists once the block is released.  The record
 *   pointer read from a header is therefore checked to lie inside the table and
 *   on a record boundary before it is dereferenced.
 *
 * RETURN VALUE
 *   Returns true if data addresses a record of the table, false otherwise.
 */
static bool otelc_dbg_is_table_record(const struct otelc_dbg_mem *mem, const struct otelc_dbg_mem_data *data)
{
	const uintptr_t offset = OTEL_CAST_REINTERPRET(uintptr_t, data) - OTEL_CAST_REINTERPRET(uintptr_t, mem->data);

	if (offset >= (mem->count * sizeof(*data)))
		return false;

	return (offset % sizeof(*data)) == 0;
}


/***
 * NAME
 *   otelc_dbg_mem_add - adds a memory allocation to the tracking list
 *
 * SYNOPSIS
 *   static void otelc_dbg_mem_add(const char *func, int line, void *ptr, size_t size, struct otelc_dbg_mem_data *data, int op_idx)
 *
 * ARGUMENTS
 *   func   - the name of the calling function
 *   line   - the line number of the call
 *   ptr    - the real address of the allocated data
 *   size   - the number of bytes to allocate
 *   data   - pointer to the metadata for the allocation
 *   op_idx - the type of memory operation
 *
 * DESCRIPTION
 *   Adds a new memory allocation to the list of tracked allocations.  This
 *   function records the location, size, and type of the allocation, which is
 *   used by the memory debugger to detect leaks and other memory errors.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void otelc_dbg_mem_add(const char *func, int line, void *ptr, size_t size, struct otelc_dbg_mem_data *data, int op_idx)
{
	struct otelc_dbg_mem *mem = dbg_mem;

	OTELC_FUNC_EX(MEM, "\"%s\", %d, %p, %zu, %p, %d", OTELC_STR_ARG(func), line, ptr, size, data, op_idx);

	(void)snprintf(data->func, sizeof(data->func), "%s:%d", OTELC_STR_ARG(func), line);

	data->ptr  = ptr;
	data->size = size;
	data->used = true;

	mem->size += size;
	mem->op_cnt[op_idx]++;

	otelc_dbg_set_metadata(ptr, data);

	OTELC_RETURN();
}


/***
 * NAME
 *   otelc_dbg_mem_alloc - tracks a memory allocation
 *
 * SYNOPSIS
 *   static void otelc_dbg_mem_alloc(const char *func, int line, void *old_ptr, void *ptr, size_t size)
 *
 * ARGUMENTS
 *   func    - the name of the calling function
 *   line    - the line number of the call
 *   old_ptr - the original address of the data (for realloc)
 *   ptr     - the real address of the allocated data
 *   size    - the number of bytes to allocate
 *
 * DESCRIPTION
 *   Tracks a memory allocation, recording its location and size.  This function
 *   is called by the debugging allocation functions to register new memory
 *   allocations with the memory debugger.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void otelc_dbg_mem_alloc(const char *func, int line, void *old_ptr, void *ptr, size_t size)
{
	struct otelc_dbg_mem *mem = dbg_mem;
	size_t                idx = 0;
	int                   rc;

	OTELC_FUNC_EX(MEM, "\"%s\", %d, %p, %p, %zu", OTELC_STR_ARG(func), line, old_ptr, ptr, size);

	if (OTEL_NULL(mem)) {
		/***
		 * The tracker is not yet initialized (this allocation ran
		 * before otelc_dbg_mem_init()).  The block still carries the
		 * reserved metadata header, and otelc_dbg_free() relies on a
		 * valid magic to locate the allocation base; initialize the
		 * header here so the block stays freeable even while untracked.
		 */
		otelc_dbg_set_metadata(ptr, nullptr);

		OTELC_RETURN();
	}
	else if (OTEL_NULL(ptr)) {
		DBG_MEM_ERR("allocation failed: %s:%d(%p %zu)", OTELC_STR_ARG(func), line, old_ptr, size);

		OTELC_RETURN();
	}

	if ((rc = pthread_mutex_lock(&(mem->mutex))) != 0) {
		DBG_MEM_ERR("unable to lock mutex: %s", otel_strerror(rc));

		OTELC_RETURN();
	}

	if (dbg_mem != mem) {
		/***
		 * The tracker was disabled while this thread was waiting for
		 * the mutex.  Mark the block untracked, exactly as the
		 * uninitialized-tracker path above does, so it stays freeable.
		 */
		otelc_dbg_set_metadata(ptr, nullptr);

		(void)pthread_mutex_unlock(&(mem->mutex));

		OTELC_RETURN();
	}

	OTEL_ARG_DEFAULT(func, "(null)");

	if (!OTEL_NULL(old_ptr)) {
		/* Reallocating memory. */
		struct otelc_dbg_mem_metadata *metadata = OTEL_CAST_TYPEOF(metadata, ptr);

		if (OTEL_NULL(metadata)) {
			DBG_MEM_ERR("no metadata: MEM_REALLOC %s:%d(%p -> %p %zu)", func, line, old_ptr, DBG_MEM_PTR(ptr), size);
		}
		else if (OTEL_NULL(metadata->data)) {
			DBG_MEM_ERR("invalid metadata: MEM_REALLOC %s:%d(%p -> %p %zu)", func, line, old_ptr, DBG_MEM_PTR(ptr), size);
		}
		else if (metadata->data == OTEL_CAST_TYPEOF(metadata->data, metadata)) {
			DBG_MEM_ERR("unset metadata: MEM_REALLOC %s:%d(%p -> %p %zu)", func, line, old_ptr, DBG_MEM_PTR(ptr), size);
		}
		else if (metadata->magic != DBG_MEM_MAGIC) {
			DBG_MEM_ERR("invalid magic: MEM_REALLOC %s:%d(%p -> %p %zu) 0x%016" PRIx64, func, line, old_ptr, DBG_MEM_PTR(ptr), size, metadata->magic);
		}
		else if (!otelc_dbg_is_table_record(mem, metadata->data)) {
			DBG_MEM_ERR("foreign metadata: MEM_REALLOC %s:%d(%p -> %p %zu) %p", func, line, old_ptr, DBG_MEM_PTR(ptr), size, metadata->data);
		}
		else if (metadata->data->used && (metadata->data->ptr == DBG_MEM_DATA(old_ptr))) {
			OTELC_DBG(MEM, "MEM_REALLOC: %s:%d(%p %zu -> %p %zu)", func, line, old_ptr, metadata->data->size, DBG_MEM_PTR(ptr), size);

			mem->size -= metadata->data->size;
			otelc_dbg_mem_add(func, line, ptr, size, metadata->data, OTELC_DBG_MEM_OP_REALLOC);
		}
		else {
			DBG_MEM_ERR("invalid ptr: MEM_REALLOC %s:%d(%p -> %p %zu) %s %hhu", func, line, old_ptr, DBG_MEM_PTR(ptr), size, metadata->data->func, metadata->data->used);
		}
	} else {
		otelc_dbg_set_metadata(ptr, nullptr);

		/***
		 * The first attempt is to find a location that has not been
		 * used at all so far.  If such is not found, an attempt is made
		 * to find the first available location.
		 */
		if (mem->assigned < mem->count) {
			idx = mem->assigned++;
		} else {
			do {
				if (mem->reused >= mem->count)
					mem->reused = 0;

				if (!mem->data[mem->reused].used) {
					idx = mem->reused++;

					break;
				}

				mem->reused++;
			} while (++idx < mem->count);
		}

		if (idx < mem->count) {
			OTELC_DBG(MEM, "MEM_ALLOC: %s:%d(%p %zu %zu)", func, line, DBG_MEM_PTR(ptr), size, idx);

			otelc_dbg_mem_add(func, line, ptr, size, mem->data + idx, OTELC_DBG_MEM_OP_ALLOC);
		}
	}

	if ((rc = pthread_mutex_unlock(&(mem->mutex))) != 0) {
		DBG_MEM_ERR("unable to unlock mutex: %s", otel_strerror(rc));

		OTELC_RETURN();
	}

	if (idx >= mem->count)
		DBG_MEM_ERR("alloc overflow: %s:%d(%p -> %p %zu)", func, line, old_ptr, DBG_MEM_PTR(ptr), size);

	OTELC_RETURN();
}


/***
 * NAME
 *   otelc_dbg_mem_release - releases a tracked memory allocation
 *
 * SYNOPSIS
 *   static int otelc_dbg_mem_release(const char *func, int line, void *ptr, int op_idx)
 *
 * ARGUMENTS
 *   func   - the name of the calling function
 *   line   - the line number of the call
 *   ptr    - the address of the data being released
 *   op_idx - the type of memory operation
 *
 * DESCRIPTION
 *   Marks a tracked memory allocation as released.  This is called by the
 *   debugging version of free to update the status of an allocation in the
 *   memory debugger.  The release is rejected when the record pointer of the
 *   header lies outside the tracking table, or when the record shows that the
 *   block is not currently allocated or does not match the pointer; both cases
 *   indicate a double or stray free.
 *
 * RETURN VALUE
 *   Returns OTELC_RET_OK when the block may be released, or OTELC_RET_ERROR
 *   when the pointer is rejected.
 */
static int otelc_dbg_mem_release(const char *func, int line, void *ptr, int op_idx)
{
	struct otelc_dbg_mem *mem = dbg_mem;
	int                   rc, retval = OTELC_RET_OK;

	OTELC_FUNC_EX(MEM, "\"%s\", %d, %p, %d", OTELC_STR_ARG(func), line, ptr, op_idx);

	if (OTEL_NULL(mem)) {
		OTELC_RETURN_INT(retval);
	}
	else if (OTEL_NULL(ptr)) {
		DBG_MEM_ERR("invalid memory address: %p", ptr);

		OTELC_RETURN_INT(OTELC_RET_ERROR);
	}

	if ((rc = pthread_mutex_lock(&(mem->mutex))) != 0) {
		DBG_MEM_ERR("unable to lock mutex: %s", otel_strerror(rc));

		OTELC_RETURN_INT(retval);
	}

	if (dbg_mem != mem) {
		/* The tracker was disabled while this thread was waiting for the mutex. */
		(void)pthread_mutex_unlock(&(mem->mutex));

		OTELC_RETURN_INT(retval);
	}

	OTEL_ARG_DEFAULT(func, "(null)");

	const auto metadata = DBG_MEM_DATA(ptr);
	if (OTEL_NULL(metadata)) {
		DBG_MEM_ERR("no metadata: MEM_%s %s:%d(%p)", (op_idx == OTELC_DBG_MEM_OP_FREE) ? "FREE" : "RELEASE", func, line, ptr);
	}
	else if (OTEL_NULL(metadata->data)) {
		DBG_MEM_ERR("invalid metadata: MEM_%s %s:%d(%p)", (op_idx == OTELC_DBG_MEM_OP_FREE) ? "FREE" : "RELEASE", func, line, ptr);
	}
	else if (metadata->data == OTEL_CAST_TYPEOF(metadata->data, metadata)) {
		DBG_MEM_ERR("unset metadata: MEM_%s %s:%d(%p)", (op_idx == OTELC_DBG_MEM_OP_FREE) ? "FREE" : "RELEASE", func, line, ptr);
	}
	else if (metadata->magic != DBG_MEM_MAGIC) {
		DBG_MEM_ERR("invalid magic: MEM_%s %s:%d(%p) 0x%016" PRIx64, (op_idx == OTELC_DBG_MEM_OP_FREE) ? "FREE" : "RELEASE", func, line, ptr, metadata->magic);
	}
	else if (!otelc_dbg_is_table_record(mem, metadata->data)) {
		DBG_MEM_ERR("foreign metadata: MEM_%s %s:%d(%p) %p", (op_idx == OTELC_DBG_MEM_OP_FREE) ? "FREE" : "RELEASE", func, line, ptr, metadata->data);

		retval = OTELC_RET_ERROR;
	}
	else if (metadata->data->used && (metadata->data->ptr == metadata)) {
		OTELC_DBG(MEM, "MEM_%s: %s:%d(%p %zu)", (op_idx == OTELC_DBG_MEM_OP_FREE) ? "FREE" : "RELEASE", func, line, ptr, metadata->data->size);

		metadata->data->used = false;

		mem->size -= metadata->data->size;
		mem->op_cnt[op_idx]++;
	}
	else {
		DBG_MEM_ERR("invalid ptr: %s:%d(%p)", func, line, ptr);

		retval = OTELC_RET_ERROR;

		if (!OTEL_NULL(metadata))
			for (size_t i = 0; i < mem->count; i++)
				if (mem->data[i].ptr == metadata)
					DBG_MEM_ERR("possible previous use: %s %hhu", mem->data[i].func, mem->data[i].used);
	}

	if ((rc = pthread_mutex_unlock(&(mem->mutex))) != 0) {
		DBG_MEM_ERR("unable to unlock mutex: %s", otel_strerror(rc));

		OTELC_RETURN_INT(retval);
	}

	OTELC_RETURN_INT(retval);
}


/***
 * NAME
 *   otelc_dbg_mem_check - verifies the record of a tracked block before a reallocation
 *
 * SYNOPSIS
 *   static int otelc_dbg_mem_check(const char *func, int line, void *ptr)
 *
 * ARGUMENTS
 *   func - the name of the calling function
 *   line - the line number of the call
 *   ptr  - the address of the data being reallocated
 *
 * DESCRIPTION
 *   Checks, under the tracker mutex, that the record referenced by the header
 *   of a block lies inside the tracking table and still describes the block as
 *   allocated.  The header alone is not trusted because the C library reuses
 *   its bytes once the block is released, so a stale record pointer found there
 *   may point anywhere.  The check runs before the block is handed over to
 *   realloc(), which would otherwise operate on released memory.
 *
 * RETURN VALUE
 *   Returns OTELC_RET_OK when the block may be reallocated, or OTELC_RET_ERROR
 *   when the pointer is rejected.
 */
static int otelc_dbg_mem_check(const char *func, int line, void *ptr)
{
	struct otelc_dbg_mem          *mem = dbg_mem;
	struct otelc_dbg_mem_metadata *metadata = DBG_MEM_DATA(ptr);
	int                            rc, retval = OTELC_RET_OK;

	OTELC_FUNC_EX(MEM, "\"%s\", %d, %p", OTELC_STR_ARG(func), line, ptr);

	if (OTEL_NULL(mem))
		OTELC_RETURN_INT(retval);

	if ((rc = pthread_mutex_lock(&(mem->mutex))) != 0) {
		DBG_MEM_ERR("unable to lock mutex: %s", otel_strerror(rc));

		OTELC_RETURN_INT(retval);
	}

	if (dbg_mem != mem) {
		/* The tracker was disabled while waiting for the mutex. */
		(void)pthread_mutex_unlock(&(mem->mutex));

		OTELC_RETURN_INT(retval);
	}

	if (!otelc_dbg_is_table_record(mem, metadata->data)) {
		DBG_MEM_ERR("foreign metadata: MEM_REALLOC %s:%d(%p) %p", OTELC_STR_ARG(func), line, ptr, metadata->data);

		retval = OTELC_RET_ERROR;
	}
	else if (!metadata->data->used || (metadata->data->ptr != metadata)) {
		DBG_MEM_ERR("invalid ptr: MEM_REALLOC %s:%d(%p) %s %hhu", OTELC_STR_ARG(func), line, ptr, metadata->data->func, metadata->data->used);

		retval = OTELC_RET_ERROR;
	}

	if ((rc = pthread_mutex_unlock(&(mem->mutex))) != 0)
		DBG_MEM_ERR("unable to unlock mutex: %s", otel_strerror(rc));

	OTELC_RETURN_INT(retval);
}


/***
 * NAME
 *   otelc_dbg_mem_is_stale - tells whether a block without a valid header came from this allocator
 *
 * SYNOPSIS
 *   static bool otelc_dbg_mem_is_stale(const char *func, int line, void *ptr, const char *op)
 *
 * ARGUMENTS
 *   func - the name of the calling function
 *   line - the line number of the call
 *   ptr  - the address of the data being released or reallocated
 *   op   - the name of the memory operation, used in the diagnostic
 *
 * DESCRIPTION
 *   A block whose header carries no valid metadata is either foreign or a block
 *   of this allocator whose header the C library overwrote when the block was
 *   released.  The records assigned so far are searched under the tracker mutex
 *   for the real base of the block to tell the two apart; a match is reported
 *   as a stale block together with its allocation site and its current state.
 *   Only this path pays for the search.  The search finds a released block only
 *   while its record still holds the old base; once a full table has reused the
 *   record, a later double free of that block reaches the C library.  A foreign
 *   block that happens to start where a released block once did is misreported;
 *   such a coincidence is rare.
 *
 * RETURN VALUE
 *   Returns true if the block came from this allocator and must not be passed
 *   to the C library, false if it is foreign.
 */
static bool otelc_dbg_mem_is_stale(const char *func, int line, void *ptr, const char *op)
{
	struct otelc_dbg_mem *mem = dbg_mem;
	const void           *base = DBG_MEM_DATA(ptr);
	bool                  retval = false;
	int                   rc;

	OTELC_FUNC_EX(MEM, "\"%s\", %d, %p, \"%s\"", OTELC_STR_ARG(func), line, ptr, op);

	if (OTEL_NULL(mem))
		OTELC_RETURN_EX(retval, bool, "%hhu");

	if ((rc = pthread_mutex_lock(&(mem->mutex))) != 0) {
		DBG_MEM_ERR("unable to lock mutex: %s", otel_strerror(rc));

		OTELC_RETURN_EX(retval, bool, "%hhu");
	}

	if (dbg_mem != mem) {
		/* The tracker was disabled while waiting for the mutex. */
		(void)pthread_mutex_unlock(&(mem->mutex));

		OTELC_RETURN_EX(retval, bool, "%hhu");
	}

	for (size_t i = 0; (i < mem->assigned) && !retval; i++)
		if (mem->data[i].ptr == base) {
			DBG_MEM_ERR("stale block: MEM_%s %s:%d(%p) %s %s", op, OTELC_STR_ARG(func), line, ptr, mem->data[i].func, mem->data[i].used ? "header lost" : "already released");

			retval = true;
		}

	if ((rc = pthread_mutex_unlock(&(mem->mutex))) != 0)
		DBG_MEM_ERR("unable to unlock mutex: %s", otel_strerror(rc));

	OTELC_RETURN_EX(retval, bool, "%hhu");
}


/***
 * NAME
 *   otelc_dbg_malloc - allocates memory with debugging information
 *
 * SYNOPSIS
 *   void *otelc_dbg_malloc(const char *func, int line, size_t size)
 *
 * ARGUMENTS
 *   func - the name of the calling function
 *   line - the line number of the call
 *   size - the number of bytes to allocate
 *
 * DESCRIPTION
 *   Allocates a block of memory and records debugging information about the
 *   allocation.  This is a wrapper around the standard malloc function that
 *   integrates with the memory debugger.
 *
 * RETURN VALUE
 *   Returns a pointer to the allocated memory, or nullptr on failure.
 */
void *otelc_dbg_malloc(const char *func, int line, size_t size)
{
	OTELC_FUNC_EX(MEM, "\"%s\", %d, %zu", OTELC_STR_ARG(func), line, size);

	if (size > (SIZE_MAX - DBG_MEM_SIZE(0)))
		OTELC_RETURN_EX(nullptr, void *, "%p");

	auto retptr = malloc(DBG_MEM_SIZE(size));

	otelc_dbg_mem_alloc(func, line, nullptr, retptr, size);

	OTELC_RETURN_EX(DBG_MEM_RETURN(retptr), void *, "%p");
}


/***
 * NAME
 *   otelc_dbg_calloc - allocates and zero-initializes memory with debugging information
 *
 * SYNOPSIS
 *   void *otelc_dbg_calloc(const char *func, int line, size_t nelem, size_t elsize)
 *
 * ARGUMENTS
 *   func   - the name of the calling function
 *   line   - the line number of the call
 *   nelem  - the number of elements to allocate
 *   elsize - the size of each element
 *
 * DESCRIPTION
 *   Allocates a block of memory for an array of elements, zero-initializes the
 *   memory, and records debugging information about the allocation.  It behaves
 *   like the standard calloc function but integrates with the memory debugger.
 *
 * RETURN VALUE
 *   Returns a pointer to the allocated memory, or nullptr on failure.
 */
void *otelc_dbg_calloc(const char *func, int line, size_t nelem, size_t elsize)
{
	OTELC_FUNC_EX(MEM, "\"%s\", %d, %zu, %zu", OTELC_STR_ARG(func), line, nelem, elsize);

	if ((elsize > 0) && (nelem > ((SIZE_MAX - DBG_MEM_SIZE(0)) / elsize)))
		OTELC_RETURN_EX(nullptr, void *, "%p");

	auto retptr = malloc(DBG_MEM_SIZE(nelem * elsize));
	if (!OTEL_NULL(retptr))
		(void)memset(retptr, 0, DBG_MEM_SIZE(nelem * elsize));

	otelc_dbg_mem_alloc(func, line, nullptr, retptr, nelem * elsize);

	OTELC_RETURN_EX(DBG_MEM_RETURN(retptr), void *, "%p");
}


/***
 * NAME
 *   otelc_dbg_realloc - reallocates memory with debugging information
 *
 * SYNOPSIS
 *   void *otelc_dbg_realloc(const char *func, int line, void *ptr, size_t size)
 *
 * ARGUMENTS
 *   func - the name of the calling function
 *   line - the line number of the call
 *   ptr  - a pointer to the memory block to be reallocated
 *   size - the new size of the memory block
 *
 * DESCRIPTION
 *   Changes the size of the memory block pointed to by ptr and records
 *   debugging information about the reallocation.  This is a wrapper around
 *   the standard realloc function that integrates with the memory debugger.
 *   A pointer the tracker rejects as a released or unknown block is reported
 *   and a null pointer is returned, with the memory left untouched.
 *
 * RETURN VALUE
 *   Returns a pointer to the reallocated memory, or nullptr on failure.
 */
void *otelc_dbg_realloc(const char *func, int line, void *ptr, size_t size)
{
	void *retptr;

	OTELC_FUNC_EX(MEM, "\"%s\", %d, %p, %zu", OTELC_STR_ARG(func), line, ptr, size);

	if (size > (SIZE_MAX - DBG_MEM_SIZE(0)))
		OTELC_RETURN_EX(nullptr, void *, "%p");

	if (OTEL_NULL(ptr)) {
		retptr = malloc(DBG_MEM_SIZE(size));

		otelc_dbg_mem_alloc(func, line, nullptr, retptr, size);
	} else {
		struct otelc_dbg_mem_metadata *metadata = DBG_MEM_DATA(ptr);

		/***
		 * If memory is not allocated via these debug functions, it must
		 * not be reallocated via them either, unless the block turns
		 * out to be a released block of this allocator whose header
		 * the C library has overwritten; see otelc_dbg_free().
		 */
		if (!otelc_dbg_is_wrapper_block(metadata)) {
			if (otelc_dbg_mem_is_stale(func, line, ptr, "REALLOC"))
				retptr = nullptr;
			else
				retptr = realloc(ptr, size);

			OTELC_RETURN_PTR(retptr);
		}
		else if (metadata->data == OTEL_CAST_TYPEOF(metadata->data, metadata)) {
			/***
			 * An untracked block (allocated before the tracker was
			 * initialized, or while the tracking table was full)
			 * carries self-referencing metadata.  Reallocate its
			 * real base and re-initialize the header: the copied
			 * self-pointer would go stale when the block moves,
			 * and the tracked path below would then inspect freed
			 * memory through it.
			 */
			retptr = realloc(DBG_MEM_DATA(ptr), DBG_MEM_SIZE(size));

			otelc_dbg_set_metadata(retptr, nullptr);
		}
		else if (otelc_dbg_mem_check(func, line, ptr) == OTELC_RET_ERROR) {
			/* Released or corrupted block; leave it untouched. */
			retptr = nullptr;
		}
		else {
			retptr = realloc(DBG_MEM_DATA(ptr), DBG_MEM_SIZE(size));

			otelc_dbg_mem_alloc(func, line, ptr, retptr, size);
		}
	}

	OTELC_RETURN_EX(DBG_MEM_RETURN(retptr), void *, "%p");
}


/***
 * NAME
 *   otelc_dbg_free - frees memory with debugging information
 *
 * SYNOPSIS
 *   void otelc_dbg_free(const char *func, int line, void *ptr)
 *
 * ARGUMENTS
 *   func - the name of the calling function
 *   line - the line number of the call
 *   ptr  - a pointer to the memory block to be freed
 *
 * DESCRIPTION
 *   Frees a block of memory and records debugging information about the
 *   operation.  This is a wrapper around the standard free function that
 *   integrates with the memory debugger.  When the release of a tracked block
 *   is rejected as a double or stray free, the memory is deliberately left
 *   untouched after the diagnostic.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
void otelc_dbg_free(const char *func, int line, void *ptr)
{
	struct otelc_dbg_mem_metadata *metadata;

	OTELC_FUNC_EX(MEM, "\"%s\", %d, %p", OTELC_STR_ARG(func), line, ptr);

	if (OTEL_NULL(ptr))
		OTELC_RETURN();

	/***
	 * If memory was not allocated via these debug functions, it must not be
	 * released through them either; free it transparently, as the realloc
	 * path does, rather than flagging it as an invalid free.
	 *
	 * The magic probe here and in otelc_dbg_realloc() reads the metadata
	 * header sizeof(struct otelc_dbg_mem_metadata) bytes below the payload
	 * pointer.  For a pointer that was not produced by these functions the
	 * read lies outside the allocation: formally undefined behavior that in
	 * practice lands in the allocator's chunk header on glibc, which is why
	 * otelc_dbg_is_wrapper_block() carries DBG_MEM_NO_ASAN.  A foreign block
	 * whose preceding bytes happen to match the magic would be mistaken for
	 * a tracked one; the probability of that is negligible.
	 *
	 * A block of this allocator loses its header on release, because the C
	 * library reuses those bytes for its free lists.  A pointer without the
	 * magic is therefore looked up by its base among the assigned records
	 * before it is handed to free(): a match is a double free, or a live
	 * block whose header was overwritten, and is reported instead.  A block
	 * that the C library served from its own mapping is unmapped by its
	 * first free, so a second free of it faults in the probe itself; that
	 * case cannot be diagnosed here, and neither can the second free of a
	 * block that never had a record because no tracker existed, or it was
	 * disabled or its table was full, when the block was allocated.
	 */
	metadata = DBG_MEM_DATA(ptr);
	if (!otelc_dbg_is_wrapper_block(metadata)) {
		if (!otelc_dbg_mem_is_stale(func, line, ptr, "FREE"))
			free(ptr);

		OTELC_RETURN();
	}

	/***
	 * An untracked block (allocated before the tracker was initialized, or
	 * while the tracking table was full) carries self-referencing metadata.
	 * Free its real base directly: there is no record to release, and
	 * otelc_dbg_mem_release() would only log a spurious error for it.
	 */
	if (metadata->data == OTEL_CAST_TYPEOF(metadata->data, metadata)) {
		free(DBG_MEM_DATA(ptr));

		OTELC_RETURN();
	}

	/***
	 * A rejected release means the block is not currently allocated (a
	 * double or stray free); freeing it again would corrupt the heap, so
	 * the memory is deliberately left untouched after the diagnostic.
	 */
	if (otelc_dbg_mem_release(func, line, ptr, OTELC_DBG_MEM_OP_FREE) == OTELC_RET_ERROR)
		OTELC_RETURN();

	free(DBG_MEM_DATA(ptr));

	OTELC_RETURN();
}


/***
 * NAME
 *   otelc_dbg_strdup - duplicates a string with debugging information
 *
 * SYNOPSIS
 *   char *otelc_dbg_strdup(const char *func, int line, const char *s)
 *
 * ARGUMENTS
 *   func - the name of the calling function
 *   line - the line number of the call
 *   s    - the string to be duplicated
 *
 * DESCRIPTION
 *   Duplicates a string and records debugging information about the allocation.
 *   It behaves like the standard strdup function but integrates with the memory
 *   debugger.  If s is a null pointer, no allocation is performed and a null
 *   pointer is returned.
 *
 * RETURN VALUE
 *   Returns a pointer to the newly allocated string, or nullptr on failure.
 */
char *otelc_dbg_strdup(const char *func, int line, const char *s)
{
	size_t  len;
	char   *retptr;

	OTELC_FUNC_EX(MEM, "\"%s\", %d, \"%s\"", OTELC_STR_ARG(func), line, OTELC_STR_ARG(s));

	if (OTEL_NULL(s))
		OTELC_RETURN_EX(nullptr, char *, "%p");

	len    = strlen(s) + 1;
	retptr = OTEL_CAST_TYPEOF(retptr, malloc(DBG_MEM_SIZE(len)));
	if (!OTEL_NULL(retptr))
		(void)memcpy(DBG_MEM_PTR(retptr), s, len);

	otelc_dbg_mem_alloc(func, line, nullptr, retptr, len);

	OTELC_RETURN_EX(OTEL_CAST_TYPEOF(retptr, DBG_MEM_RETURN(retptr)), decltype(retptr), "%p");
}


/***
 * NAME
 *   otelc_dbg_strndup - duplicates a string with a specified length and debugging information
 *
 * SYNOPSIS
 *   char *otelc_dbg_strndup(const char *func, int line, const char *s, size_t size)
 *
 * ARGUMENTS
 *   func - the name of the calling function
 *   line - the line number of the call
 *   s    - the string to be duplicated
 *   size - the maximum number of characters to copy from the source string
 *
 * DESCRIPTION
 *   Duplicates a string up to a specified length and records debugging
 *   information about the allocation.  It behaves like the standard strndup
 *   function but integrates with the memory debugger.  If s is a null pointer,
 *   no allocation is performed and a null pointer is returned.
 *
 * RETURN VALUE
 *   Returns a pointer to the newly allocated string, or nullptr on failure.
 */
char *otelc_dbg_strndup(const char *func, int line, const char *s, size_t size)
{
	size_t  len;
	char   *retptr;

	OTELC_FUNC_EX(MEM, "\"%s\", %d, \"%.*s\", %zu", OTELC_STR_ARG(func), line, OTEL_CAST_STATIC(int, size), OTELC_STR_ARG(s), size);

	if (OTEL_NULL(s))
		OTELC_RETURN_EX(nullptr, char *, "%p");

	len    = strnlen(s, size);
	retptr = OTEL_CAST_TYPEOF(retptr, malloc(DBG_MEM_SIZE(len + 1)));
	if (!OTEL_NULL(retptr)) {
		(void)memcpy(DBG_MEM_PTR(retptr), s, len);
		DBG_MEM_PTR(retptr)[len] = '\0';
	}

	otelc_dbg_mem_alloc(func, line, nullptr, retptr, len + 1);

	OTELC_RETURN_EX(OTEL_CAST_TYPEOF(retptr, DBG_MEM_RETURN(retptr)), decltype(retptr), "%p");
}


/***
 * NAME
 *   otelc_dbg_mem_init - initializes the memory debugger
 *
 * SYNOPSIS
 *   int otelc_dbg_mem_init(struct otelc_dbg_mem *mem, struct otelc_dbg_mem_data *data, size_t count)
 *
 * ARGUMENTS
 *   mem   - a pointer to the memory debugger state structure
 *   data  - a pointer to an array of metadata structures for tracking allocations
 *   count - the number of elements in the metadata array
 *
 * DESCRIPTION
 *   Initializes the memory debugger with the provided state and metadata
 *   storage.  Allocations made before this call are not tracked, but they stay
 *   valid and are released correctly by the debugging free function.
 *
 * RETURN VALUE
 *   Returns OTELC_RET_OK on success, or OTELC_RET_ERROR on failure.
 */
int otelc_dbg_mem_init(struct otelc_dbg_mem *mem, struct otelc_dbg_mem_data *data, size_t count)
{
	pthread_mutexattr_t attr;
	int                 retval = OTELC_RET_ERROR;

	OTELC_FUNC_EX(MEM, "%p, %p, %zu", mem, data, count);

	if (OTEL_NULL(mem) || OTEL_NULL(data) || (count == 0))
		OTELC_RETURN_INT(retval);

	(void)memset(mem, 0, sizeof(*mem));
	(void)memset(data, 0, sizeof(*data) * count);

	mem->data  = data;
	mem->count = count;

	if (pthread_mutexattr_init(&attr) == 0) {
		if (pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE) == 0)
			if (pthread_mutex_init(&(mem->mutex), &attr) == 0)
				retval = OTELC_RET_OK;

		(void)pthread_mutexattr_destroy(&attr);
	}

	if (retval == OTELC_RET_OK)
		dbg_mem = mem;

	OTELC_RETURN_INT(retval);
}


/***
 * NAME
 *   otelc_dbg_mem_disable - disables the memory debugger
 *
 * SYNOPSIS
 *   void otelc_dbg_mem_disable(void)
 *
 * ARGUMENTS
 *   This function takes no arguments.
 *
 * DESCRIPTION
 *   Disables the memory debugger.  The global tracker pointer is cleared under
 *   the tracker mutex, so a tracker operation running on another thread either
 *   completes before the debugger is disabled or detects the disabling after it
 *   acquires the mutex and backs out.  The mutex itself is deliberately not
 *   destroyed: a thread may still be blocked on it, and destroying a mutex in
 *   that state is undefined behavior.  The mutex remains initialized inside the
 *   caller-provided state structure, which must therefore stay valid.  After
 *   this function is called, the debugging memory functions will no longer
 *   track allocations.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
void otelc_dbg_mem_disable(void)
{
	struct otelc_dbg_mem *mem = dbg_mem;
	int                   rc;

	OTELC_FUNC_EX(MEM, "");

	if (OTEL_NULL(mem))
		OTELC_RETURN();

	if ((rc = pthread_mutex_lock(&(mem->mutex))) != 0) {
		DBG_MEM_ERR("unable to lock mutex: %s", otel_strerror(rc));

		OTELC_RETURN();
	}

	dbg_mem = nullptr;

	if ((rc = pthread_mutex_unlock(&(mem->mutex))) != 0)
		DBG_MEM_ERR("unable to unlock mutex: %s", otel_strerror(rc));

	OTELC_RETURN();
}


/***
 * NAME
 *   otelc_dbg_mem_info - prints memory debugging information
 *
 * SYNOPSIS
 *   void otelc_dbg_mem_info(void)
 *
 * ARGUMENTS
 *   This function takes no arguments.
 *
 * DESCRIPTION
 *   Prints a summary of the memory allocations that are currently being tracked
 *   by the memory debugger.  This is useful for identifying memory leaks and
 *   other issues.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
void otelc_dbg_mem_info(void)
{
#if defined(HAVE_MALLINFO) || defined(HAVE_MALLINFO2)
	struct otelc_mallinfo mi;
#endif
	struct otelc_dbg_mem *mem = dbg_mem;
	size_t                chunks = 0;
	uint64_t              size = 0;
	int                   rc;

	OTELC_FUNC_EX(MEM, "");

	if (OTEL_NULL(mem))
		OTELC_RETURN();

	if ((rc = pthread_mutex_lock(&(mem->mutex))) != 0) {
		DBG_MEM_ERR("unable to lock mutex: %s", otel_strerror(rc));

		OTELC_RETURN();
	}

	if (dbg_mem != mem) {
		/* The tracker was disabled while this thread was waiting for the mutex. */
		(void)pthread_mutex_unlock(&(mem->mutex));

		OTELC_RETURN();
	}

	OTELC_DBG(INFO, "--- Memory info -------------------------------------");
	OTELC_DBG(INFO, "  alloc/realloc: %" PRIu64 "/%" PRIu64 ", free/release: %" PRIu64 "/%" PRIu64, mem->op_cnt[0], mem->op_cnt[1], mem->op_cnt[2], mem->op_cnt[3]);
	OTELC_DBG(INFO, "  assigned: %zu, reused: %zu, count: %zu", mem->assigned, mem->reused, mem->count);
	for (size_t i = 0; i < mem->count; i++)
		if (mem->data[i].used) {
			OTELC_DBG(INFO, "  %zu %s(%p %zu)", chunks, mem->data[i].func, DBG_MEM_PTR(mem->data[i].ptr), mem->data[i].size);

			size += mem->data[i].size;
			chunks++;
		}

	if (chunks > 0)
		OTELC_DBG(INFO, "  allocated %" PRIu64 " byte(s) in %zu chunk(s)", size, chunks);

	if (mem->size != size)
		OTELC_DBG(INFO, "  size does not match: %" PRIu64 " != %" PRIu64, mem->size, size);

	if ((rc = pthread_mutex_unlock(&(mem->mutex))) != 0) {
		DBG_MEM_ERR("unable to unlock mutex: %s", otel_strerror(rc));

		OTELC_RETURN();
	}

#if defined(HAVE_MALLINFO) || defined(HAVE_MALLINFO2)
	mi = otelc_mallinfo();
	OTELC_DBG(DEBUG, "--- Memory space usage ------------------------------");
	OTELC_DBG(DEBUG, "  Total non-mmapped bytes:     %" PRI_MI, mi.arena);
	OTELC_DBG(DEBUG, "  # of free chunks:            %" PRI_MI, mi.ordblks);
	OTELC_DBG(DEBUG, "  # of free fastbin blocks:    %" PRI_MI, mi.smblks);
	OTELC_DBG(DEBUG, "  Bytes in mapped regions:     %" PRI_MI, mi.hblkhd);
	OTELC_DBG(DEBUG, "  # of mapped regions:         %" PRI_MI, mi.hblks);
	OTELC_DBG(DEBUG, "  Max. total allocated space:  %" PRI_MI, mi.usmblks);
	OTELC_DBG(DEBUG, "  Free bytes held in fastbins: %" PRI_MI, mi.fsmblks);
	OTELC_DBG(DEBUG, "  Total allocated space:       %" PRI_MI, mi.uordblks);
	OTELC_DBG(DEBUG, "  Total free space:            %" PRI_MI, mi.fordblks);
	OTELC_DBG(DEBUG, "  Topmost releasable block:    %" PRI_MI, mi.keepcost);
#endif

	OTELC_RETURN();
}


/***
 * NAME
 *   otelc_dbg_memdup - duplicates a block of memory with debugging information
 *
 * SYNOPSIS
 *   void *otelc_dbg_memdup(const char *func, int line, const void *s, size_t size)
 *
 * ARGUMENTS
 *   func - the name of the calling function
 *   line - the line number of the call
 *   s    - a pointer to the memory block to be duplicated
 *   size - the number of bytes to duplicate
 *
 * DESCRIPTION
 *   Duplicates a block of memory and records debugging information about the
 *   allocation.  It allocates memory, copies the specified number of bytes
 *   from the source, and integrates with the memory debugger.  If s is a null
 *   pointer, no allocation is performed and a null pointer is returned.
 *
 * RETURN VALUE
 *   Returns a pointer to the newly allocated memory, or nullptr on failure.
 */
void *otelc_dbg_memdup(const char *func, int line, const void *s, size_t size)
{
	void *retptr;

	OTELC_FUNC_EX(MEM, "\"%s\", %d, %p, %zu", OTELC_STR_ARG(func), line, s, size);

	/* The payload is size + 1, hence DBG_MEM_SIZE(1) in the overflow guard. */
	if (OTEL_NULL(s) || (size > (SIZE_MAX - DBG_MEM_SIZE(1))))
		OTELC_RETURN_EX(nullptr, void *, "%p");

	retptr = malloc(DBG_MEM_SIZE(size + 1));
	if (!OTEL_NULL(retptr)) {
		(void)memcpy(DBG_MEM_PTR(retptr), s, size);
		DBG_MEM_PTR(retptr)[size] = '\0';
	}

	otelc_dbg_mem_alloc(func, line, nullptr, retptr, size + 1);

	OTELC_RETURN_EX(DBG_MEM_RETURN(retptr), void *, "%p");
}

/*
 * Local variables:
 *  c-indent-level: 8
 *  c-basic-offset: 8
 * End:
 *
 * vi: noexpandtab shiftwidth=8 tabstop=8
 */
