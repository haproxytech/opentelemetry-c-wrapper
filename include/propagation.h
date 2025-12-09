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
#ifndef _OPENTELEMETRY_C_WRAPPER_PROPAGATION_H_
#define _OPENTELEMETRY_C_WRAPPER_PROPAGATION_H_

/***
 * https://www.w3.org/TR/trace-context/
 *
 * Trace context is split into two individual propagation fields supporting
 * interoperability and vendor-specific extensibility:
 *  - traceparent describes the position of the incoming request in its trace
 *    graph in a portable, fixed-length format.
 *  - tracestate extends traceparent with vendor-specific data represented by
 *    a set of name/value pairs.  Storing information in tracestate is optional.
 */
#define OTEL_MAP_CARRIER              otel_map_carrier
#define OTEL_MAP_CARRIER_DATA         std::string, std::string
#define OTEL_MAP_CARRIER_CONTAINER    std::map<OTEL_MAP_CARRIER_DATA>

/***
 * NAME
 *   OTEL_MAP_CARRIER - generic TextMapCarrier implementation
 *
 * DESCRIPTION
 *   A generic TextMapCarrier implementation that stores and retrieves data
 *   from an underlying map-like container.  This class adapts a container
 *   (such as std::map) to the TextMapCarrier interface, enabling it to be
 *   used for propagating context information.
 */
template <typename T>
class OTEL_MAP_CARRIER : public otel_context::propagation::TextMapCarrier {
public:
	/***
	 * A constructor that sets the tm_data object to the one that was
	 * passed.
	 */
	template <typename U>
	explicit OTEL_MAP_CARRIER(U &&text_map) : tm_data(std::forward<U>(text_map))
	{
		OTELC_FUNCPP("", OTELC_STRINGIFY(OTEL_MAP_CARRIER));

		OTELC_RETURN();
	}

	/* Default member initialization. */
	OTEL_MAP_CARRIER() = default;

	/***
	 * NAME
	 *   Get - retrieves a value from the carrier by key
	 *
	 * SYNOPSIS
	 *   virtual otel_nostd::string_view Get(otel_nostd::string_view key) const noexcept override
	 *
	 * ARGUMENTS
	 *   key - the key to look up in the carrier
	 *
	 * DESCRIPTION
	 *   Retrieves a value from the carrier by key.
	 *
	 * RETURN VALUE
	 *   The value associated with the key, or an empty string_view if the
	 *   key is not found.
	 */
	virtual otel_nostd::string_view Get(otel_nostd::string_view key) const noexcept override
	{
		otel_nostd::string_view retval = "";

		OTELC_FUNCPP("\"%s\"", OTELC_STRINGIFY(OTEL_MAP_CARRIER), std::string(key).c_str());

		const auto it = tm_data.find(std::string(key));
		if (it != tm_data.end())
			retval = it->second;

		OTELC_DBG(OTEL, "'%s' -> '%s'", std::string(key).c_str(), std::string(retval).c_str());

		OTELC_FUNC_END("}");

		return retval;
	}

	/***
	 * NAME
	 *   Set - sets a key-value pair in the carrier
	 *
	 * SYNOPSIS
	 *   virtual void Set(otel_nostd::string_view key, otel_nostd::string_view value) noexcept override
	 *
	 * ARGUMENTS
	 *   key   - the key to set
	 *   value - the value to associate with the key
	 *
	 * DESCRIPTION
	 *   Sets a key-value pair in the carrier.
	 *
	 * RETURN VALUE
	 *   This function does not return a value.
	 */
	virtual void Set(otel_nostd::string_view key, otel_nostd::string_view value) noexcept override
	{
		OTELC_FUNCPP("\"%s\", \"%s\"", OTELC_STRINGIFY(OTEL_MAP_CARRIER), std::string(key).c_str(), std::string(value).c_str());

		tm_data[std::string{key}] = std::string{value};

		OTELC_RETURN();
	}

	/***
	 * NAME
	 *   Keys - iterates over all keys in the carrier
	 *
	 * SYNOPSIS
	 *   virtual bool Keys(otel_nostd::function_ref<bool(otel_nostd::string_view)> f) const noexcept
	 *
	 * ARGUMENTS
	 *   f - a function to be called for each key
	 *
	 * DESCRIPTION
	 *   Iterates over all keys in the carrier and invokes a callback
	 *   function for each key.
	 *
	 * RETURN VALUE
	 *   Returns true if the iteration completed successfully, false
	 *   otherwise.
	 */
	virtual bool Keys(otel_nostd::function_ref<bool(otel_nostd::string_view)> f) const noexcept
	{
		OTELC_FUNCPP("<f>", OTELC_STRINGIFY(OTEL_MAP_CARRIER));

		for (const auto &it : tm_data) {
			if (!f)
				OTELC_DBG(OTEL, "'%s' -> '%s'", it.first.c_str(), it.second.c_str());
			else if (!f(it.first))
				OTELC_RETURN_EX(false, bool, "%hhu");
		}

		OTELC_RETURN_EX(true, bool, "%hhu");
	}

	/***
	 * NAME
	 *   GetAllEntries - iterates over all key-value pairs in the carrier
	 *
	 * SYNOPSIS
	 *   bool GetAllEntries(otel_nostd::function_ref<bool(otel_nostd::string_view, otel_nostd::string_view)> f) const noexcept
	 *
	 * ARGUMENTS
	 *   f - a function to be called for each key-value pair
	 *
	 * DESCRIPTION
	 *   Iterates over all key-value pairs in the carrier and invokes a
	 *   callback function for each pair.
	 *
	 * RETURN VALUE
	 *   Returns true if the iteration completed successfully, false
	 *   otherwise.
	 */
	bool GetAllEntries(otel_nostd::function_ref<bool(otel_nostd::string_view, otel_nostd::string_view)> f) const noexcept
	{
		OTELC_FUNCPP("<f>", OTELC_STRINGIFY(OTEL_MAP_CARRIER));

		for (const auto &it : tm_data) {
			if (!f)
				OTELC_DBG(OTEL, "'%s' -> '%s'", it.first.c_str(), it.second.c_str());
			else if (!f(it.first, it.second))
				OTELC_RETURN_EX(false, bool, "%hhu");
		}

		OTELC_RETURN_EX(true, bool, "%hhu");
	}

	/***
	 * NAME
	 *   GetAllEntries - iterates over all key-value pairs in the carrier
	 *
	 * SYNOPSIS
	 *   bool GetAllEntries(std::function<bool(otel_nostd::string_view, otel_nostd::string_view)> f) const noexcept
	 *
	 * ARGUMENTS
	 *   f - a std::function to be called for each key-value pair
	 *
	 * DESCRIPTION
	 *   Overload of GetAllEntries that accepts a std::function.
	 *
	 * RETURN VALUE
	 *   Returns true if the iteration completed successfully, false
	 *   otherwise.
	 */
	bool GetAllEntries(std::function<bool(otel_nostd::string_view, otel_nostd::string_view)> f) const noexcept
	{
		return GetAllEntries(otel_nostd::function_ref<bool(otel_nostd::string_view, otel_nostd::string_view)>(f));
	}

	~OTEL_MAP_CARRIER() noexcept
	{
		OTELC_FUNCPP("", OTELC_STRINGIFY(OTEL_MAP_CARRIER));

		OTELC_RETURN();
	}

	T tm_data;

	const T& data() const noexcept { return tm_data; }
};

/***
 * This is actually a copy of the 'OTEL_MAP_CARRIER' class.  If at some point
 * it is determined that this class is not needed, it should be deleted.
 */
#define OTEL_HTTP_CARRIER             otel_http_carrier
#define OTEL_HTTP_CARRIER_DATA        std::string, std::string
#define OTEL_HTTP_CARRIER_CONTAINER   std::map<OTEL_HTTP_CARRIER_DATA>

/***
 * NAME
 *   OTEL_HTTP_CARRIER - TextMapCarrier implementation for HTTP headers
 *
 * DESCRIPTION
 *   A TextMapCarrier implementation for HTTP headers that stores and retrieves
 *   data from an underlying map-like container.  This class adapts a container
 *   (such as std::map) to the TextMapCarrier interface, enabling it to be used
 *   for propagating context information over HTTP.
 */
template <typename T>
class OTEL_HTTP_CARRIER : public otel_context::propagation::TextMapCarrier {
public:
	template <typename U>
	explicit OTEL_HTTP_CARRIER(U &&headers) : headers_data(std::forward<U>(headers))
	{
		OTELC_FUNCPP("", OTELC_STRINGIFY(OTEL_HTTP_CARRIER));

		OTELC_RETURN();
	}

	OTEL_HTTP_CARRIER() = default;

	/***
	 * NAME
	 *   Get - retrieves a value from the carrier by key
	 *
	 * SYNOPSIS
	 *   virtual otel_nostd::string_view Get(otel_nostd::string_view key) const noexcept override
	 *
	 * ARGUMENTS
	 *   key - the key to look up in the carrier
	 *
	 * DESCRIPTION
	 *   Retrieves a value from the carrier by key.
	 *
	 * RETURN VALUE
	 *   The value associated with the key, or an empty string_view if the
	 *   key is not found.
	 */
	virtual otel_nostd::string_view Get(otel_nostd::string_view key) const noexcept override
	{
		otel_nostd::string_view retval = "";

		OTELC_FUNCPP("\"%s\"", OTELC_STRINGIFY(OTEL_HTTP_CARRIER), std::string(key).c_str());

		const auto it = headers_data.find(std::string(key));
		if (it != headers_data.end())
			retval = it->second;

		OTELC_DBG(OTEL, "'%s' -> '%s'", std::string(key).c_str(), std::string(retval).c_str());

		OTELC_FUNC_END("}");

		return retval;
	}

	/***
	 * NAME
	 *   Set - sets a key-value pair in the carrier
	 *
	 * SYNOPSIS
	 *   virtual void Set(otel_nostd::string_view key, otel_nostd::string_view value) noexcept override
	 *
	 * ARGUMENTS
	 *   key   - the key to set
	 *   value - the value to associate with the key
	 *
	 * DESCRIPTION
	 *   Sets a key-value pair in the carrier.
	 *
	 * RETURN VALUE
	 *   This function does not return a value.
	 */
	virtual void Set(otel_nostd::string_view key, otel_nostd::string_view value) noexcept override
	{
		OTELC_FUNCPP("\"%s\", \"%s\"", OTELC_STRINGIFY(OTEL_HTTP_CARRIER), std::string(key).c_str(), std::string(value).c_str());

		headers_data[std::string{key}] = std::string{value};

		OTELC_RETURN();
	}

	/***
	 * NAME
	 *   Keys - iterates over all keys in the carrier
	 *
	 * SYNOPSIS
	 *   virtual bool Keys(otel_nostd::function_ref<bool(otel_nostd::string_view)> f) const noexcept
	 *
	 * ARGUMENTS
	 *   f - a function to be called for each key
	 *
	 * DESCRIPTION
	 *   Iterates over all keys in the carrier and invokes a callback
	 *   function for each key.
	 *
	 * RETURN VALUE
	 *   Returns true if the iteration completed successfully, false
	 *   otherwise.
	 */
	virtual bool Keys(otel_nostd::function_ref<bool(otel_nostd::string_view)> f) const noexcept
	{
		OTELC_FUNCPP("<f>", OTELC_STRINGIFY(OTEL_HTTP_CARRIER));

		for (const auto &it : headers_data) {
			if (!f)
				OTELC_DBG(OTEL, "'%s' -> '%s'", it.first.c_str(), it.second.c_str());
			else if (!f(it.first))
				OTELC_RETURN_EX(false, bool, "%hhu");
		}

		OTELC_RETURN_EX(true, bool, "%hhu");
	}

	/***
	 * NAME
	 *   GetAllEntries - iterates over all key-value pairs in the carrier
	 *
	 * SYNOPSIS
	 *   bool GetAllEntries(otel_nostd::function_ref<bool(otel_nostd::string_view, otel_nostd::string_view)> f) const noexcept
	 *
	 * ARGUMENTS
	 *   f - a function to be called for each key-value pair
	 *
	 * DESCRIPTION
	 *   Iterates over all key-value pairs in the carrier and invokes a
	 *   callback function for each pair.
	 *
	 * RETURN VALUE
	 *   Returns true if the iteration completed successfully, false
	 *   otherwise.
	 */
	bool GetAllEntries(otel_nostd::function_ref<bool(otel_nostd::string_view, otel_nostd::string_view)> f) const noexcept
	{
		OTELC_FUNCPP("<f>", OTELC_STRINGIFY(OTEL_HTTP_CARRIER));

		for (const auto &it : headers_data) {
			if (!f)
				OTELC_DBG(OTEL, "'%s' -> '%s'", it.first.c_str(), it.second.c_str());
			else if (!f(it.first, it.second))
				OTELC_RETURN_EX(false, bool, "%hhu");
		}

		OTELC_RETURN_EX(true, bool, "%hhu");
	}

	~OTEL_HTTP_CARRIER() noexcept
	{
		OTELC_FUNCPP("", OTELC_STRINGIFY(OTEL_HTTP_CARRIER));

		OTELC_RETURN();
	}

	T headers_data;

	const T& data() const noexcept { return headers_data; }
};

#endif /* _OPENTELEMETRY_C_WRAPPER_PROPAGATION_H_ */

/*
 * Local variables:
 *  c-indent-level: 8
 *  c-basic-offset: 8
 * End:
 *
 * vi: noexpandtab shiftwidth=8 tabstop=8
 */
