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
#ifndef OPENTELEMETRY_C_WRAPPER_DBG_MALLOC_H
#define OPENTELEMETRY_C_WRAPPER_DBG_MALLOC_H

__CPLUSPLUS_DECL_BEGIN

#ifdef OTELC_USE_INTERNAL_INCLUDES
#  define OTELC_MALLOC(f,l,s)      OTELC_DBG_IFDEF(otelc_dbg_malloc(f, l, (s)),       malloc(s)             )
#  define OTELC_CALLOC(f,l,n,e)    OTELC_DBG_IFDEF(otelc_dbg_calloc(f, l, (n), (e)),  calloc((n), (e))      )
#  define OTELC_REALLOC(f,l,p,s)   OTELC_DBG_IFDEF(otelc_dbg_realloc(f, l, (p), (s)), realloc((p), (s))     )
#  define OTELC_FREE(f,l,p)        OTELC_DBG_IFDEF(otelc_dbg_free(f, l, (p)),         free(p)               )
#  define OTELC_MEMDUP(f,l,s,n)    OTELC_DBG_IFDEF(otelc_dbg_memdup(f, l, (s), (n)),  otelc_memdup((s), (n)))
#  define OTELC_MEMINFO()          OTELC_DBG_IFDEF(otelc_dbg_mem_info(),              while (0)             )
#  define OTELC_STRDUP(f,l,s)      OTELC_DBG_IFDEF(otelc_dbg_strdup(f, l, (s)),       strdup(s)             )
#  define OTELC_STRNDUP(f,l,s,n)   OTELC_DBG_IFDEF(otelc_dbg_strndup(f, l, (s), (n)), strndup((s), (n))     )
#  define OTELC_SFREE(p)           do { if ((p) != NULL) OTELC_FREE(__func__, __LINE__, (p)); } while (0)
#  define OTELC_SFREE_CLEAR(p)     do { if ((p) != NULL) { OTELC_FREE(__func__, __LINE__, (p)); (p) = NULL; } } while (0)
#else
#  define OTELC_MALLOC(s)          OTELC_DBG_IFDEF(otelc_dbg_malloc(__func__, __LINE__, (s)),       malloc(s)             )
#  define OTELC_CALLOC(n,e)        OTELC_DBG_IFDEF(otelc_dbg_calloc(__func__, __LINE__, (n), (e)),  calloc((n), (e))      )
#  define OTELC_REALLOC(p,s)       OTELC_DBG_IFDEF(otelc_dbg_realloc(__func__, __LINE__, (p), (s)), realloc((p), (s))     )
#  define OTELC_FREE(p)            OTELC_DBG_IFDEF(otelc_dbg_free(__func__, __LINE__, (p)),         free(p)               )
#  define OTELC_MEMDUP(s,n)        OTELC_DBG_IFDEF(otelc_dbg_memdup(__func__, __LINE__, (s), (n)),  otelc_memdup((s), (n)))
#  define OTELC_MEMINFO()          OTELC_DBG_IFDEF(otelc_dbg_mem_info(),                            while (0)             )
#  define OTELC_STRDUP(s)          OTELC_DBG_IFDEF(otelc_dbg_strdup(__func__, __LINE__, (s)),       strdup(s)             )
#  define OTELC_STRNDUP(s,n)       OTELC_DBG_IFDEF(otelc_dbg_strndup(__func__, __LINE__, (s), (n)), strndup((s), (n))     )
#  define OTELC_SFREE(p)           do { if ((p) != NULL) OTELC_FREE(p); } while (0)
#  define OTELC_SFREE_CLEAR(p)     do { if ((p) != NULL) { OTELC_FREE(p); (p) = NULL; } } while (0)
#endif /* OTELC_USE_INTERNAL_INCLUDES */

#ifdef OTELC_DBG_MEM
#  ifndef OTEL_CAST_TYPEOF
#    define OTEL_CAST_TYPEOF(t,e)   ((typeof(t))(e))
#  endif
#  define OTELC_DBG_MEM_TRACKING(p,n)                                \
	do {                                                         \
		int   errno_;                                        \
		void *ptr_;                                          \
		                                                     \
		if ((p) == NULL)                                     \
			break;                                       \
		                                                     \
		errno_ = errno;                                      \
		ptr_   = OTELC_MEMDUP(__func__, __LINE__, (p), (n)); \
		if (ptr_ != NULL) {                                  \
			free(p);                                     \
			(p) = OTEL_CAST_TYPEOF((p), ptr_);           \
		}                                                    \
		errno = errno_;                                      \
	} while (0)

enum {
	OTELC_DBG_MEM_OP_ALLOC,
	OTELC_DBG_MEM_OP_REALLOC,
	OTELC_DBG_MEM_OP_FREE,
	OTELC_DBG_MEM_OP_RELEASE,

	OTELC_DBG_MEM_OP_MAX
};

/***
 * Stores information about a single memory allocation for debugging and
 * leak-detection purposes.
 */
struct otelc_dbg_mem_data {
	void   *ptr;      /* Pointer to the allocated memory block. */
	size_t  size;     /* Size of the allocation in bytes. */
	char    func[63]; /* Name of the function that performed the allocation. */
	bool    used;     /* Indicates whether this record is currently in use. */
} __attribute__((packed));

/***
 * Maintains a collection of memory allocation records and aggregated statistics
 * used for debugging, leak detection and diagnostics.
 */
struct otelc_dbg_mem {
	struct otelc_dbg_mem_data *data;                         /* Array of allocation metadata records. */
	size_t                     count;                        /* Total number of records in the data array. */
	size_t                     unused;                       /* Number of currently unused records. */
	size_t                     reused;                       /* Number of records reused after being freed. */
	uint64_t                   size;                         /* Total size of currently allocated memory (bytes). */
	uint64_t                   op_cnt[OTELC_DBG_MEM_OP_MAX]; /* Operation counters (alloc, realloc, free, release). */
	pthread_mutex_t            mutex;                        /* Mutex protecting access to this structure. */
};


void *otelc_dbg_malloc(const char *func, int line, size_t size);
void *otelc_dbg_calloc(const char *func, int line, size_t nelem, size_t elsize);
void *otelc_dbg_realloc(const char *func, int line, void *ptr, size_t size);
void  otelc_dbg_free(const char *func, int line, void *ptr);
void *otelc_dbg_memdup(const char *func, int line, const void *s, size_t size);
char *otelc_dbg_strdup(const char *func, int line, const char *s);
char *otelc_dbg_strndup(const char *func, int line, const char *s, size_t size);
int   otelc_dbg_mem_init(struct otelc_dbg_mem *mem, struct otelc_dbg_mem_data *data, size_t count);
void  otelc_dbg_mem_disable(void);
void  otelc_dbg_mem_info(void);

#else
#  define OTELC_DBG_MEM_TRACKING(p,n)   while (0)
#endif /* OTELC_DBG_MEM */

__CPLUSPLUS_DECL_END
#endif /* OPENTELEMETRY_C_WRAPPER_DBG_MALLOC_H */

/*
 * Local variables:
 *  c-indent-level: 8
 *  c-basic-offset: 8
 * End:
 *
 * vi: noexpandtab shiftwidth=8 tabstop=8
 */
