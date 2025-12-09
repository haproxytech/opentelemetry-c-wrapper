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
#ifndef _OPENTELEMETRY_C_WRAPPER_STD_H_
#define _OPENTELEMETRY_C_WRAPPER_STD_H_

namespace otel
{
	/***
	 * NAME
	 *   make_unique_nothrow - creates a std::unique_ptr with nothrow
	 *
	 * DESCRIPTION
	 *   Creates a std::unique_ptr using a non-throwing new expression,
	 *   returning a nullptr on allocation failure.  If the constructor of
	 *   T throws, the new expression automatically frees the raw memory
	 *   before propagating the exception, which is then caught and nullptr
	 *   is returned.  The std::unique_ptr constructor is noexcept and does
	 *   not allocate a control block, so it cannot throw.
	 */
	template <class T, class... Args>
	std::unique_ptr<T> make_unique_nothrow(Args &&... args) noexcept
	{
		try {
			auto *ptr = new(std::nothrow) T(std::forward<Args>(args)...);

			return OTEL_NULL(ptr) ? nullptr : std::unique_ptr<T>(ptr);
		}
		catch (...) {
			return nullptr;
		}
	}

	/***
	 * NAME
	 *   make_shared_nothrow - creates a std::shared_ptr with nothrow
	 *
	 * DESCRIPTION
	 *   Creates a std::shared_ptr using a non-throwing new expression,
	 *   returning a nullptr on allocation failure.  Unlike unique_ptr, the
	 *   std::shared_ptr constructor allocates a separate control block
	 *   which can throw std::bad_alloc.  To handle this, ptr is declared
	 *   before the try block so the catch clause can delete it.  If the
	 *   constructor of T throws, the new expression frees the raw memory
	 *   before propagating and ptr remains nullptr, so delete in the catch
	 *   clause is a safe no-op.
	 */
	template <class T, class... Args>
	std::shared_ptr<T> make_shared_nothrow(Args &&... args) noexcept
	{
		T *ptr = nullptr;

		try {
			ptr = new(std::nothrow) T(std::forward<Args>(args)...);

			return OTEL_NULL(ptr) ? nullptr : std::shared_ptr<T>(ptr);
		}
		catch (...) {
			delete ptr;

			return nullptr;
		}
	}

	/* For C-style strings. */
	template <typename T, typename std::enable_if<std::is_convertible<T, const char*>::value, int>::type = 0>
	std::string make_string_nothrow(T str) noexcept
	{
		if (OTEL_NULL(str))
			return {};

		try {
			return std::string(static_cast<const char*>(str));
		}
		catch (const std::exception &e) {
			OTELC_DBG(DEBUG, "make_string_nothrow exception: %s", e.what());

			return {};
		}
		catch (...) {
			OTELC_DBG(DEBUG, "make_string_nothrow unknown exception");

			return {};
		}
	}

	template <typename T, typename std::enable_if<std::is_same<typename std::decay<T>::type, std::string>::value, int>::type = 0>
	std::string make_string_nothrow(const T& str) noexcept
	{
		try {
			return std::string(str);
		}
		catch (const std::exception &e) {
			OTELC_DBG(DEBUG, "make_string_nothrow exception: %s", e.what());

			return {};
		}
		catch (...) {
			OTELC_DBG(DEBUG, "make_string_nothrow unknown exception");

			return {};
		}
	}
} /* namespace otel */

#endif /* _OPENTELEMETRY_C_WRAPPER_STD_H_ */

/*
 * Local variables:
 *  c-indent-level: 8
 *  c-basic-offset: 8
 * End:
 *
 * vi: noexpandtab shiftwidth=8 tabstop=8
 */
