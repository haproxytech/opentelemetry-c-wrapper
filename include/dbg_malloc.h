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
#ifndef _OPENTELEMETRY_C_WRAPPER_DBG_MALLOC_H_
#define _OPENTELEMETRY_C_WRAPPER_DBG_MALLOC_H_

#define DBG_MEM_ERR(f, ...)   OTELC_DBG(ERROR, "MEM_ERROR: " f, ##__VA_ARGS__)

/* 8 bytes - dBgM (dBgM ^ 0xffffffff) */
#define DBG_MEM_MAGIC         UINT64_C(0x6442674d9bbd98b2)
#define DBG_MEM_SIZE(n)       ((n) + sizeof(struct otelc_dbg_mem_metadata))
#define DBG_MEM_PTR(p)        DBG_MEM_SIZE(OTEL_CAST_TYPEOF(uint8_t *, (p)))
#define DBG_MEM_DATA(p)       OTEL_CAST_TYPEOF(struct otelc_dbg_mem_metadata *, OTEL_CAST_TYPEOF(uint8_t *, (p)) - DBG_MEM_SIZE(0))
#define DBG_MEM_RETURN(p)     (OTEL_NULL(p) ? nullptr : DBG_MEM_PTR(p))

/***
 * Associates a debug memory record with a magic value used to validate the
 * integrity and origin of the metadata structure.
 */
struct otelc_dbg_mem_metadata {
	struct otelc_dbg_mem_data *data;  /* Pointer to the associated allocation debug record. */
	uint64_t                   magic; /* Magic value used for validation and corruption detection. */
};

/***
 * Select the appropriate mallinfo API depending on platform support.
 *
 * mallinfo2() is preferred when available, as it provides 64-bit fields and
 * avoids overflow issues present in mallinfo().
 */
#ifdef HAVE_MALLINFO2
#  define otelc_mallinfo      mallinfo2
#  define PRI_MI              "zu"
#else
#  define otelc_mallinfo      mallinfo
#  ifdef __sun
#    define PRI_MI            "lu"
#  else
#    define PRI_MI            "d"
#  endif
#endif

#endif /* _OPENTELEMETRY_C_WRAPPER_DBG_MALLOC_H_ */

/*
 * Local variables:
 *  c-indent-level: 8
 *  c-basic-offset: 8
 * End:
 *
 * vi: noexpandtab shiftwidth=8 tabstop=8
 */
