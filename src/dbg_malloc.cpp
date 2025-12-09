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


static struct otelc_dbg_mem *dbg_mem = nullptr;


/***
 * NAME
 *   otelc_dbg_set_metadata - sets metadata for a memory allocation
 *
 * SYNOPSIS
 *   static void otelc_dbg_set_metadata(void *ptr, struct otelc_dbg_mem_data *data)
 *
 * ARGUMENTS
 *   ptr  - the real address of the allocated data
 *   data - pointer to the metadata to be associated with the allocation
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
	OTELC_FUNC_EX(MEM, "\"%s\", %d, %p, %zu, %p, %d", OTELC_STR_ARG(func), line, ptr, size, data, op_idx);

	(void)snprintf(data->func, sizeof(data->func), "%s:%d", OTELC_STR_ARG(func), line);

	data->ptr  = ptr;
	data->size = size;
	data->used = true;

	dbg_mem->size += size;
	dbg_mem->op_cnt[op_idx]++;

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
 *   is called by the debugging versions of malloc, calloc, and realloc to
 *   register new memory allocations with the memory debugger.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void otelc_dbg_mem_alloc(const char *func, int line, void *old_ptr, void *ptr, size_t size)
{
	size_t idx = 0;
	int    rc;

	OTELC_FUNC_EX(MEM, "\"%s\", %d, %p, %p, %zu", OTELC_STR_ARG(func), line, old_ptr, ptr, size);

	if (OTEL_NULL(dbg_mem)) {
		OTELC_RETURN();
	}
	else if (OTEL_NULL(ptr)) {
		DBG_MEM_ERR("invalid memory address: %p", ptr);

		OTELC_RETURN();
	}

	if ((rc = pthread_mutex_lock(&(dbg_mem->mutex))) != 0) {
		DBG_MEM_ERR("unable to lock mutex: %s", otel_strerror(rc));

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
			DBG_MEM_ERR("invalid magic: MEM_REALLOC %s:%d(%p -> %p %zu) 0x%016" PRIu64, func, line, old_ptr, DBG_MEM_PTR(ptr), size, metadata->magic);
		}
		else if (metadata->data->used && (metadata->data->ptr == DBG_MEM_DATA(old_ptr))) {
			OTELC_DBG(MEM, "MEM_REALLOC: %s:%d(%p %zu -> %p %zu)", func, line, old_ptr, metadata->data->size, DBG_MEM_PTR(ptr), size);

			dbg_mem->size -= metadata->data->size;
			otelc_dbg_mem_add(func, line, ptr, size, metadata->data, OTELC_DBG_MEM_OP_REALLOC);
		}
	} else {
		otelc_dbg_set_metadata(ptr, nullptr);

		/***
		 * The first attempt is to find a location that has not been
		 * used at all so far.  If such is not found, an attempt is made
		 * to find the first available location.
		 */
		if (dbg_mem->unused < dbg_mem->count) {
			idx = dbg_mem->unused++;
		} else {
			do {
				if (dbg_mem->reused >= dbg_mem->count)
					dbg_mem->reused = 0;

				if (!dbg_mem->data[dbg_mem->reused].used) {
					idx = dbg_mem->reused++;

					break;
				}

				dbg_mem->reused++;
			} while (++idx < dbg_mem->count);
		}

		if (idx < dbg_mem->count) {
			OTELC_DBG(MEM, "MEM_ALLOC: %s:%d(%p %zu %zu)", func, line, DBG_MEM_PTR(ptr), size, idx);

			otelc_dbg_mem_add(func, line, ptr, size, dbg_mem->data + idx, OTELC_DBG_MEM_OP_ALLOC);
		}
	}

	if ((rc = pthread_mutex_unlock(&(dbg_mem->mutex))) != 0) {
		DBG_MEM_ERR("unable to unlock mutex: %s", otel_strerror(rc));

		OTELC_RETURN();
	}

	if (idx >= dbg_mem->count)
		DBG_MEM_ERR("alloc overflow: %s:%d(%p -> %p %zu)", func, line, old_ptr, DBG_MEM_PTR(ptr), size);

	OTELC_RETURN();
}


/***
 * NAME
 *   otelc_dbg_mem_release - releases a tracked memory allocation
 *
 * SYNOPSIS
 *   static void otelc_dbg_mem_release(const char *func, int line, void *ptr, int op_idx)
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
 *   memory debugger.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void otelc_dbg_mem_release(const char *func, int line, void *ptr, int op_idx)
{
	int rc;

	OTELC_FUNC_EX(MEM, "\"%s\", %d, %p, %d", OTELC_STR_ARG(func), line, ptr, op_idx);

	if (OTEL_NULL(dbg_mem)) {
		OTELC_RETURN();
	}
	else if (OTEL_NULL(ptr)) {
		DBG_MEM_ERR("invalid memory address: %p", ptr);

		OTELC_RETURN();
	}

	if ((rc = pthread_mutex_lock(&(dbg_mem->mutex))) != 0) {
		DBG_MEM_ERR("unable to lock mutex: %s", otel_strerror(rc));

		OTELC_RETURN();
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
		DBG_MEM_ERR("invalid magic: MEM_%s %s:%d(%p) 0x%016" PRIu64, (op_idx == OTELC_DBG_MEM_OP_FREE) ? "FREE" : "RELEASE", func, line, ptr, metadata->magic);
	}
	else if (metadata->data->used && (metadata->data->ptr == metadata)) {
		OTELC_DBG(MEM, "MEM_%s: %s:%d(%p %zu)", (op_idx == OTELC_DBG_MEM_OP_FREE) ? "FREE" : "RELEASE", func, line, ptr, metadata->data->size);

		metadata->data->used = false;

		dbg_mem->size -= metadata->data->size;
		dbg_mem->op_cnt[op_idx]++;
	}
	else {
		DBG_MEM_ERR("invalid ptr: %s:%d(%p)", func, line, ptr);

		if (!OTEL_NULL(metadata))
			for (size_t i = 0; i < dbg_mem->count; i++)
				if (dbg_mem->data[i].ptr == metadata)
					DBG_MEM_ERR("possible previous use: %s %hhu", dbg_mem->data[i].func, dbg_mem->data[i].used);
	}

	if ((rc = pthread_mutex_unlock(&(dbg_mem->mutex))) != 0) {
		DBG_MEM_ERR("unable to unlock mutex: %s", otel_strerror(rc));

		OTELC_RETURN();
	}

	OTELC_RETURN();
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
 *   memory, and records debugging information about the allocation.  This is a
 *   wrapper around the standard calloc function that integrates with the memory
 *   debugger.
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
 *
 * RETURN VALUE
 *   Returns a pointer to the reallocated memory, or nullptr on failure.
 */
void *otelc_dbg_realloc(const char *func, int line, void *ptr, size_t size)
{
	void *retptr;

	OTELC_FUNC_EX(MEM, "\"%s\", %d, %p, %zu", OTELC_STR_ARG(func), line, ptr, size);

	if (OTEL_NULL(ptr)) {
		retptr = malloc(DBG_MEM_SIZE(size));

		otelc_dbg_mem_alloc(func, line, nullptr, retptr, size);
	} else {
		const struct otelc_dbg_mem_metadata *metadata = DBG_MEM_DATA(ptr);

		/***
		 * If memory is not allocated via these debug functions, it must
		 * not be reallocated via them either.
		 */
		if (OTEL_NULL(metadata) || OTEL_NULL(metadata->data) || (metadata->magic != DBG_MEM_MAGIC)) {
			retptr = realloc(ptr, size);

			OTELC_RETURN_PTR(retptr);
		} else {
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
 *   integrates with the memory debugger.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
void otelc_dbg_free(const char *func, int line, void *ptr)
{
	const struct otelc_dbg_mem_metadata *metadata;

	OTELC_FUNC_EX(MEM, "\"%s\", %d, %p", OTELC_STR_ARG(func), line, ptr);

	otelc_dbg_mem_release(func, line, ptr, OTELC_DBG_MEM_OP_FREE);

	if (OTEL_NULL(ptr))
		OTELC_RETURN();

	metadata = DBG_MEM_DATA(ptr);
	if (OTEL_NULL(metadata) || OTEL_NULL(metadata->data) || (metadata->magic != DBG_MEM_MAGIC))
		free(ptr);
	else
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
 *   This is a wrapper around the standard strdup function that integrates with
 *   the memory debugger.
 *
 * RETURN VALUE
 *   Returns a pointer to the newly allocated string, or nullptr on failure.
 */
char *otelc_dbg_strdup(const char *func, int line, const char *s)
{
	size_t  len = 0;
	char   *retptr = nullptr;

	OTELC_FUNC_EX(MEM, "\"%s\", %d, \"%s\"", OTELC_STR_ARG(func), line, OTELC_STR_ARG(s));

	if (!OTEL_NULL(s)) {
		len    = strlen(s) + 1;
		retptr = OTEL_CAST_TYPEOF(retptr, malloc(DBG_MEM_SIZE(len)));
		if (!OTEL_NULL(retptr))
			(void)memcpy(DBG_MEM_PTR(retptr), s, len);
	}

	otelc_dbg_mem_alloc(func, line, nullptr, retptr, len);

	OTELC_RETURN_EX(OTEL_CAST_TYPEOF(retptr, DBG_MEM_RETURN(retptr)), typeof(retptr), "%p");
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
 *   information about the allocation.  This is a wrapper around the standard
 *   strndup function that integrates with the memory debugger.
 *
 * RETURN VALUE
 *   Returns a pointer to the newly allocated string, or nullptr on failure.
 */
char *otelc_dbg_strndup(const char *func, int line, const char *s, size_t size)
{
	size_t  len = 0;
	char   *retptr = nullptr;

	OTELC_FUNC_EX(MEM, "\"%s\", %d, \"%.*s\", %zu", OTELC_STR_ARG(func), line, OTEL_CAST_STATIC(int, size), OTELC_STR_ARG(s), size);

	if (!OTEL_NULL(s)) {
		len    = strnlen(s, size);
		retptr = OTEL_CAST_TYPEOF(retptr, malloc(DBG_MEM_SIZE(len + 1)));
		if (!OTEL_NULL(retptr)) {
			(void)memcpy(DBG_MEM_PTR(retptr), s, len);
			DBG_MEM_PTR(retptr)[len] = '\0';
		}
	}

	otelc_dbg_mem_alloc(func, line, nullptr, retptr, len + 1);

	OTELC_RETURN_EX(OTEL_CAST_TYPEOF(retptr, DBG_MEM_RETURN(retptr)), typeof(retptr), "%p");
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
 *   storage.  This must be called before any of the debugging memory functions
 *   are used.
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
 *   Disables the memory debugger and releases any associated resources.  After
 *   this function is called, the debugging memory functions will no longer
 *   track allocations.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
void otelc_dbg_mem_disable(void)
{
	OTELC_FUNC_EX(MEM, "");

	if (OTEL_NULL(dbg_mem))
		OTELC_RETURN();

	(void)pthread_mutex_destroy(&(dbg_mem->mutex));

	dbg_mem = nullptr;

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
	size_t                chunks = 0;
	uint64_t              size = 0;

	OTELC_FUNC_EX(MEM, "");

	if (OTEL_NULL(dbg_mem))
		OTELC_RETURN();

	OTELC_DBG(INFO, "--- Memory info -------------------------------------");
	OTELC_DBG(INFO, "  alloc/realloc: %" PRIu64 "/%" PRIu64 ", free/release: %" PRIu64 "/%" PRIu64, dbg_mem->op_cnt[0], dbg_mem->op_cnt[1], dbg_mem->op_cnt[2], dbg_mem->op_cnt[3]);
	OTELC_DBG(INFO, "  unused: %zu, reused: %zu, count: %zu", dbg_mem->unused, dbg_mem->reused, dbg_mem->count);
	for (size_t i = 0; i < dbg_mem->count; i++)
		if (dbg_mem->data[i].used) {
			OTELC_DBG(INFO, "  %zu %s(%p %zu)", chunks, dbg_mem->data[i].func, DBG_MEM_PTR(dbg_mem->data[i].ptr), dbg_mem->data[i].size);

			size += dbg_mem->data[i].size;
			chunks++;
		}

	if (chunks > 0)
		OTELC_DBG(INFO, "  allocated %" PRIu64 " byte(s) in %zu chunk(s)", size, chunks);

	if (dbg_mem->size != size)
		OTELC_DBG(INFO, "  size does not match: %" PRIu64 " != %" PRIu64, dbg_mem->size, size);

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
 *   allocation.  It allocates memory, copies the specified number of bytes from
 *   the source, and integrates with the memory debugger.
 *
 * RETURN VALUE
 *   Returns a pointer to the newly allocated memory, or nullptr on failure.
 */
void *otelc_dbg_memdup(const char *func, int line, const void *s, size_t size)
{
	void *retptr = nullptr;

	OTELC_FUNC_EX(MEM, "\"%s\", %d, %p, %zu", OTELC_STR_ARG(func), line, s, size);

	if (!OTEL_NULL(s)) {
		retptr = malloc(DBG_MEM_SIZE(size + 1));
		if (!OTEL_NULL(retptr)) {
			(void)memcpy(DBG_MEM_PTR(retptr), s, size);
			DBG_MEM_PTR(retptr)[size] = '\0';
		}
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
