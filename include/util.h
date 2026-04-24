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
constexpr size_t OTEL_HANDLE_MAP_SHARDS    = 64;

#ifdef OTELC_USE_STATIC_HANDLE
#  define OTEL_HANDLE(h,m)           ((h).m)
#else
#  define OTEL_HANDLE(h,m)           ((h)->m)
#endif
#define OTEL_HANDLE_FMT(h)           h "{ < %zu/%zu/%zu > %" PRId64 " %zu %" PRId64 " %" PRId64 " %" PRId64 " }"
#define OTEL_HANDLE_ARGS(p)          OTEL_HANDLE((p), total_map_size()), OTEL_HANDLE((p), max_bucket_count()), OTEL_HANDLE((p), shards.size()), OTEL_HANDLE((p), id).load(), OTEL_HANDLE((p), peak_size).load(), OTEL_HANDLE((p), alloc_fail_cnt).load(), OTEL_HANDLE((p), erase_cnt).load(), OTEL_HANDLE((p), destroy_cnt).load()
#define OTEL_DBG_HANDLE(l,h,p)       OTELC_DBG(_##l, OTEL_HANDLE_FMT(h), OTEL_HANDLE_ARGS(p))
#define OTEL_HANDLE_PEAK_SIZE(h,s)                                                                                                      \
	do {                                                                                                                            \
		size_t peak_size = OTEL_HANDLE(h, peak_size).load();                                                                    \
		while (!OTEL_HANDLE(h, peak_size).compare_exchange_weak(peak_size, std::max(peak_size, OTEL_HANDLE(h, s.map).size()))); \
	} while (0)

/***
 * Encapsulates the try/emplace/catch/peak-size pattern for inserting a handle
 * into a sharded map.  Uses 'bool emplace_ok' to track whether the emplace
 * succeeded, avoiding the verbose std::pair<iterator,bool>.
 */
#define OTEL_HANDLE_EMPLACE(map_name, idx, handle_var, cleanup, err_macro, dup_msg, exc_msg) \
	do {                                                                                 \
		bool emplace_ok = false;                                                     \
		try {                                                                        \
			OTEL_DBG_THROW();                                                    \
			emplace_ok = OTEL_HANDLE(map_name, get_shard(idx).map).emplace(      \
				(idx), (handle_var)).second;                                 \
			                                                                     \
			if (!emplace_ok) {                                                   \
				cleanup;                                                     \
				                                                             \
				err_macro(dup_msg);                                          \
			}                                                                    \
		}                                                                            \
		OTEL_CATCH_SIGNAL_RETURN({                                                   \
			if (emplace_ok)                                                      \
				OTEL_HANDLE(map_name, get_shard(idx).map).erase(idx);        \
			                                                                     \
			cleanup;                                                             \
			}, err_macro, exc_msg                                                \
		)                                                                            \
		                                                                             \
		OTEL_HANDLE_PEAK_SIZE(map_name, get_shard(idx));                             \
	} while (0)

/* NOTE: The offset from the demangled name was determined empirically. */
#define OTEL_HANDLE_DEMANGLED_NAME   (typeid(T).name() + 3)

#define OTEL_LOCK_METER(a)           const std::lock_guard<std::mutex> guard_##a(OTEL_HANDLE(otel_##a, get_shard(0).mutex))
#ifdef OTELC_USE_THREAD_SHARED_HANDLE
#  define OTEL_LOCK_TRACER(a,n)      const std::lock_guard<std::mutex> guard_##a(OTEL_HANDLE(otel_##a, get_shard(n).mutex))
#  define THREAD_LOCAL
constexpr bool OTEL_HANDLE_SHARED = true;
#else
#  define OTEL_LOCK_TRACER(...)      while (0)
#  define THREAD_LOCAL               thread_local
constexpr bool OTEL_HANDLE_SHARED = false;
#endif

/***
 * Returns the value as uint64_t.  If the value type is OTELC_VALUE_INT64, the
 * signed value is cast to uint64_t.  The caller must ensure that the int64
 * value is non-negative before calling this function.
 */
#define OTELC_VALUE_TO_UINT64(v)     (((v)->u_type == OTELC_VALUE_INT64) ? OTEL_CAST_STATIC(uint64_t, (v)->u.value_int64) : (v)->u.value_uint64)

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
				OTEL_ERR_RETURN##arg_type("%s", (arg_fmt));                                                 \
		}                                                                                                           \
		OTEL_CATCH_SIGNAL_RETURN( , OTEL_ERR_RETURN##arg_type, arg_fmt)                                             \
	} while (0)

/***
 * otel_log_handler - forwards SDK internal log messages to a C callback.
 */
class otel_log_handler : public otel_sdk_internal_log::LogHandler
{
public:
	otel_log_handler(otelc_log_handler_cb_t handler, void *ctx, bool forward_attr)
		: handler_(handler), ctx_(ctx), forward_attr_(forward_attr) {}

	void Handle(otel_sdk_internal_log::LogLevel level, const char *file, int line, const char *msg, const otel_sdk_common::AttributeMap &attributes) noexcept override;

private:
	otelc_log_handler_cb_t  handler_;
	void                   *ctx_;
	bool                    forward_attr_;
};

/***
 * Safe map lookup that returns a default-constructed value (nullptr for
 * pointers) when the key is not found, instead of throwing std::out_of_range.
 */
template <typename T>
typename T::mapped_type otel_map_find(T &map, const typename T::key_type &key) noexcept
{
	auto it = map.find(key);

	return (it != map.end()) ? it->second : typename T::mapped_type{};
}

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
template<typename T, bool shared = true>
struct otel_handle {
	struct shard {
		std::unordered_map<
			int64_t,            /* Key type used to identify entries.   (class Key) */
			T,                  /* Value type stored in the handle map. (class T) */
			otel_hash_function, /* Custom hash function for keys.       (class Hash = std::hash<Key>) */
			otel_key_eq         /* Custom key equality comparator.      (class KeyEqual = std::equal_to<Key>) */
		> map;                      /* Map storing active handles indexed by ID. */
		std::mutex mutex;           /* Mutex protecting concurrent access to the handle map. */
	};

	std::atomic<int64_t>      id;             /* Unique identifier generator (current handle ID). */
	std::atomic<size_t>       peak_size;      /* Peak number of elements in a single shard. */
	std::atomic<int64_t>      alloc_fail_cnt; /* Number of allocation failures. */
	std::atomic<int64_t>      erase_cnt;      /* Number of entries erased from the map. */
	std::atomic<int64_t>      destroy_cnt;    /* Number of entries destroyed during cleanup. */
	std::vector<struct shard> shards;         /* Independently-locked partitions of the handle map. */

	void otel_handle_init(void)
	{
		OTELC_FUNCPP("", OTEL_HANDLE_DEMANGLED_NAME);

		for (auto &it : shards) {
			it.map.clear();
			it.map.reserve(OTEL_HANDLE_RESERVE_COUNT / shards.size());
		}

		id             = 0;
		peak_size      = 0;
		alloc_fail_cnt = 0;
		erase_cnt      = 0;
		destroy_cnt    = 0;

		OTELC_RETURN();
	}

	otel_handle() : shards(OTEL_HANDLE_MAP_SHARDS)
	{
		otel_handle_init();
	}

	otel_handle(size_t num_shards) : shards(num_shards)
	{
		if ((num_shards == 0) || ((num_shards & (num_shards - 1)) != 0))
			throw std::invalid_argument("Invalid vector size, must be a power of two: " + std::to_string(num_shards));

		otel_handle_init();
	}

	/***
	 * Get the shard index for a given ID.
	 */
	size_t get_shard_index(int64_t key) const noexcept
	{
		return OTEL_CAST_STATIC(size_t, key & (shards.size() - 1));
	}

	/***
	 * Get the shard for a given ID.
	 */
	struct shard &get_shard(int64_t key) noexcept
	{
		return shards[get_shard_index(key)];
	}

	/***
	 * Calculate the total memory usage of this handle structure.  This
	 * includes the struct itself, the vector's heap-allocated shard storage
	 * and each unordered_map's buckets and nodes.  It does not account for
	 * objects pointed to by stored values.
	 */
	size_t size_of() const noexcept
	{
		size_t retval = sizeof(*this);

		/* Heap-allocated shard storage managed by the vector. */
		retval += shards.capacity() * sizeof(struct shard);

		for (const auto &it : shards) {
			/* Bucket array of the unordered_map. */
			retval += it.map.bucket_count() * sizeof(void *);

			/* Each map node: next-pointer and key-value pair. */
			retval += it.map.size() * (sizeof(void *) + sizeof(std::pair<const int64_t, T>));
		}

		return retval;
	}

	/***
	 * Returns the total number of elements across all shards.
	 */
	size_t total_map_size() const noexcept
	{
		size_t retval = 0;

		for (const auto &it : shards)
			retval += it.map.size();

		return retval;
	}

	/***
	 * Returns the maximum bucket count among all shards.
	 */
	size_t max_bucket_count() const noexcept
	{
		size_t retval = 0;

		for (const auto &it : shards)
			retval = std::max(retval, it.map.bucket_count());

		return retval;
	}

	/***
	 * For each shard, acquire its mutex, invoke a callback on every entry,
	 * and clear the map.  Acquiring the mutex drains any in-flight
	 * operation that already passed its null-check but has not yet
	 * released the shard lock.
	 */
	template<typename F>
	void for_each_locked(F f) noexcept
	{
		for (auto &it_shard : shards) {
			if constexpr (shared) {
				const std::lock_guard<std::mutex> guard(it_shard.mutex);

				for (auto &it : it_shard.map)
					f(it.first, it.second);
				it_shard.map.clear();
			} else {
				for (auto &it : it_shard.map)
					f(it.first, it.second);
				it_shard.map.clear();
			}
		}
	}

	/***
	 * Delete all entries and clear every shard while holding the per-shard
	 * mutex, making a subsequent delete of the handle safe.
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
		OTELC_DBG_IFDEF(size_t total_size = 0, );

		OTELC_FUNCPP("", OTEL_HANDLE_DEMANGLED_NAME);

		for (auto &it_shard : shards) {
			OTELC_DBG_IFDEF(total_size += it_shard.map.size(), );

			for (auto &it : it_shard.map)
				delete it.second;

			it_shard.map.clear();
		}

		OTELC_DBG(OTEL, "handle size: %zu", total_size);

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
	OTEL_##arg_signal##_RETURN_INT(arg_msg);

/***
 * Internal definition of the library context.  Owns the parsed YAML
 * configuration document.  Multiple instances may coexist within a single
 * process, each loaded from a distinct configuration file.
 */
struct otelc_ctx {
	OTEL_YAML_DOC *fyd;  /* Parsed YAML configuration document. */
	char          *name; /* Caller-provided name identifying the context. */
};


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
