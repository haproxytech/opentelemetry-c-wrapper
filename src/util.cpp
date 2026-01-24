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


static std::atomic<int> otelc_tid = 0;
otelc_ext_malloc_t      otelc_ext_malloc = OTELC_DBG_IFDEF(otelc_dbg_malloc, malloc);
otelc_ext_free_t        otelc_ext_free   = OTELC_DBG_IFDEF(otelc_dbg_free,   free);

#ifdef DEBUG
__thread int otelc_dbg_indent        = 0;
int          otelc_dbg_level         = 0x07ff;
int          otelc_dbg_tid_width     = 2;
bool         otelc_dbg_trigger_throw = false;


/***
 * NAME
 *   otelc_lib_constructor - performs library load-time initialization
 *
 * SYNOPSIS
 *   static void __attribute__((constructor)) otelc_lib_constructor(void)
 *
 * ARGUMENTS
 *   This function takes no arguments.
 *
 * DESCRIPTION
 *   This function is automatically invoked when the shared library is loaded.
 *   It performs one-time library initialization required before any other
 *   OpenTelemetry C wrapper functionality is used.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void __attribute__((constructor)) otelc_lib_constructor(void)
{
	OTELC_FUNC("");

	OTELC_RETURN();
}


/***
 * NAME
 *   otelc_lib_destructor - performs library unload-time cleanup
 *
 * SYNOPSIS
 *   static void __attribute__((destructor)) otelc_lib_destructor(void)
 *
 * ARGUMENTS
 *   This function takes no arguments.
 *
 * DESCRIPTION
 *   This function is automatically invoked when the shared library is unloaded.
 *   It performs final cleanup and resource release for the OpenTelemetry C
 *   wrapper.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
static void __attribute__((destructor)) otelc_lib_destructor(void)
{
	OTELC_FUNC("");

	OTELC_RETURN();
}

#endif /* DEBUG */


/***
 * NAME
 *   timespec_to_duration_us - converts seconds and nanoseconds to microseconds
 *
 * SYNOPSIS
 *   std::chrono::microseconds timespec_to_duration_us(const struct timespec *ts)
 *
 * ARGUMENTS
 *   ts - time in seconds and nanoseconds
 *
 * DESCRIPTION
 *   The time found in the timespec structure is converted to duration expressed
 *   in microseconds.
 *
 * RETURN VALUE
 *   Returns time duration in microseconds.
 */
std::chrono::microseconds timespec_to_duration_us(const struct timespec *ts)
{
	if (OTEL_NULL(ts))
		return std::chrono::microseconds{0};

	const auto duration = std::chrono::seconds{ts->tv_sec} + std::chrono::microseconds{ts->tv_nsec / 1000};

	return std::chrono::duration_cast<std::chrono::microseconds>(duration);
}


/***
 * NAME
 *   timespec_to_duration - converts seconds and nanoseconds to nanoseconds
 *
 * SYNOPSIS
 *   std::chrono::nanoseconds timespec_to_duration(const struct timespec *ts)
 *
 * ARGUMENTS
 *   ts - time in seconds and nanoseconds
 *
 * DESCRIPTION
 *   The time found in the timespec structure is converted to duration expressed
 *   in nanoseconds.
 *
 * RETURN VALUE
 *   Returns time duration in nanoseconds.
 */
std::chrono::nanoseconds timespec_to_duration(const struct timespec *ts)
{
	if (OTEL_NULL(ts))
		return std::chrono::nanoseconds{0};

	const auto duration = std::chrono::seconds{ts->tv_sec} + std::chrono::nanoseconds{ts->tv_nsec};

	return std::chrono::duration_cast<std::chrono::nanoseconds>(duration);
}


/***
 * NAME
 *   otelc_runtime - returns program execution duration in microseconds
 *
 * SYNOPSIS
 *   int64_t otelc_runtime(void)
 *
 * ARGUMENTS
 *   This function takes no arguments.
 *
 * DESCRIPTION
 *   This function returns the time period since the start of the program (that
 *   is, from the first call of this function when setting the initial time).
 *
 * RETURN VALUE
 *   Returns the time period (in microseconds) since the first call to this
 *   function.
 */
int64_t otelc_runtime(void)
{
	static const auto time_start = std::chrono::high_resolution_clock::now();
	const auto        time_elapsed = std::chrono::high_resolution_clock::now() - time_start;

	return std::chrono::duration_cast<std::chrono::microseconds>(time_elapsed).count();
}


/***
 * NAME
 *   otelc_thread_id - retrieves the current thread identifier
 *
 * SYNOPSIS
 *   static int otelc_thread_id(void)
 *
 * ARGUMENTS
 *   This function takes no arguments.
 *
 * DESCRIPTION
 *   Returns an integer identifying the current thread.  This can be used
 *   internally by the C wrapper library for thread-local operations, debugging,
 *   or associating data with specific threads in a multithreaded environment.
 *
 * RETURN VALUE
 *   Returns an integer representing the current thread ID.
 */
static int otelc_thread_id(void)
{
	thread_local int retval = otelc_tid++;

	return retval;
}


otelc_ext_thread_id_t otelc_ext_thread_id = otelc_thread_id;


/***
 * NAME
 *   otelc_ext_init - initializes the C wrapper library with custom callbacks
 *
 * SYNOPSIS
 *   void otelc_ext_init(otelc_ext_malloc_t func_malloc, otelc_ext_free_t func_free, otelc_ext_thread_id_t func_thread_id)
 *
 * ARGUMENTS
 *   func_malloc    - custom memory allocation function
 *   func_free      - custom memory deallocation function
 *   func_thread_id - function returning the current thread ID
 *
 * DESCRIPTION
 *   Sets up the C wrapper library to use the provided callbacks for memory
 *   management and thread identification.  By supplying custom allocation and
 *   free functions, the library can integrate seamlessly with different memory
 *   environments.  The thread ID callback allows the library to perform
 *   thread-local operations and maintain diagnostic or debugging information
 *   in multithreaded contexts.  This function must be called once before using
 *   any other library functions that depend on these callbacks.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
void otelc_ext_init(otelc_ext_malloc_t func_malloc, otelc_ext_free_t func_free, otelc_ext_thread_id_t func_thread_id)
{
	OTELC_FUNC("%p, %p, %p", func_malloc, func_free, func_thread_id);

	/***
	 * The functions otelc_ext_malloc/free() are used only for memory
	 * operations on the otelc_span and otelc_span_context structures.
	 */
	otelc_ext_malloc    = OTEL_NULL(func_malloc)    ? OTELC_DBG_IFDEF(otelc_dbg_malloc, malloc) : func_malloc;
	otelc_ext_free      = OTEL_NULL(func_free)      ? OTELC_DBG_IFDEF(otelc_dbg_free,   free)   : func_free;
	otelc_ext_thread_id = OTEL_NULL(func_thread_id) ? otelc_thread_id                           : func_thread_id;

	OTELC_RETURN();
}


/***
 * NAME
 *   otelc_nsleep - sleeps for a specified duration
 *
 * SYNOPSIS
 *   void otelc_nsleep(time_t sec, long nsec)
 *
 * ARGUMENTS
 *   sec  - number of seconds to sleep
 *   nsec - number of additional nanoseconds to sleep
 *
 * DESCRIPTION
 *   Suspends the execution of the calling thread for the specified duration,
 *   given by sec seconds plus nsec nanoseconds.  This function is useful for
 *   implementing delays or throttling in multithreaded or timing-sensitive
 *   operations.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
void otelc_nsleep(time_t sec, long nsec)
{
	struct timespec ts1 = { sec, nsec }, ts2, *req = &ts1, *rem = &ts2;

	while ((nanosleep(req, rem) == -1) && (errno == EINTR))
		std::swap(req, rem);
}


/***
 * NAME
 *   otelc_memdup - duplicates a memory block
 *
 * SYNOPSIS
 *   void *otelc_memdup(const void *s, size_t size)
 *
 * ARGUMENTS
 *   s    - pointer to the source memory block to duplicate
 *   size - number of bytes to copy from the source
 *
 * DESCRIPTION
 *   Allocates a new memory block of size + 1 bytes, copies size bytes from the
 *   source memory pointed to by s into it, and appends a null byte after the
 *   copied data.  The trailing null byte ensures that the result can be safely
 *   used as a C string if the source data is textual.
 *
 *   If s is a null pointer or size is 0, no allocation is performed and a null
 *   pointer is returned.
 *
 * RETURN VALUE
 *   Returns a pointer to the newly allocated memory block, or a null pointer
 *   if the arguments are invalid or memory allocation fails.
 */
void *otelc_memdup(const void *s, size_t size)
{
	void *retptr = nullptr;

	if (OTEL_NULL(s) || (size == 0))
		return retptr;

	retptr = OTELC_MALLOC(__func__, __LINE__, size + 1);
	if (!OTEL_NULL(retptr)) {
		(void)memcpy(retptr, s, size);
		(OTEL_CAST_STATIC(uint8_t *, retptr))[size] = '\0';
	}

	return retptr;
}


/***
 * NAME
 *   otelc_sprintf - formats a string and allocates memory for the result
 *
 * SYNOPSIS
 *   int otelc_sprintf(char **ret, const char *format, ...)
 *
 * ARGUMENTS
 *   ret    - address of a pointer where the newly allocated string will be stored
 *   format - printf-style format string
 *   ...    - arguments to be formatted according to the format string
 *
 * DESCRIPTION
 *   Formats a string according to the provided format specifier and arguments,
 *   allocates sufficient memory for the result, and stores the pointer to the
 *   new string in *ret.  If *ret already points to a previously allocated
 *   string, that memory is freed before the new string is assigned.
 *
 * RETURN VALUE
 *   Returns the number of bytes written to *ret, not including the terminating
 *   null byte, or OTELC_RET_ERROR in case of an error.
 */
int otelc_sprintf(char **ret, const char *format, ...)
{
	va_list  ap;
	char    *ptr = nullptr;
	int      retval = OTELC_RET_ERROR;

	if (OTEL_NULL(ret) || OTEL_NULL(format))
		return retval;

	va_start(ap, format);
	retval = vasprintf(&ptr, format, ap);
	va_end(ap);

	OTELC_SFREE_CLEAR(*ret);
	if (retval != -1) {
		OTELC_DBG_MEM_TRACKING(ptr, retval);

		*ret = ptr;
	}

	return retval;
}


/***
 * NAME
 *   otelc_strlcpy - copies a string with size bounds
 *
 * SYNOPSIS
 *   ssize_t otelc_strlcpy(char *dst, size_t dst_size, const char *src, size_t src_size)
 *
 * ARGUMENTS
 *   dst      - destination buffer
 *   dst_size - size of the destination buffer
 *   src      - source string to copy from
 *   src_size - number of bytes to copy from src, or 0 to use strlen(src)
 *
 * DESCRIPTION
 *   Copies at most dst_size - 1 bytes from src to dst, guaranteeing null
 *   termination of the result.  If src_size is 0, the length of src is
 *   determined automatically using strlen().  The actual number of bytes
 *   copied is the minimum of src_size and dst_size - 1.
 *
 * RETURN VALUE
 *   Returns the number of bytes copied (not counting the terminating null
 *   byte), or OTELC_RET_ERROR if dst or src is NULL or dst_size is 0.
 */
ssize_t otelc_strlcpy(char *dst, size_t dst_size, const char *src, size_t src_size)
{
	ssize_t retval = OTELC_RET_ERROR;

	if (OTEL_NULL(dst) || OTEL_NULL(src))
		return retval;
	else if (dst_size == 0)
		return retval;

	if (src_size == 0)
		src_size = strlen(src);

	retval = std::min(src_size, dst_size - 1);
	(void)memcpy(dst, src, retval);
	dst[retval] = '\0';

	return retval;
}


/***
 * NAME
 *   otelc_strtoi - converts a string to an integer
 *
 * SYNOPSIS
 *   bool otelc_strtoi(const char *str, char **endptr, bool flag_end, int base, int *retval, int val_min, int val_max, char **err)
 *
 * ARGUMENTS
 *   str      - the string to convert
 *   endptr   - a pointer to the character that stops the scan
 *   flag_end - flag to check if the end of the string is reached
 *   base     - the base for the conversion
 *   retval   - a pointer to the variable where the result will be stored
 *   val_min  - the minimum valid value
 *   val_max  - the maximum valid value
 *   err      - address of a pointer to store an error message on failure
 *
 * DESCRIPTION
 *   Converts a string to an integer with validation checks.  It checks for
 *   valid number format, range limits, and ensures the entire string is
 *   consumed if flag_end is set.  If val_min is less than or equal to val_max,
 *   the value is checked against this range.
 *
 * RETURN VALUE
 *   Returns true on success, false if an error occurs.
 */
bool otelc_strtoi(const char *str, char **endptr, bool flag_end, int base, int *retval, int val_min, int val_max, char **err)
{
	char *ptr = nullptr;
	long  value;

	if (OTEL_NULL(str) || OTEL_NULL(retval))
		return false;

	if (OTEL_NULL(endptr))
		endptr = &ptr;

	errno = 0;

	value = strtol(str, endptr, base);
	if ((*str == '\0') || (flag_end && (**endptr != '\0')))
		(void)otelc_sprintf(err, "Not a number: '%s'", str);
	else if (((value == LONG_MIN) || (value == LONG_MAX)) && (errno == ERANGE))
		(void)otelc_sprintf(err, "Value out of range: '%s'", str);
	else if ((val_min <= val_max) && !OTELC_IN_RANGE(value, val_min, val_max))
		(void)otelc_sprintf(err, "Value out of range [%d, %d]: '%s'", val_min, val_max, str);
	else if ((value == 0) && (errno == EINVAL))
		(void)otelc_sprintf(err, "Invalid value: '%s'", str);
	else if ((value < INT_MIN) || (value > INT_MAX))
		(void)otelc_sprintf(err, "Value out of range: '%s'", str);
	else {
		*retval = value;

		return true;
	}

	return false;
}


/***
 * NAME
 *   otelc_strhex - converts binary data to a hexadecimal string
 *
 * SYNOPSIS
 *   const char *otelc_strhex(const void *data, size_t size)
 *
 * ARGUMENTS
 *   data - pointer to the input buffer containing binary data
 *   size - number of bytes in the input buffer
 *
 * DESCRIPTION
 *   Converts the given binary data buffer into a lowercase hexadecimal string
 *   representation.  The returned string is null-terminated and suitable for
 *   logging or display purposes.
 *
 * RETURN VALUE
 *   Returns a pointer to a null-terminated string containing the hexadecimal
 *   representation of the input data.
 */
const char *otelc_strhex(const void *data, size_t size)
{
	thread_local char  retbuf[BUFSIZ];
	const uint8_t     *ptr = OTEL_CAST_TYPEOF(ptr, data);
	size_t             i;

	if (OTEL_NULL(data))
		return "(null)";
	else if (size == 0)
		return "()";

	for (i = 0, size <<= 1; (i < (sizeof(retbuf) - 2)) && (i < size); ptr++) {
		retbuf[i++] = otel_nibble_to_hex(*ptr >> 4);
		retbuf[i++] = otel_nibble_to_hex(*ptr & 0x0f);
	}

	retbuf[i] = '\0';

	return retbuf;
}


/***
 * NAME
 *   otelc_strctrl - converts binary data to a printable string with control characters escaped
 *
 * SYNOPSIS
 *   const char *otelc_strctrl(const void *data, size_t size)
 *
 * ARGUMENTS
 *   data - pointer to the input buffer containing binary data
 *   size - number of bytes in the input buffer
 *
 * DESCRIPTION
 *   Converts the given binary data buffer into a printable string by escaping
 *   non-printable and control characters (for example, newline, tab, or
 *   non-ASCII bytes).  This is intended for safe logging or diagnostic output.
 *
 * RETURN VALUE
 *   Returns a pointer to a null-terminated string containing a printable
 *   representation of the input data.
 */
const char *otelc_strctrl(const void *data, size_t size)
{
	thread_local char  retbuf[BUFSIZ];
	const uint8_t     *ptr = OTEL_CAST_TYPEOF(ptr, data);
	size_t             i, n = 0;

	if (OTEL_NULL(data))
		return "(null)";
	else if (size == 0)
		return "()";

	for (i = 0; (n < (sizeof(retbuf) - 1)) && (i < size); i++)
		retbuf[n++] = OTELC_IN_RANGE(ptr[i], 0x20, 0x7e) ? ptr[i] : '.';

	retbuf[n] = '\0';

	return retbuf;
}


#ifdef DEBUG

/***
 * NAME
 *   otelc_text_map_dump - dumps the contents of a text map to stdout (debug only)
 *
 * SYNOPSIS
 *   void otelc_text_map_dump(const struct otelc_text_map *text_map, const char *desc)
 *
 * ARGUMENTS
 *   text_map - pointer to an otelc_text_map structure whose contents will be printed
 *   desc     - optional descriptive label printed alongside the dump output (may be nullptr)
 *
 * DESCRIPTION
 *   Debug-only function that prints the key-value pairs contained in the given
 *   text map to standard output (stdout) in a human-readable form.  Intended
 *   for diagnostic and development use only.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
void otelc_text_map_dump(const struct otelc_text_map *text_map, const char *desc)
{
	OTELC_FUNC("%p, \"%s\"", text_map, OTELC_STR_ARG(desc));

	OTELC_DBG(DEBUG, "%s %p:{%s", OTEL_NULL(desc) ? "text_map" : desc, text_map, OTEL_NULL(text_map) ? " }" : "");
	if (!OTEL_NULL(text_map)) {
		OTELC_DBG(DEBUG, "  %p %p %p %zu/%zu %hhu", text_map->flags, text_map->key, text_map->value, text_map->count, text_map->size, text_map->is_dynamic);

		if (!OTEL_NULL(text_map->flags) && !OTEL_NULL(text_map->key) && !OTEL_NULL(text_map->value))
			for (size_t i = 0; i < text_map->count; i++)
				OTELC_DBG(DEBUG, "  0x%02hhx '%s' -> '%s'", text_map->flags[i], text_map->key[i], text_map->value[i]);

		OTELC_DBG(DEBUG, "}");
	}

	OTELC_RETURN();
}


/***
 * NAME
 *   otel_get_kv_cb - displays a single key-value pair of data
 *
 * SYNOPSIS
 *   bool otel_get_kv_cb(otel_nostd::string_view key, otel_nostd::string_view value)
 *
 * ARGUMENTS
 *   key   - key for which the value is displayed
 *   value - value associated with the key
 *
 * DESCRIPTION
 *   Callback function invoked for each key-value pair during baggage iteration.
 *   This function displays the key and value for debugging purposes.  Returning
 *   false would terminate the iteration early; however, this implementation
 *   always allows iteration to continue.
 *
 * RETURN VALUE
 *   Always returns true.
 */
bool otel_get_kv_cb(otel_nostd::string_view key, otel_nostd::string_view value)
{
	OTELC_DBG(OTEL, "'%s' -> '%s'", std::string(key).c_str(), std::string(value).c_str());

	return true;
}


/***
 * NAME
 *   otel_baggage_get_all_entries - wraps Baggage::GetAllEntries
 *
 * SYNOPSIS
 *   bool otel_baggage_get_all_entries(const otel_nostd::shared_ptr<otel_baggage::Baggage> &baggage, std::function<bool(otel_nostd::string_view, otel_nostd::string_view)> f)
 *
 * ARGUMENTS
 *   baggage - shared pointer to the baggage instance
 *   f       - std::function to be called for each entry
 *
 * DESCRIPTION
 *   A helper function that wraps the call to Baggage::GetAllEntries.  Since
 *   Baggage::GetAllEntries expects an otel_nostd::function_ref, this wrapper
 *   allows passing a std::function (including lambdas with captures) by
 *   casting it appropriately.
 *
 * RETURN VALUE
 *   Returns the result of Baggage::GetAllEntries (true on success, false if
 *   callback returned false).
 */
bool otel_baggage_get_all_entries(const otel_nostd::shared_ptr<otel_baggage::Baggage> &baggage, std::function<bool(otel_nostd::string_view, otel_nostd::string_view)> f)
{
	bool retval = false;

	OTELC_FUNC("<baggage>, <f>");

	if (!OTEL_NULL(baggage))
		retval = baggage->GetAllEntries(otel_nostd::function_ref<bool(otel_nostd::string_view, otel_nostd::string_view)>(f));

	OTELC_RETURN_EX(retval, bool, "%hhu");
}


/***
 * NAME
 *   otelc_value_dump - formats the contents of an otelc_value structure as a string (debug only)
 *
 * SYNOPSIS
 *   const char *otelc_value_dump(const struct otelc_value *value, const char *desc)
 *
 * ARGUMENTS
 *   value - pointer to an otelc_value structure to format
 *   desc  - optional descriptive label prepended to the output (may be nullptr)
 *
 * DESCRIPTION
 *   Debug-only function that formats the contents of the given otelc_value
 *   structure into a human-readable, null-terminated string.  The result
 *   includes the pointer address, the type tag, and the stored value.  If
 *   desc is not nullptr, it is prepended to the output.  The returned pointer
 *   refers to a thread-local buffer that is overwritten on each call from the
 *   same thread.
 *
 * RETURN VALUE
 *   Returns a pointer to a thread-local, null-terminated string containing
 *   the formatted representation of the value structure.
 */
const char *otelc_value_dump(const struct otelc_value *value, const char *desc)
{
	thread_local char  retbuf[BUFSIZ];
	const char        *s = OTELC_STR_ARG(desc);

	if (OTEL_NULL(value))
		(void)snprintf(retbuf, sizeof(retbuf), "%s%p:{ }", s, value);
	else if (value->u_type == OTELC_VALUE_NULL)
		(void)snprintf(retbuf, sizeof(retbuf), "%s%p:{ %d null '%s' }", s, value, value->u_type, OTELC_STR_ARG(value->u.value_string));
	else if (OTELC_IN_RANGE(value->u_type, OTELC_VALUE_BOOL, OTELC_VALUE_DATA))
		otelc_value_visit(value, [&](auto val) {
			using T = std::decay_t<decltype(val)>;

			if constexpr (std::is_same_v<T, bool>)
				(void)snprintf(retbuf, sizeof(retbuf), "%s%p:{ %d bool %hhu }", s, value, value->u_type, val);
			else if constexpr (std::is_same_v<T, int32_t>)
				(void)snprintf(retbuf, sizeof(retbuf), "%s%p:{ %d int32_t %" PRId32 " }", s, value, value->u_type, val);
			else if constexpr (std::is_same_v<T, int64_t>)
				(void)snprintf(retbuf, sizeof(retbuf), "%s%p:{ %d int64_t %" PRId64 " }", s, value, value->u_type, val);
			else if constexpr (std::is_same_v<T, uint32_t>)
				(void)snprintf(retbuf, sizeof(retbuf), "%s%p:{ %d uint32_t %" PRIu32 " }", s, value, value->u_type, val);
			else if constexpr (std::is_same_v<T, uint64_t>)
				(void)snprintf(retbuf, sizeof(retbuf), "%s%p:{ %d uint64_t %" PRIu64 " }", s, value, value->u_type, val);
			else if constexpr (std::is_same_v<T, double>)
				(void)snprintf(retbuf, sizeof(retbuf), "%s%p:{ %d double %f }", s, value, value->u_type, val);
			else if constexpr (std::is_same_v<T, const char *>)
				(void)snprintf(retbuf, sizeof(retbuf), "%s%p:{ %d %s '%s' }", s, value, value->u_type,
				               (value->u_type == OTELC_VALUE_STRING) ? "string" : "data", OTELC_STR_ARG(val));
		});
	else
		(void)snprintf(retbuf, sizeof(retbuf), "%s%p:{ %d %p }", s, value, value->u_type, &(value->u));

	return retbuf;
}


/***
 * NAME
 *   otelc_kv_dump - formats the contents of an otelc_kv structure as a string (debug only)
 *
 * SYNOPSIS
 *   const char *otelc_kv_dump(const struct otelc_kv *kv, const char *desc)
 *
 * ARGUMENTS
 *   kv   - pointer to an otelc_kv structure to format
 *   desc - optional descriptive label prepended to the output (may be nullptr)
 *
 * DESCRIPTION
 *   Debug-only function that formats the contents of the given key-value
 *   (otelc_kv) structure into a human-readable, null-terminated string.  The
 *   result includes the pointer address, the key, and the stored value with
 *   its type.  If desc is not nullptr, it is prepended to the output.  The
 *   returned pointer refers to a thread-local buffer that is overwritten on
 *   each call from the same thread.
 *
 * RETURN VALUE
 *   Returns a pointer to a thread-local, null-terminated string containing
 *   the formatted representation of the key-value structure.
 */
const char *otelc_kv_dump(const struct otelc_kv *kv, const char *desc)
{
	thread_local char  retbuf[BUFSIZ];
	const char        *d = OTELC_STR_ARG(desc), *key = OTEL_NULL(kv) ? "" : OTELC_STR_ARG(kv->key);

	if (OTEL_NULL(kv))
		(void)snprintf(retbuf, sizeof(retbuf), "%s%p:{ }", d, kv);
	else if (kv->value.u_type == OTELC_VALUE_NULL)
		(void)snprintf(retbuf, sizeof(retbuf), "%s%p:{ '%s' { null '%s' } }", d, kv, key, OTELC_STR_ARG(kv->value.u.value_string));
	else if (OTELC_IN_RANGE(kv->value.u_type, OTELC_VALUE_BOOL, OTELC_VALUE_DATA))
		otelc_value_visit(&kv->value, [&](auto val) {
			using T = std::decay_t<decltype(val)>;

			if constexpr (std::is_same_v<T, bool>)
				(void)snprintf(retbuf, sizeof(retbuf), "%s%p:{ '%s' { bool %hhu } }", d, kv, key, val);
			else if constexpr (std::is_same_v<T, int32_t>)
				(void)snprintf(retbuf, sizeof(retbuf), "%s%p:{ '%s' { int32_t %" PRId32 " } }", d, kv, key, val);
			else if constexpr (std::is_same_v<T, int64_t>)
				(void)snprintf(retbuf, sizeof(retbuf), "%s%p:{ '%s' { int64_t %" PRId64 " } }", d, kv, key, val);
			else if constexpr (std::is_same_v<T, uint32_t>)
				(void)snprintf(retbuf, sizeof(retbuf), "%s%p:{ '%s' { uint32_t %" PRIu32 " } }", d, kv, key, val);
			else if constexpr (std::is_same_v<T, uint64_t>)
				(void)snprintf(retbuf, sizeof(retbuf), "%s%p:{ '%s' { uint64_t %" PRIu64 " } }", d, kv, key, val);
			else if constexpr (std::is_same_v<T, double>)
				(void)snprintf(retbuf, sizeof(retbuf), "%s%p:{ '%s' { double %f } }", d, kv, key, val);
			else if constexpr (std::is_same_v<T, const char *>)
				(void)snprintf(retbuf, sizeof(retbuf), "%s%p:{ '%s' { %s '%s' } }", d, kv, key,
				               (kv->value.u_type == OTELC_VALUE_STRING) ? "string" : "data", OTELC_STR_ARG(val));
		});
	else
		(void)snprintf(retbuf, sizeof(retbuf), "%s%p:{ '%s' { %d %p } }", d, kv, key, kv->value.u_type, &(kv->value.u));

	return retbuf;
}

#endif /* DEBUG */


/***
 * NAME
 *   otelc_text_map_new - creates a new OpenTelemetry text map
 *
 * SYNOPSIS
 *   struct otelc_text_map *otelc_text_map_new(OTELC_DBG_IFDEF(OTELC_ARGS(const char *func, int line, ), ) struct otelc_text_map *text_map, size_t size)
 *
 * ARGUMENTS
 *   func     - (debug-only) name of the calling function
 *   line     - (debug-only) line number in the source code where this function is called
 *   text_map - optional pointer to an existing otelc_text_map structure to initialize
 *   size     - number of key-value entries the text map should be able to hold
 *
 * DESCRIPTION
 *   Initializes an otelc_text_map structure capable of holding up to size
 *   key-value pairs.  If text_map is nullptr, a new structure is allocated;
 *   otherwise, the provided structure is initialized in place.  When debugging
 *   is enabled, the calling function and line number are recorded for
 *   diagnostic purposes.  The returned text map can be used to store baggage
 *   or other key-value metadata in OpenTelemetry instrumentation.
 *
 * RETURN VALUE
 *   Returns a pointer to the newly allocated or initialized otelc_text_map,
 *   or nullptr on allocation failure.
 */
struct otelc_text_map *otelc_text_map_new(OTELC_DBG_IFDEF(OTELC_ARGS(const char *func, int line, ), ) struct otelc_text_map *text_map, size_t size)
{
	struct otelc_text_map *retptr = text_map;

	OTELC_FUNC("\"%s\", %d, %p, %zu", OTELC_STR_ARG(func), line, text_map, size);

	if (OTEL_NULL(retptr))
		retptr = OTEL_CAST_TYPEOF(retptr, OTELC_CALLOC(func, line, 1, sizeof(*retptr)));
	if (OTEL_NULL(retptr))
		OTELC_RETURN_PTR(retptr);

	retptr->count      = 0;
	retptr->size       = size;
	retptr->is_dynamic = OTEL_NULL(text_map);

	if (size == 0)
		/* Do nothing. */;
	else if (OTEL_NULL(retptr->flags = OTEL_CAST_TYPEOF(retptr->flags, OTELC_CALLOC(func, line, size, sizeof(*(retptr->flags))))))
		otelc_text_map_destroy(&retptr);
	else if (OTEL_NULL(retptr->key = OTEL_CAST_TYPEOF(retptr->key, OTELC_CALLOC(func, line, size, sizeof(*(retptr->key))))))
		otelc_text_map_destroy(&retptr);
	else if (OTEL_NULL(retptr->value = OTEL_CAST_TYPEOF(retptr->value, OTELC_CALLOC(func, line, size, sizeof(*(retptr->value))))))
		otelc_text_map_destroy(&retptr);

	OTELC_RETURN_PTR(retptr);
}


/***
 * NAME
 *   otelc_text_map_add - adds a key-value pair to a text map, allowing duplicate keys
 *
 * SYNOPSIS
 *   int otelc_text_map_add(OTELC_DBG_IFDEF(OTELC_ARGS(const char *func, int line, ), ) struct otelc_text_map *text_map, const char *key, size_t key_len, const char *value, size_t value_len, otelc_text_map_flags_t flags)
 *
 * ARGUMENTS
 *   func      - (debug-only) name of the calling function
 *   line      - (debug-only) line number in the source code where this function is called
 *   text_map  - pointer to the text map to which the entry will be added
 *   key       - pointer to the key string
 *   key_len   - length of the key string
 *   value     - pointer to the value string
 *   value_len - length of the value string
 *   flags     - bitmask controlling key/value ownership and lifetime
 *
 * DESCRIPTION
 *   Adds a key-value pair to the specified text map.  Multiple entries with
 *   identical keys are permitted and are stored as distinct entries.  The
 *   function associates the provided key and value with the map and manages
 *   their lifetime based on the supplied flags.
 *
 *   If OTELC_TEXT_MAP_DUP_KEY or OTELC_TEXT_MAP_DUP_VALUE is specified, the
 *   corresponding data is copied into internally managed storage.  Otherwise,
 *   the text map stores the provided pointer directly.
 *
 *   If OTELC_TEXT_MAP_FREE_KEY or OTELC_TEXT_MAP_FREE_VALUE is specified, the
 *   corresponding data will be released when the entry is removed or when the
 *   text map is destroyed.  These flags are typically used when ownership of
 *   dynamically allocated memory is transferred to the text map.
 *
 *   When debugging is enabled, the calling function name and line number are
 *   recorded for diagnostic purposes.
 *
 * RETURN VALUE
 *   Returns the current number of entries in the text map after the new
 *   key-value pair has been added on success, or OTELC_RET_ERROR on failure.
 */
int otelc_text_map_add(OTELC_DBG_IFDEF(OTELC_ARGS(const char *func, int line, ), ) struct otelc_text_map *text_map, const char *key, size_t key_len, const char *value, size_t value_len, otelc_text_map_flags_t flags)
{
	int retval = OTELC_RET_ERROR;

	OTELC_FUNC("\"%s\", %d, %p, %p, %zu, %p, %zu, 0x%04x", OTELC_STR_ARG(func), line, text_map, key, key_len, value, value_len, flags);

	if (OTEL_NULL(text_map) || OTEL_NULL(key) || OTEL_NULL(value))
		OTELC_RETURN_INT(retval);

	/***
	 * Check if it is necessary to increase the number of key-value pairs.
	 * The number of pairs is increased by half the current number of pairs
	 * (for example: 8 -> 12 -> 18 -> 27 -> 40 -> 60 ...).
	 */
	if (text_map->count >= text_map->size) {
		typeof(text_map->flags) ptr_flags;
		typeof(text_map->key)   ptr_key;
		typeof(text_map->value) ptr_value;
		const size_t            size_add = (text_map->size > 1) ? (text_map->size / 2) : 1;

		OTELC_DBG(OTEL, "reallocating text_map: %zu + %zu", text_map->size, size_add);

		if (OTEL_NULL(ptr_flags = OTEL_CAST_TYPEOF(ptr_flags, OTELC_REALLOC(func, line, text_map->flags, sizeof(*(text_map->flags)) * (text_map->size + size_add)))))
			OTELC_RETURN_INT(retval);
		text_map->flags = ptr_flags;
		(void)memset(text_map->flags + text_map->size, 0, sizeof(*(text_map->flags)) * size_add);

		if (OTEL_NULL(ptr_key = OTEL_CAST_TYPEOF(ptr_key, OTELC_REALLOC(func, line, text_map->key, sizeof(*(text_map->key)) * (text_map->size + size_add)))))
			OTELC_RETURN_INT(retval);
		text_map->key = ptr_key;
		(void)memset(text_map->key + text_map->size, 0, sizeof(*(text_map->key)) * size_add);

		if (OTEL_NULL(ptr_value = OTEL_CAST_TYPEOF(ptr_value, OTELC_REALLOC(func, line, text_map->value, sizeof(*(text_map->value)) * (text_map->size + size_add)))))
			OTELC_RETURN_INT(retval);
		text_map->value = ptr_value;
		(void)memset(text_map->value + text_map->size, 0, sizeof(*(text_map->value)) * size_add);

		text_map->size += size_add;
	}

	text_map->flags[text_map->count] = flags;
	text_map->key[text_map->count]   = ((flags & OTELC_TEXT_MAP_DUP_KEY) != 0) ? ((key_len > 0) ? OTELC_STRNDUP(func, line, key, key_len) : OTELC_STRDUP(func, line, key)) : OTEL_CAST_CONST(char *, key);
	text_map->value[text_map->count] = ((flags & OTELC_TEXT_MAP_DUP_VALUE) != 0) ? ((value_len > 0) ? OTELC_STRNDUP(func, line, value, value_len) : OTELC_STRDUP(func, line, value)) : OTEL_CAST_CONST(char *, value);

	if (!OTEL_NULL(text_map->key[text_map->count]) && !OTEL_NULL(text_map->value[text_map->count])) {
		retval = text_map->count++;
	} else {
		if ((text_map->flags[text_map->count] & OTELC_TEXT_MAP_FREE_KEY) != 0)
			OTELC_SFREE(text_map->key[text_map->count]);
		if ((text_map->flags[text_map->count] & OTELC_TEXT_MAP_FREE_VALUE) != 0)
			OTELC_SFREE(text_map->value[text_map->count]);
	}

	OTELC_RETURN_INT(retval);
}


/***
 * NAME
 *   otelc_text_map_free - releases a text map and its entries
 *
 * SYNOPSIS
 *   void otelc_text_map_free(struct otelc_text_map *text_map)
 *
 * ARGUMENTS
 *   text_map - pointer to the text map to be released
 *
 * DESCRIPTION
 *   Releases all resources associated with the entries stored in the specified
 *   text map, including all key-value pairs.  For each entry, the key and/or
 *   value data is released according to the ownership flags specified when the
 *   entry was added.
 *
 *   After this function returns, the text map is completely empty and may be
 *   reused to store new entries.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
void otelc_text_map_free(struct otelc_text_map *text_map)
{
	OTELC_FUNC("%p", text_map);

	if (OTEL_NULL(text_map))
		OTELC_RETURN();

	OTELC_DBG_TEXT_MAP(OTEL, "free", text_map);

	if (!OTEL_NULL(text_map->flags) && !OTEL_NULL(text_map->key))
		for (size_t i = 0; i < text_map->count; i++)
			if ((text_map->flags[i] & OTELC_TEXT_MAP_FREE_KEY) != 0)
				OTELC_SFREE(text_map->key[i]);

	if (!OTEL_NULL(text_map->flags) && !OTEL_NULL(text_map->value))
		for (size_t i = 0; i < text_map->count; i++)
			if ((text_map->flags[i] & OTELC_TEXT_MAP_FREE_VALUE) != 0)
				OTELC_SFREE(text_map->value[i]);

	OTELC_SFREE_CLEAR(text_map->flags);
	OTELC_SFREE_CLEAR(text_map->key);
	OTELC_SFREE_CLEAR(text_map->value);

	text_map->count = 0;
	text_map->size  = 0;

	OTELC_RETURN();
}


/***
 * NAME
 *   otelc_text_map_destroy - destroys a text map and releases all resources
 *
 * SYNOPSIS
 *   void otelc_text_map_destroy(struct otelc_text_map **text_map)
 *
 * ARGUMENTS
 *   text_map - address of a pointer to the text map to be destroyed
 *
 * DESCRIPTION
 *   Destroys the specified text map.  This function first removes all entries
 *   from the map by calling otelc_text_map_free(), releasing any resources
 *   associated with stored key-value pairs according to their ownership flags.
 *
 *   If the text map structure was dynamically allocated, the memory for the
 *   structure itself is released and the pointer referenced by text_map is set
 *   to nullptr.  If the structure was not dynamically allocated, only the
 *   contents of the map are cleared.
 *
 *   After this function returns, the text map contains no entries.  When the
 *   structure was dynamically allocated, the pointer is nullptr and must not
 *   be used.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
void otelc_text_map_destroy(struct otelc_text_map **text_map)
{
	OTELC_FUNC("%p:%p", OTELC_DPTR_ARGS(text_map));

	if (OTEL_NULL(text_map) || OTEL_NULL(*text_map))
		OTELC_RETURN();

	otelc_text_map_free(*text_map);
	if ((*text_map)->is_dynamic)
		OTELC_SFREE_CLEAR(*text_map);

	OTELC_RETURN();
}


/***
 * NAME
 *   otelc_value_strtonum - converts a string value to a numeric type
 *
 * SYNOPSIS
 *   int otelc_value_strtonum(struct otelc_value *value, otelc_value_type_t type)
 *
 * ARGUMENTS
 *   value - pointer to the value entry whose string is to be converted
 *   type  - target numeric type for the conversion
 *
 * DESCRIPTION
 *   Converts the string stored in the given value entry to the numeric type
 *   indicated by the type argument.  The source value entry must have type
 *   OTELC_VALUE_STRING or OTELC_VALUE_DATA; otherwise the function returns an
 *   error without modifying the entry.  The type argument must be in the range
 *   from OTELC_VALUE_BOOL to OTELC_VALUE_DOUBLE.
 *
 *   The string is parsed using the C library conversion function that matches
 *   the requested type: strtoul() for OTELC_VALUE_BOOL and OTELC_VALUE_UINT32,
 *   strtol() for OTELC_VALUE_INT32, strtoll() for OTELC_VALUE_INT64, strtoull()
 *   for OTELC_VALUE_UINT64, and strtod() for OTELC_VALUE_DOUBLE.  All integer
 *   conversions use a base of 0, allowing decimal, octal, and hexadecimal
 *   input.  For OTELC_VALUE_BOOL the result is false when the parsed value is
 *   zero, true otherwise.  For OTELC_VALUE_INT32 and OTELC_VALUE_UINT32, the
 *   parsed value is additionally range-checked against the 32-bit type limits;
 *   an out-of-range value is treated as a conversion error.
 *
 *   On successful conversion the value entry is updated in place: the union
 *   member is set to the parsed result, the type tag is changed to the
 *   requested type, and the original string is released.  For OTELC_VALUE_DATA
 *   the owned buffer is freed before overwriting; for OTELC_VALUE_STRING the
 *   pointer is simply overwritten.
 *
 *   If the string cannot be fully parsed (trailing characters remain) or a
 *   conversion error occurs, the value entry is left unchanged.
 *
 * RETURN VALUE
 *   Returns the target type cast to int on success, or OTELC_RET_ERROR
 *   on failure.
 */
int otelc_value_strtonum(struct otelc_value *value, otelc_value_type_t type)
{
	struct otelc_value  buffer;
	const char         *str;
	char               *endptr = nullptr;
	int                 retval = OTELC_RET_ERROR;

	OTELC_FUNC("%p, %d", value, type);

	if (OTEL_NULL(value))
		OTELC_RETURN_INT(retval);
	else if (!OTELC_IN_RANGE(type, OTELC_VALUE_BOOL, OTELC_VALUE_DOUBLE))
		OTELC_RETURN_INT(retval);

	OTELC_DBG_VALUE(DEBUG, "", value);

	str = (value->u_type == OTELC_VALUE_STRING) ? value->u.value_string : ((value->u_type == OTELC_VALUE_DATA) ? OTEL_CAST_TYPEOF(str, value->u.value_data) : nullptr);
	if (OTEL_NULL(str))
		OTELC_RETURN_INT(retval);

	errno = 0;

	buffer.u_type = type;
	if (type == OTELC_VALUE_BOOL) {
		buffer.u.value_bool = (strtoul(str, &endptr, 0) == 0) ? false : true;
	}
	else if (type == OTELC_VALUE_INT32) {
		long value_long = strtol(str, &endptr, 0);

#if (LONG_MAX > INT_MAX)
		if ((value_long < INT_MIN) || (value_long > INT_MAX))
			errno = ERANGE;
		else
#endif
			buffer.u.value_int32 = value_long;
	}
	else if (type == OTELC_VALUE_INT64) {
		buffer.u.value_int64 = strtoll(str, &endptr, 0);
	}
	else if (type == OTELC_VALUE_UINT32) {
		unsigned long value_ulong = strtoul(str, &endptr, 0);

#if (ULONG_MAX > UINT_MAX)
		if (value_ulong > UINT_MAX)
			errno = ERANGE;
		else
#endif
			buffer.u.value_uint32 = value_ulong;
	}
	else if (type == OTELC_VALUE_UINT64) {
		buffer.u.value_uint64 = strtoull(str, &endptr, 0);
	}
	else if (type == OTELC_VALUE_DOUBLE) {
		buffer.u.value_double = strtod(str, &endptr);
	}

	if ((errno != 0) || OTELC_STR_IS_VALID(endptr))
		OTELC_RETURN_INT(retval);

	retval = type;

	if (value->u_type == OTELC_VALUE_DATA)
		OTELC_SFREE_CLEAR(value->u.value_data);

	(void)memcpy(value, &buffer, sizeof(buffer));

	OTELC_DBG_VALUE(DEBUG, "", value);

	OTELC_RETURN_INT(retval);
}


/***
 * NAME
 *   otelc_value_new - allocates a new array of value entries
 *
 * SYNOPSIS
 *   struct otelc_value *otelc_value_new(size_t n)
 *
 * ARGUMENTS
 *   n - number of value entries to allocate
 *
 * DESCRIPTION
 *   Allocates memory for an array of n value entries (struct otelc_value).
 *   The entries are zero-initialized; the caller is responsible for setting
 *   their types and values as needed.
 *
 *   The allocated array can later be released using otelc_value_destroy().
 *
 * RETURN VALUE
 *   Returns a pointer to the newly allocated array of n value entries on
 *   success, or nullptr if the allocation fails.
 */
struct otelc_value *otelc_value_new(size_t n)
{
	struct otelc_value *retptr = nullptr;

	OTELC_FUNC("%zu", n);

	if (n > 0)
		retptr = OTEL_CAST_TYPEOF(retptr, OTELC_CALLOC(__func__, __LINE__, n, sizeof(*retptr)));

	OTELC_RETURN_PTR(retptr);
}


/***
 * NAME
 *   otelc_value_add - appends a value entry to a dynamic array
 *
 * SYNOPSIS
 *   int otelc_value_add(struct otelc_value **value, size_t *value_len, const void *data, size_t data_size)
 *
 * ARGUMENTS
 *   value     - pointer to a dynamically allocated array of otelc_value entries
 *   value_len - pointer to the current number of entries in the array
 *   data      - pointer to the data to be duplicated and stored
 *   data_size - size of the data in bytes
 *
 * DESCRIPTION
 *   Appends a new entry to the dynamic array of value entries pointed to by
 *   value.  The array is reallocated to accommodate one additional entry.
 *   If data is not nullptr and data_size is greater than zero, the data is
 *   duplicated into internally managed storage.  The new entry's value type
 *   is set to OTELC_VALUE_DATA.
 *
 *   On success, value is updated to point to the reallocated array and
 *   value_len is incremented.  If the array is successfully reallocated but the
 *   data duplication fails, OTELC_RET_ERROR is returned and value_len is not
 *   incremented; the unused slot remains at the end of the reallocated array.
 *   The caller is responsible for eventually releasing the array using
 *   otelc_value_destroy().
 *
 * RETURN VALUE
 *   Returns the new number of entries in the array on success,
 *   or OTELC_RET_ERROR on failure.
 */
int otelc_value_add(struct otelc_value **value, size_t *value_len, const void *data, size_t data_size)
{
	struct otelc_value *ptr;
	int                 retval = OTELC_RET_ERROR;

	OTELC_FUNC("%p:%p, %p, %p, %zu", OTELC_DPTR_ARGS(value), value_len, data, data_size);

	if (OTEL_NULL(value) || OTEL_NULL(value_len))
		OTELC_RETURN_INT(retval);

#ifdef DEBUG
	for (size_t i = 0; i < *value_len; i++)
		OTELC_DBG_VALUE(DEBUG, "", *value + i);
#endif

	ptr = OTEL_CAST_TYPEOF(ptr, OTELC_REALLOC(__func__, __LINE__, *value, sizeof(**value) * (*value_len + 1)));
	if (OTEL_NULL(ptr))
		OTELC_RETURN_INT(retval);

	*value = ptr;
	(void)memset(ptr + *value_len, 0, sizeof(*ptr));

	if (!OTEL_NULL(data) && (data_size > 0)) {
		ptr[*value_len].u.value_data = OTELC_MEMDUP(__func__, __LINE__, data, data_size);
		if (OTEL_NULL(ptr[*value_len].u.value_data))
			OTELC_RETURN_INT(retval);
	}

	ptr[*value_len].u_type = OTELC_VALUE_DATA;

	retval = ++(*value_len);

	OTELC_RETURN_INT(retval);
}


/***
 * NAME
 *   otelc_value_destroy - destroys an array of value entries and releases all resources
 *
 * SYNOPSIS
 *   void otelc_value_destroy(struct otelc_value **value, size_t n)
 *
 * ARGUMENTS
 *   value - pointer to the caller's pointer to an array of otelc_value entries
 *   n     - number of entries in the array
 *
 * DESCRIPTION
 *   Releases all resources associated with an array of value entries.  For each
 *   entry, the value data is freed if the value type is OTELC_VALUE_DATA.
 *
 *   After all entries are processed, the array itself is freed and the caller's
 *   pointer is set to nullptr.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
void otelc_value_destroy(struct otelc_value **value, size_t n)
{
	OTELC_FUNC("%p:%p, %zu", OTELC_DPTR_ARGS(value), n);

	if (OTEL_NULL(value) || OTEL_NULL(*value))
		OTELC_RETURN();

	for (size_t i = 0; i < n; i++) {
		OTELC_DBG_VALUE(DEBUG, "", *value + i);

		if ((*value)[i].u_type == OTELC_VALUE_DATA)
			OTELC_SFREE((*value)[i].u.value_data);
	}

	OTELC_SFREE_CLEAR(*value);

	OTELC_RETURN();
}


/***
 * NAME
 *   otelc_kv_new - allocates a new array of key-value entries
 *
 * SYNOPSIS
 *   struct otelc_kv *otelc_kv_new(size_t n)
 *
 * ARGUMENTS
 *   n - number of key-value entries to allocate
 *
 * DESCRIPTION
 *   Allocates memory for an array of n key-value entries (struct otelc_kv).
 *   The entries are zero-initialized; the caller is responsible for setting
 *   their keys, values, and ownership flags as needed.
 *
 *   The allocated array can later be released using otelc_kv_destroy().
 *
 * RETURN VALUE
 *   Returns a pointer to the newly allocated array of n key-value entries on
 *   success, or nullptr if the allocation fails.
 */
struct otelc_kv *otelc_kv_new(size_t n)
{
	struct otelc_kv *retptr = nullptr;

	OTELC_FUNC("%zu", n);

	if (n > 0)
		retptr = OTEL_CAST_TYPEOF(retptr, OTELC_CALLOC(__func__, __LINE__, n, sizeof(*retptr)));

	OTELC_RETURN_PTR(retptr);
}


/***
 * NAME
 *   otelc_kv_add - appends a key-value entry to a dynamic array
 *
 * SYNOPSIS
 *   int otelc_kv_add(struct otelc_kv **kv, size_t *kv_len, const char *key, const void *data, size_t data_size)
 *
 * ARGUMENTS
 *   kv        - pointer to a dynamically allocated array of otelc_kv entries
 *   kv_len    - pointer to the current number of entries in the array
 *   key       - null-terminated string used as the entry key, or nullptr
 *   data      - pointer to the data to be duplicated and stored
 *   data_size - size of the data in bytes
 *
 * DESCRIPTION
 *   Appends a new entry to the dynamic array of key-value entries pointed to
 *   by kv.  The array is reallocated to accommodate one additional entry.  If
 *   key is not nullptr, it is duplicated into internally managed storage and
 *   key_is_dynamic is set to true; otherwise, the key member is left unset
 *   (nullptr).  If data is not nullptr and data_size is greater than zero, the
 *   data is duplicated as well.  The new entry's value type is set to
 *   OTELC_VALUE_DATA.
 *
 *   On success, kv is updated to point to the reallocated array and kv_len is
 *   incremented.  If the array is successfully reallocated but key or data
 *   duplication fails, the partially allocated members of the new entry are
 *   freed, OTELC_RET_ERROR is returned, and kv_len is not incremented; the
 *   unused slot remains at the end of the reallocated array.  The caller is
 *   responsible for eventually releasing the array using otelc_kv_destroy().
 *
 * RETURN VALUE
 *   Returns the new number of entries in the array on success,
 *   or OTELC_RET_ERROR on failure.
 */
int otelc_kv_add(struct otelc_kv **kv, size_t *kv_len, const char *key, const void *data, size_t data_size)
{
	struct otelc_kv *ptr;
	bool             flag_alloc_error = false;
	int              retval = OTELC_RET_ERROR;

	OTELC_FUNC("%p:%p, %p, \"%s\", %p, %zu", OTELC_DPTR_ARGS(kv), kv_len, OTELC_STR_ARG(key), data, data_size);

	if (OTEL_NULL(kv) || OTEL_NULL(kv_len))
		OTELC_RETURN_INT(retval);

#ifdef DEBUG
	for (size_t i = 0; i < *kv_len; i++)
		OTELC_DBG_KV(DEBUG, "", *kv + i);
#endif

	ptr = OTEL_CAST_TYPEOF(ptr, OTELC_REALLOC(__func__, __LINE__, *kv, sizeof(**kv) * (*kv_len + 1)));
	if (OTEL_NULL(ptr))
		OTELC_RETURN_INT(retval);

	*kv = ptr;
	(void)memset(ptr + *kv_len, 0, sizeof(*ptr));

	if (!OTEL_NULL(key)) {
		ptr[*kv_len].key = OTELC_STRDUP(__func__, __LINE__, key);
		if (OTEL_NULL(ptr[*kv_len].key))
			flag_alloc_error = true;
		else
			ptr[*kv_len].key_is_dynamic = true;
	}

	if (!flag_alloc_error && !OTEL_NULL(data) && (data_size > 0)) {
		ptr[*kv_len].value.u.value_data = OTELC_MEMDUP(__func__, __LINE__, data, data_size);
		if (OTEL_NULL(ptr[*kv_len].value.u.value_data))
			flag_alloc_error = true;
	}

	if (flag_alloc_error) {
		OTELC_SFREE(ptr[*kv_len].key);
		OTELC_SFREE(ptr[*kv_len].value.u.value_data);
		ptr[*kv_len].key_is_dynamic = false;
	} else {
		ptr[*kv_len].value.u_type = OTELC_VALUE_DATA;

		retval = ++(*kv_len);
	}

	OTELC_RETURN_INT(retval);
}


/***
 * NAME
 *   otelc_kv_destroy - destroys an array of key-value entries and releases all resources
 *
 * SYNOPSIS
 *   void otelc_kv_destroy(struct otelc_kv **kv, size_t n)
 *
 * ARGUMENTS
 *   kv - pointer to the caller's pointer to an array of otelc_kv entries
 *   n  - number of entries in the array
 *
 * DESCRIPTION
 *   Releases all resources associated with an array of key-value entries.  For
 *   each entry, the key is freed if key_is_dynamic is set, and the value data
 *   is freed if the value type is OTELC_VALUE_DATA.
 *
 *   After all entries are processed, the array itself is freed and the caller's
 *   pointer is set to nullptr.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
void otelc_kv_destroy(struct otelc_kv **kv, size_t n)
{
	OTELC_FUNC("%p:%p, %zu", OTELC_DPTR_ARGS(kv), n);

	if (OTEL_NULL(kv) || OTEL_NULL(*kv))
		OTELC_RETURN();

	for (size_t i = 0; i < n; i++) {
		OTELC_DBG_KV(DEBUG, "", *kv + i);

		if ((*kv)[i].key_is_dynamic)
			OTELC_SFREE((*kv)[i].key);
		if ((*kv)[i].value.u_type == OTELC_VALUE_DATA)
			OTELC_SFREE((*kv)[i].value.u.value_data);
	}

	OTELC_SFREE_CLEAR(*kv);

	OTELC_RETURN();
}


/***
 * NAME
 *   otel_strerror - returns a human-readable string for an error code
 *
 * SYNOPSIS
 *   const char *otel_strerror(int errnum)
 *
 * ARGUMENTS
 *   errnum - the error code to translate
 *
 * DESCRIPTION
 *   Returns a human-readable string describing the specified error code.
 *   The string is stored in a statically allocated buffer owned by the
 *   implementation.  The caller must not modify or free the returned string.
 *
 *   Because the buffer is static, each call to otel_strerror() may overwrite
 *   the contents of the previous result.  If multiple error strings need to be
 *   preserved simultaneously, the caller should copy them to separate storage.
 *
 * RETURN VALUE
 *   Returns a pointer to a null-terminated string describing the error code,
 *   or a generic message if the error code is unknown.
 */
const char *otel_strerror(int errnum)
{
	thread_local char  retbuf[1024];
	const char        *retptr = retbuf;

	OTELC_FUNC("%d", errnum);

	*retbuf = '\0';
	errno   = 0;
#ifdef STRERROR_R_CHAR_P
	retptr = strerror_r(errnum, retbuf, sizeof(retbuf));
#else
	(void)strerror_r(errnum, retbuf, sizeof(retbuf));
#endif
	if (errno != 0)
		retptr = "Unknown error";

	OTELC_RETURN_PTR(retptr);
}


/***
 * NAME
 *   otelc_statistics - retrieves internal library statistics
 *
 * SYNOPSIS
 *   void otelc_statistics(char *buffer, size_t bufsiz)
 *
 * ARGUMENTS
 *   buffer - pointer to a character buffer to store the statistics string
 *   bufsiz - size of the buffer in bytes
 *
 * DESCRIPTION
 *   Populates the provided buffer with a formatted string containing internal
 *   statistics about the OpenTelemetry C wrapper library.  This may include
 *   counts of active handles for spans, contexts, instruments, and views,
 *   which is useful for debugging and monitoring the library's resource usage.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
void otelc_statistics(char *buffer, size_t bufsiz)
{
	OTELC_FUNC("%p, %zu", buffer, bufsiz);

	if (OTEL_NULL(buffer) || (bufsiz < 64))
		OTELC_RETURN();

#ifdef OTELC_USE_STATIC_HANDLE
	(void)snprintf(buffer, bufsiz, OTEL_HANDLE_FMT("span:") OTEL_HANDLE_FMT(", context:") OTEL_HANDLE_FMT(", instrument:") OTEL_HANDLE_FMT(", view:"), OTEL_HANDLE_ARGS(otel_span), OTEL_HANDLE_ARGS(otel_span_context), OTEL_HANDLE_ARGS(otel_instrument), OTEL_HANDLE_ARGS(otel_view));
#else
	char buffer_span[BUFSIZ] = "{ }", buffer_context[BUFSIZ] = "{ }", buffer_instrument[BUFSIZ] = "{ }", buffer_view[BUFSIZ] = "{ }";

	if (!OTEL_NULL(otel_span))
		(void)snprintf(buffer_span, sizeof(buffer_span), OTEL_HANDLE_FMT(""), OTEL_HANDLE_ARGS(otel_span));
	if (!OTEL_NULL(otel_span_context))
		(void)snprintf(buffer_context, sizeof(buffer_context), OTEL_HANDLE_FMT(""), OTEL_HANDLE_ARGS(otel_span_context));
	if (!OTEL_NULL(otel_instrument))
		(void)snprintf(buffer_instrument, sizeof(buffer_instrument), OTEL_HANDLE_FMT(""), OTEL_HANDLE_ARGS(otel_instrument));
	if (!OTEL_NULL(otel_view))
		(void)snprintf(buffer_view, sizeof(buffer_view), OTEL_HANDLE_FMT(""), OTEL_HANDLE_ARGS(otel_view));

	(void)snprintf(buffer, bufsiz, "span:%s, context:%s, instrument:%s, view:%s", buffer_span, buffer_context, buffer_instrument, buffer_view);
#endif /* OTELC_USE_STATIC_HANDLE */

	OTELC_RETURN();
}


/***
 * NAME
 *   otelc_statistics_check - verifies handle statistics against expected values
 *
 * SYNOPSIS
 *   int otelc_statistics_check(int type, size_t size, int64_t id, int64_t alloc_fail, int64_t erase, int64_t destroy)
 *
 * ARGUMENTS
 *   type       - handle type: 0 = span, 1 = span context, 2 = instrument, 3 = view
 *   size       - expected number of entries in the handle map
 *   id         - expected value of the id counter
 *   alloc_fail - expected value of the allocation failure counter
 *   erase      - expected value of the erase counter
 *   destroy    - expected value of the destroy counter
 *
 * DESCRIPTION
 *   Compares the current statistics of the specified handle type against the
 *   expected values.  The function checks the map size, id counter, allocation
 *   failure counter, erase counter, and destroy counter for the given handle
 *   type.
 *
 * RETURN VALUE
 *   Returns 0 if all statistics match, or a non-zero value with the bit
 *   corresponding to the handle type set on mismatch.
 */
int otelc_statistics_check(int type, size_t size, int64_t id, int64_t alloc_fail, int64_t erase, int64_t destroy)
{
#define OTEL_STAT_CHECK(p,a,b,c,d,e)   (((OTEL_HANDLE((p), total_map_size()) == (a)) && (OTEL_HANDLE((p), id) == (b)) && (OTEL_HANDLE((p), alloc_fail_cnt) == (c)) && (OTEL_HANDLE((p), erase_cnt) == (d)) && (OTEL_HANDLE((p), destroy_cnt) == (e))) ? 0 : 1)
	int retval = 0;

	OTELC_FUNC("%d, %zu, %" PRId64 ", %" PRId64 ", %" PRId64 ", %" PRId64, type, size, id, alloc_fail, erase, destroy);

	if (OTELC_USE_STATIC_HANDLE_IFDEF( , !OTEL_NULL(otel_span) &&) (type == 0))
		retval = OTEL_STAT_CHECK(otel_span, size, id, alloc_fail, erase, destroy) << type;
	else if (OTELC_USE_STATIC_HANDLE_IFDEF( , !OTEL_NULL(otel_span_context) &&) (type == 1))
		retval = OTEL_STAT_CHECK(otel_span_context, size, id, alloc_fail, erase, destroy) << type;
	else if (OTELC_USE_STATIC_HANDLE_IFDEF( , !OTEL_NULL(otel_instrument) &&) (type == 2))
		retval = OTEL_STAT_CHECK(otel_instrument, size, id, alloc_fail, erase, destroy) << type;
	else if (OTELC_USE_STATIC_HANDLE_IFDEF( , !OTEL_NULL(otel_view) &&) (type == 3))
		retval = OTEL_STAT_CHECK(otel_view, size, id, alloc_fail, erase, destroy) << type;

	OTELC_RETURN_INT(retval);
}


/***
 * NAME
 *   otelc_init - initializes the OpenTelemetry C wrapper library
 *
 * SYNOPSIS
 *   int otelc_init(const char *cfgfile, char **err)
 *
 * ARGUMENTS
 *   cfgfile - path to the YAML configuration file
 *   err     - address of a pointer to store an error message on failure
 *
 * DESCRIPTION
 *   Initializes the OpenTelemetry C wrapper library using the specified YAML
 *   configuration file.  This function must be called before any other library
 *   functions are used.
 *
 * RETURN VALUE
 *   Returns OTELC_RET_OK on success, or OTELC_RET_ERROR on failure.
 */
int otelc_init(const char *cfgfile, char **err)
{
	OTELC_FUNC("\"%s\", %p:%p", OTELC_STR_ARG(cfgfile), OTELC_DPTR_ARGS(err));

	if (!OTEL_NULL(otelc_fyd))
		OTEL_ERETURN_INT("The OpenTelemetry C wrapper library is already initialized");
	else if (OTEL_NULL(cfgfile))
		OTEL_ERETURN_INT("Invalid configuration file path");

	otelc_fyd = yaml_open(cfgfile, err);

	OTELC_RETURN_INT(OTEL_NULL(otelc_fyd) ? OTELC_RET_ERROR : OTELC_RET_OK);
}


/***
 * NAME
 *   otelc_deinit - deinitializes the OpenTelemetry C wrapper library
 *
 * SYNOPSIS
 *   void otelc_deinit(struct otelc_tracer **tracer, struct otelc_meter **meter, struct otelc_logger **logger)
 *
 * ARGUMENTS
 *   tracer - address of the tracer pointer to destroy, or NULL
 *   meter  - address of the meter pointer to destroy, or NULL
 *   logger - address of the logger pointer to destroy, or NULL
 *
 * DESCRIPTION
 *   Destroys the registered tracer, meter, and logger if they are non-NULL,
 *   closes the YAML configuration document, and shuts down the OpenTelemetry
 *   C wrapper library.  Each provider pointer is set to NULL after destruction.
 *   The external callback pointers registered via otelc_ext_init() are reset
 *   so that no references to the caller's code remain after this call.  This
 *   function should be called when the library is no longer needed, typically
 *   at application exit.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
void otelc_deinit(struct otelc_tracer **tracer, struct otelc_meter **meter, struct otelc_logger **logger)
{
	OTELC_FUNC("%p:%p, %p:%p, %p:%p", OTELC_DPTR_ARGS(tracer), OTELC_DPTR_ARGS(meter), OTELC_DPTR_ARGS(logger));

	if (!OTEL_NULL(otelc_fyd)) {
		yaml_close(&otelc_fyd);

		otelc_fyd = nullptr;
	}

	if (!OTEL_NULL(logger) && !OTEL_NULL(*logger))
		(*logger)->destroy(logger);

	if (!OTEL_NULL(meter) && !OTEL_NULL(*meter))
		(*meter)->destroy(meter);

	if (!OTEL_NULL(tracer) && !OTEL_NULL(*tracer))
		(*tracer)->destroy(tracer);

	otelc_ext_init(nullptr, nullptr, nullptr);

	OTELC_RETURN();
}

/*
 * Local variables:
 *  c-indent-level: 8
 *  c-basic-offset: 8
 * End:
 *
 * vi: noexpandtab shiftwidth=8 tabstop=8
 */
