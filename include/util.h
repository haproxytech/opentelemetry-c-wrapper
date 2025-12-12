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
#ifndef _OPENTELEMETRY_C_WRAPPER_UTIL_H_
#define _OPENTELEMETRY_C_WRAPPER_UTIL_H_

/***
 * Sets the number of buckets to the number needed to accomodate at least 8192
 * elements without exceeding maximum load factor and rehashes the container.
 */
constexpr size_t OTEL_HANDLE_RESERVE_COUNT = 8192;

#ifdef OTELC_USE_STATIC_HANDLE
#  define OTEL_HANDLE(h,m)           ((h).m)
#else
#  define OTEL_HANDLE(h,m)           ((h)->m)
#endif
#define OTEL_HANDLE_FMT(h)           h "{ < %zu/%zu > %" PRId64 " %zu %" PRId64 " %" PRId64 " %" PRId64 " }"
#define OTEL_HANDLE_ARGS(p)          OTEL_HANDLE((p), map).size(), OTEL_HANDLE((p), map).bucket_count(), OTEL_HANDLE((p), id), OTEL_HANDLE((p), peak_size), OTEL_HANDLE((p), alloc_fail_cnt), OTEL_HANDLE((p), erase_cnt), OTEL_HANDLE((p), destroy_cnt)
#define OTEL_DBG_HANDLE(l,h,p)       OTELC_DBG(_##l, OTEL_HANDLE_FMT(h), OTEL_HANDLE_ARGS(p))

/* NOTE: The offset from the demangled name was determined empirically. */
#define OTEL_HANDLE_DEMANGLED_NAME   (typeid(T).name() + 3)

#ifdef OTELC_USE_THREAD_SHARED_HANDLE
#  define OTEL_LOCK_TRACER(a, ...)   const std::lock_guard<std::mutex> guard_##a(OTEL_HANDLE(otel_##a, mutex), ##__VA_ARGS__)
#  define OTEL_LOCK(a,b)             std::lock(OTEL_HANDLE(otel_##a, mutex), OTEL_HANDLE(otel_##b, mutex)); OTEL_LOCK_TRACER(a, std::adopt_lock); OTEL_LOCK_TRACER(b, std::adopt_lock)
#  define THREAD_LOCAL
#else
#  define OTEL_LOCK_TRACER(...)      while (0)
#  define OTEL_LOCK(...)             while (0)
#  define THREAD_LOCAL               thread_local
#endif

/***
 * Visits an otelc_value by dispatching on u_type and calling the supplied
 * callable with the active union member.  For OTELC_VALUE_NULL and unknown
 * types, as well as NULL string/data pointers, an empty string is passed.
 */
template<typename F>
decltype(auto) otelc_value_visit(const struct otelc_value *v, F &&f)
{
	switch (v->u_type) {
		case OTELC_VALUE_BOOL:   return f(v->u.value_bool);
		case OTELC_VALUE_INT32:  return f(v->u.value_int32);
		case OTELC_VALUE_INT64:  return f(v->u.value_int64);
		case OTELC_VALUE_UINT32: return f(v->u.value_uint32);
		case OTELC_VALUE_UINT64: return f(v->u.value_uint64);
		case OTELC_VALUE_DOUBLE: return f(v->u.value_double);
		case OTELC_VALUE_STRING: return f((v->u.value_string != nullptr) ? v->u.value_string : "");
		case OTELC_VALUE_DATA:   return f((v->u.value_data != nullptr) ? OTEL_CAST_REINTERPRET(const char *, v->u.value_data) : "");
		default:                 return f("");
	}
}

#define OTEL_VALUE_ADD(arg_type, arg_map, arg_operation, arg_key, arg_value, arg_err, arg_fmt)                              \
	do {                                                                                                                \
		char **err = (arg_err);                                                                                     \
		                                                                                                            \
		try {                                                                                                       \
			if (OTEL_NULL(arg_value) || ((arg_value)->u_type == OTELC_VALUE_NULL))                              \
				arg_map.arg_operation((arg_key), "");                                                       \
			else if (OTELC_IN_RANGE((arg_value)->u_type, OTELC_VALUE_BOOL, OTELC_VALUE_DATA))                   \
				otelc_value_visit((arg_value), [&](auto val_) { arg_map.arg_operation((arg_key), val_); }); \
			else                                                                                                \
				OTEL_ERETURN##arg_type("%s", (arg_fmt));                                                    \
		}                                                                                                           \
		OTEL_CATCH_ERETURN( , OTEL_ERETURN##arg_type, arg_fmt)                                                      \
	} while (0)

/***
 * NAME
 *   otel_hash_function - custom hash function for int64_t keys
 *
 * DESCRIPTION
 *   A custom hash function for int64_t keys used in unordered_map.
 */
class otel_hash_function {
	public:
	size_t operator() (int64_t id) const noexcept { return id; }
};

/***
 * NAME
 *   otel_key_eq - custom key equality comparator for int64_t keys
 *
 * DESCRIPTION
 *   A custom key equality comparator for int64_t keys used in unordered_map.
 */
class otel_key_eq {
	public:
	bool operator() (int64_t id_a, int64_t id_b) const noexcept { return id_a == id_b; }
};

/***
 * NAME
 *   otel_handle - generic handle manager
 *
 * DESCRIPTION
 *   A generic handle manager for OpenTelemetry C wrapper objects.
 */
template<typename T>
struct otel_handle {
	std::unordered_map<
		int64_t,            /* Key type used to identify entries.   (class Key) */
		T,                  /* Value type stored in the handle map. (class T) */
		otel_hash_function, /* Custom hash function for keys.       (class Hash = std::hash<Key>) */
		otel_key_eq         /* Custom key equality comparator.      (class KeyEqual = std::equal_to<Key>) */
	> map;                      /* Map storing active handles indexed by ID. */

	int64_t    id;              /* Unique identifier generator (current handle ID). */
	size_t     peak_size;       /* Peak number of elements. */
	int64_t    alloc_fail_cnt;  /* Number of allocation failures. */
	int64_t    erase_cnt;       /* Number of entries erased from the map. */
	int64_t    destroy_cnt;     /* Number of entries destroyed during cleanup. */
#ifdef OTELC_USE_THREAD_SHARED_HANDLE
	std::mutex mutex;           /* Mutex protecting concurrent access to the handle map. */
#endif

	otel_handle() noexcept
	{
		OTELC_FUNCPP("", OTEL_HANDLE_DEMANGLED_NAME);

		map.clear();
		map.reserve(OTEL_HANDLE_RESERVE_COUNT);

		id             = 0;
		peak_size      = 0;
		alloc_fail_cnt = 0;
		erase_cnt      = 0;
		destroy_cnt    = 0;

		OTELC_RETURN();
	}

	/***
	 * NAME
	 *   find - look up a handle by its key
	 *
	 * ARGUMENTS
	 *   key - the key to look up in the handle map
	 *
	 * DESCRIPTION
	 *   Looks up a handle in the map by its key using a single search
	 *   operation.  Unlike map.at(), this method does not throw an
	 *   exception when the key is not found.
	 *
	 * RETURN VALUE
	 *   Returns the handle associated with the key, or nullptr if the key
	 *   is not found.
	 */
	T find(int64_t key) const noexcept
	{
		const auto it = map.find(key);

		return (it != map.end()) ? it->second : nullptr;
	}

	/***
	 * Acquire the mutex, invoke a callback on every entry, and clear the
	 * map.  Acquiring the mutex drains any in-flight operation that already
	 * passed its null-check but has not yet released the lock.
	 */
	template<typename F>
	void for_each_locked(F f) noexcept
	{
#ifdef OTELC_USE_THREAD_SHARED_HANDLE
		const std::lock_guard<std::mutex> guard(mutex);
#endif
		for (auto &it : map)
			f(it.first, it.second);
		map.clear();
	}

	/***
	 * Delete all entries and clear the map while holding the mutex, making
	 * a subsequent delete of the handle safe.
	 */
	void clear_locked() noexcept
	{
		for_each_locked([](int64_t id_ __maybe_unused, T handle) { delete handle; });
	}

	/***
	 * NOTE: If the otel_handle structure is declared as a static global,
	 * its destructor will run after otel_lib_destructor(), which is unsafe
	 * and therefore discouraged.
	 */
	~otel_handle() noexcept
	{
		OTELC_FUNCPP("", OTEL_HANDLE_DEMANGLED_NAME);

		OTELC_DBG(OTEL, "handle size: %" PRId64, map.size());

		for (auto &it : map)
			delete it.second;

		map.clear();

		OTELC_RETURN();
	}
};

/***
 * Generates a provider operation function (ForceFlush or Shutdown) that
 * forwards the call to the underlying OpenTelemetry SDK provider.
 */
#define OTEL_PROVIDER_OP(arg_signal, arg_ptr, arg_operation, arg_msg)                                                     \
	OTELC_FUNC("%p, %p", (arg_ptr), timeout);                                                                         \
	                                                                                                                  \
	if (OTEL_NULL(arg_ptr))                                                                                           \
		OTELC_RETURN_INT(OTELC_RET_ERROR);                                                                        \
	                                                                                                                  \
	const auto provider_sdk = OTEL_##arg_signal##_PROVIDER();                                                         \
	if (!OTEL_NULL(provider_sdk)) {                                                                                   \
		const auto us = OTEL_NULL(timeout) ? std::chrono::microseconds::max() : timespec_to_duration_us(timeout); \
		                                                                                                          \
		if (provider_sdk->arg_operation(us))                                                                      \
			OTELC_RETURN_INT(OTELC_RET_OK);                                                                   \
	}                                                                                                                 \
	                                                                                                                  \
	OTEL_##arg_signal##_ERETURN_INT(arg_msg);


extern otelc_ext_malloc_t otelc_ext_malloc;
extern otelc_ext_free_t   otelc_ext_free;


#ifdef DEBUG
#  define OTEL_DBG_BAGGAGE(b)   (void)otel_baggage_get_all_entries((b), otel_get_kv_cb)
#else
#  define OTEL_DBG_BAGGAGE(b)   while (0)
#endif

#ifdef DEBUG
bool                      otel_get_kv_cb(otel_nostd::string_view key, otel_nostd::string_view value);
#endif
bool                      otel_baggage_get_all_entries(const otel_nostd::shared_ptr<otel_baggage::Baggage> &baggage, std::function<bool(otel_nostd::string_view, otel_nostd::string_view)> f);
std::chrono::microseconds timespec_to_duration_us(const struct timespec *ts);
std::chrono::nanoseconds  timespec_to_duration(const struct timespec *ts);
const char *              otel_strerror(int errnum);

#endif /* _OPENTELEMETRY_C_WRAPPER_UTIL_H_ */

/*
 * Local variables:
 *  c-indent-level: 8
 *  c-basic-offset: 8
 * End:
 *
 * vi: noexpandtab shiftwidth=8 tabstop=8
 */
