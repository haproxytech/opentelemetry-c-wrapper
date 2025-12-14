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
#ifndef OPENTELEMETRY_C_WRAPPER_UTIL_H
#define OPENTELEMETRY_C_WRAPPER_UTIL_H

__CPLUSPLUS_DECL_BEGIN

#define OTELC_DBG_TEXT_MAP(l,h,p)                                         \
	OTELC_DBG_STRUCT(_##l, h, h " %p:{ %p %p %p %zu/%zu %hhu }", (p), \
	                 (p)->flags, (p)->key, (p)->value, (p)->count, (p)->size, (p)->is_dynamic)

#if defined(DEBUG) || defined(DEBUG_OTEL)
#  define OTELC_TEXT_MAP_DUMP(p,d)        otelc_text_map_dump((p), (d))
#else
#  define OTELC_TEXT_MAP_DUMP(...)        while (0)
#endif

#define OTELC_VALUE_STR(p)                (((p)->u_type == OTELC_VALUE_STRING) ? (p)->u.value_string : (typeof((p)->u.value_string))(p)->u.value_data)

#define OTELC_DBG_VALUE(l,h,p)            OTELC_DBG(_##l, "%s", otelc_value_dump((p), (h)))
#define OTELC_DBG_KV(l,h,p)               OTELC_DBG(_##l, "%s", otelc_kv_dump((p), (h)))

#define OTELC_TEXT_MAP_NEW(t,s)           otelc_text_map_new(OTELC_DBG_ARGS (t), (s))
#define OTELC_TEXT_MAP_ADD(t,k,K,v,V,f)   otelc_text_map_add(OTELC_DBG_ARGS (t), (k), (K), (v), (V), (f))

typedef enum {
	OTELC_TEXT_MAP_DUP_KEY    = 0x01, /* Duplicate the key data. */
	OTELC_TEXT_MAP_DUP_VALUE  = 0x02, /* Duplicate the value data. */
	OTELC_TEXT_MAP_FREE_KEY   = 0x04, /* Release the key data. */
	OTELC_TEXT_MAP_FREE_VALUE = 0x08, /* Release the value data. */

	OTELC_TEXT_MAP_DUP        = OTELC_TEXT_MAP_DUP_KEY | OTELC_TEXT_MAP_DUP_VALUE,
	OTELC_TEXT_MAP_FREE       = OTELC_TEXT_MAP_FREE_KEY | OTELC_TEXT_MAP_FREE_VALUE,
	OTELC_TEXT_MAP_AUTO       = OTELC_TEXT_MAP_DUP | OTELC_TEXT_MAP_FREE,
} otelc_text_map_flags_t;

/***
 * attribute value types
 */
typedef enum {
	OTELC_VALUE_NULL,
	OTELC_VALUE_BOOL,
	OTELC_VALUE_INT32,
	OTELC_VALUE_INT64,
	OTELC_VALUE_UINT32,
	OTELC_VALUE_UINT64,
	OTELC_VALUE_DOUBLE,
	OTELC_VALUE_STRING,
	OTELC_VALUE_DATA,
} otelc_value_type_t;

/***
 * The structure that represents values of various data types, using a union to
 * store the active value.  Exactly one member of the union u is valid at any
 * time, as indicated by u_type.
 */
struct otelc_value {
	otelc_value_type_t u_type;        /* Type tag indicating the active union member in 'u'. */
	union {
		bool        value_bool;   /* Boolean value. */
		int32_t     value_int32;  /* 32-bit signed integer. */
		int64_t     value_int64;  /* 64-bit signed integer. */
		uint32_t    value_uint32; /* 32-bit unsigned integer. */
		uint64_t    value_uint64; /* 64-bit unsigned integer. */
		double      value_double; /* Double-precision floating-point value. */
		const char *value_string; /* Read-only null-terminated string (may be NULL). */
		void       *value_data;   /* Owned pointer to additional data; freed when the structure is destroyed (may be NULL). */
	} u;
};

/***
 * The structure for storing key-value data pairs.  It extends otelc_value by
 * adding a key member that identifies the data.
 */
struct otelc_kv {
	char               *key;            /* null-terminated key string identifying the value. */
	bool                key_is_dynamic; /* True if the key is dynamically allocated. */
	struct otelc_value  value;          /* Stored value associated with the key. */
};

/***
 * Represents a collection of key-value string pairs.  Each pair has a
 * corresponding flag in the flags array that indicates whether the key and/or
 * value is dynamically or statically allocated.  The structure maintains the
 * number of stored pairs (count), the allocated capacity (size), and a flag
 * indicating whether the map structure itself is dynamically allocated
 * (is_dynamic).
 */
struct otelc_text_map {
	otelc_text_map_flags_t  *flags;      /* Pointer to flags associated with the map. */
	char                   **key;        /* Array of pointers to null-terminated strings representing the keys. */
	char                   **value;      /* Array of pointers to null-terminated strings representing the values corresponding to each key. */
	size_t                   count;      /* Number of key-value pairs currently stored in the map. */
	size_t                   size;       /* Allocated capacity of the key and value arrays. */
	bool                     is_dynamic; /* True if the structure was dynamically allocated, false otherwise. */
};

struct otelc_tracer;
struct otelc_meter;


#ifdef OTELC_DBG_MEM
typedef void *(*otelc_ext_malloc_t)(const char *, int, size_t);
typedef void  (*otelc_ext_free_t)(const char *, int, void *);
#else
typedef void *(*otelc_ext_malloc_t)(size_t);
typedef void  (*otelc_ext_free_t)(void *);
#endif
typedef int   (*otelc_ext_thread_id_t)(void);


extern otelc_ext_thread_id_t otelc_ext_thread_id;


#if defined(DEBUG) || defined(DEBUG_OTEL)
void                   otelc_text_map_dump(const struct otelc_text_map *text_map, const char *desc);
const char            *otelc_value_dump(const struct otelc_value *value, const char *desc);
const char            *otelc_kv_dump(const struct otelc_kv *kv, const char *desc);
#endif

void                   otelc_ext_init(otelc_ext_malloc_t func_malloc, otelc_ext_free_t func_free, otelc_ext_thread_id_t func_thread_id);
int64_t                otelc_runtime(void);
void                   otelc_nsleep(time_t sec, long nsec);
void                  *otelc_memdup(const void *s, size_t size);
int                    otelc_sprintf(char **ret, const char *format, ...) __attribute__((format(printf, 2, 3)));
ssize_t                otelc_strlcpy(char *dst, size_t dst_size, const char *src, size_t src_size);
bool                   otelc_strtoi(const char *str, char **endptr, bool flag_end, int base, int *retval, int val_min, int val_max, char **err);
const char            *otelc_strhex(const void *data, size_t size);
const char            *otelc_strctrl(const void *data, size_t size);
void                   otelc_statistics(char *buffer, size_t bufsiz);
int                    otelc_statistics_check(int type, size_t size, int64_t id, int64_t alloc_fail, int64_t erase, int64_t destroy);

struct otelc_text_map *otelc_text_map_new(OTELC_DBG_IFDEF(OTELC_ARGS(const char *func, int line, ), ) struct otelc_text_map *text_map, size_t size);
int                    otelc_text_map_add(OTELC_DBG_IFDEF(OTELC_ARGS(const char *func, int line, ), ) struct otelc_text_map *text_map, const char *key, size_t key_len, const char *value, size_t value_len, otelc_text_map_flags_t flags);
void                   otelc_text_map_free(struct otelc_text_map *text_map);
void                   otelc_text_map_destroy(struct otelc_text_map **text_map);
int                    otelc_value_strtonum(struct otelc_value *value, otelc_value_type_t type);
struct otelc_value    *otelc_value_new(size_t n);
int                    otelc_value_add(struct otelc_value **value, size_t *value_len, const void *data, size_t data_size);
void                   otelc_value_destroy(struct otelc_value **value, size_t n);
struct otelc_kv       *otelc_kv_new(size_t n);
int                    otelc_kv_add(struct otelc_kv **kv, size_t *kv_len, const char *key, const void *data, size_t data_size);
void                   otelc_kv_destroy(struct otelc_kv **kv, size_t n);

int                    otelc_init(const char *cfgfile, char **err);
void                   otelc_deinit(struct otelc_tracer **tracer, struct otelc_meter **meter);

__CPLUSPLUS_DECL_END
#endif /* OPENTELEMETRY_C_WRAPPER_UTIL_H */

/*
 * Local variables:
 *  c-indent-level: 8
 *  c-basic-offset: 8
 * End:
 *
 * vi: noexpandtab shiftwidth=8 tabstop=8
 */
