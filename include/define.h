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
#ifndef _OPENTELEMETRY_C_WRAPPER_DEFINE_H_
#define _OPENTELEMETRY_C_WRAPPER_DEFINE_H_

namespace otel_baggage                = OPENTELEMETRY_NAMESPACE::baggage;
namespace otel_common                 = OPENTELEMETRY_NAMESPACE::common;
namespace otel_context                = OPENTELEMETRY_NAMESPACE::context;
namespace otel_exporter               = OPENTELEMETRY_NAMESPACE::exporter;
namespace otel_nostd                  = OPENTELEMETRY_NAMESPACE::nostd;
namespace otel_trace                  = OPENTELEMETRY_NAMESPACE::trace;
namespace otel_sdk_common             = OPENTELEMETRY_NAMESPACE::sdk::common;
namespace otel_sdk_trace              = OPENTELEMETRY_NAMESPACE::sdk::trace;
namespace otel_sdk_resource           = OPENTELEMETRY_NAMESPACE::sdk::resource;
namespace otel_sdk_internal_log       = otel_sdk_common::internal_log;
#ifdef HAVE_OTEL_EXPORTER_IN_MEMORY
namespace otel_exporter_memory        = otel_exporter::memory;
#endif
#ifdef HAVE_OTEL_EXPORTER_OSTREAM
namespace otel_exporter_trace         = otel_exporter::trace;
namespace otel_exporter_metrics       = otel_exporter::metrics;
#endif
#if defined(HAVE_OTEL_EXPORTER_OTLP_FILE) || defined(HAVE_OTEL_EXPORTER_OTLP_GRPC) || defined(HAVE_OTEL_EXPORTER_OTLP_HTTP)
namespace otel_exporter_otlp          = otel_exporter::otlp;
#endif
#ifdef HAVE_OTEL_EXPORTER_ZIPKIN
namespace otel_exporter_zipkin        = otel_exporter::zipkin;
#endif

namespace otel_metrics                = OPENTELEMETRY_NAMESPACE::metrics;
namespace otel_sdk_metrics            = OPENTELEMETRY_NAMESPACE::sdk::metrics;

using otel_system_timestamp           = otel_common::SystemTimestamp;
using otel_steady_timestamp           = otel_common::SteadyTimestamp;
using otel_attribute_value            = otel_common::AttributeValue;
using otel_attributes                 = std::vector<std::pair<otel_nostd::string_view, otel_attribute_value>>;

template <typename T> void otel_print_type();
#define OTEL_ERR_PRINT_TYPE(t)        otel_print_type<decltype(t)>()
#define OTEL_DBG_PRINT_TYPE(t)        OTELC_DBG(DEBUG, "typeof " #t ": '%s'", typeid(decltype(t)).name())
#define OTEL_DBG_THROW()              OTELC_DBG_IFDEF(if (otelc_dbg_trigger_throw && !(random() % 100)) throw std::runtime_error(">>> Debug-only thrown exception"), )

template <typename T> struct otel_defer_struct { T fn; ~otel_defer_struct() { fn(); } };
template <typename T> otel_defer_struct<T>make_defer(T fn) { return { fn }; }
#define OTEL_DEFER(expr)              auto OTELC_DEFER_NAME(arg_defer, __LINE__) = ::make_defer([&] { expr; })
#define OTEL_VA_AUTO(a,t)             va_start((a), (t)); OTEL_DEFER(va_end(a));

#define OTEL_NULL(p)                  ((p) == nullptr)
#define OTEL_ARG_DEFAULT(p,v)         do { if (OTEL_NULL(p)) (p) = (v); } while (0)

#define OTEL_EXT_MALLOC(s)            otelc_ext_malloc(OTELC_DBG_ARGS (s))
#define OTEL_EXT_FREE_CLEAR(p)        do { if (!OTEL_NULL(p)) { otelc_ext_free(OTELC_DBG_ARGS (p)); (p) = nullptr; } } while (0)

/***
 * Automatically frees a C-style resource n of type t using the function f when
 * it goes out of scope.  Internally, it creates a std::unique_ptr with a custom
 * deleter, mimicking Go-style defer in C++.  This ensures exception-safe and
 * early-return-safe cleanup without changing the original variable.
 */
#define OTEL_DEFER_FREE(t,v,f)        auto guard_##v = std::unique_ptr<t, void(*)(t *)>((v), [](t *ptr) { f(ptr); })
#define OTEL_DEFER_DPTR_FREE(t,v,f)   auto guard_##v = std::unique_ptr<t *, void(*)(t **)>(&(v), [](t **ptr) { f(ptr); })

#define OTEL_CAST_CONST(t,e)          const_cast<t>(e)
#define OTEL_CAST_STATIC(t,e)         static_cast<t>(e)
#define OTEL_CAST_STATIC_PTR(t,e)     std::static_pointer_cast<t>(e)
#define OTEL_CAST_DYNAMIC(t,e)        dynamic_cast<t>(e)
#define OTEL_CAST_REINTERPRET(t,e)    reinterpret_cast<t>(e)
#define OTEL_CAST_TYPEOF(t,e)         OTEL_CAST_REINTERPRET(typeof(t), (e))

#define OTEL_ERROR_MSG_ENOMEM(s)      "Unable to allocate memory for " s

#define OTEL_ERROR(f, ...)            do { if (otelc_sprintf(err, f, ##__VA_ARGS__) > 0) OTELC_DBG(OTEL, "%s", *err); } while (0)
#define OTEL_ERETURN(f, ...)          do { OTEL_ERROR(f, ##__VA_ARGS__); OTELC_RETURN(); } while (0)
#define OTEL_ERETURN_EX(t,r,f, ...)   do { OTEL_ERROR(f, ##__VA_ARGS__); OTELC_RETURN##t(r); } while (0)
#define OTEL_ERETURN_INT(f, ...)      OTEL_ERETURN_EX(_INT, OTELC_RET_ERROR, f, ##__VA_ARGS__)
#define OTEL_ERETURN_PTR(f, ...)      OTEL_ERETURN_EX(_PTR, nullptr, f, ##__VA_ARGS__)

#define OTEL_CATCH_ERETURN(arg_func, arg_return, arg_fmt, ...)       \
	catch (const std::exception &e) {                            \
		arg_func;                                            \
		arg_return(arg_fmt ": %s", ##__VA_ARGS__, e.what()); \
	}                                                            \
	catch (...) {                                                \
		arg_func;                                            \
		arg_return(arg_fmt, ##__VA_ARGS__);                  \
	}
#define OTEL_CATCH_RETURN(arg_return, arg_retval, arg_fmt, ...)           \
	catch (const std::exception &e) {                                 \
		OTELC_DBG(OTEL, arg_fmt ": %s", ##__VA_ARGS__, e.what()); \
		arg_return(arg_retval);                                   \
	}                                                                 \
	catch (...) {                                                     \
		OTELC_DBG(OTEL, arg_fmt, ##__VA_ARGS__);                  \
		arg_return(arg_retval);                                   \
	}


static constexpr char otel_nibble_to_hex(int a) { return OTEL_CAST_STATIC(char, a + ((a < 10) ? '0' : ('a' - 10))); }

#endif /* _OPENTELEMETRY_C_WRAPPER_DEFINE_H_ */

/*
 * Local variables:
 *  c-indent-level: 8
 *  c-basic-offset: 8
 * End:
 *
 * vi: noexpandtab shiftwidth=8 tabstop=8
 */
