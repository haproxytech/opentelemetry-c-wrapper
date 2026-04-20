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
#ifndef _OPENTELEMETRY_C_WRAPPER_THREADS_H_
#define _OPENTELEMETRY_C_WRAPPER_THREADS_H_

/* NOTE: The offset from the demangled name was determined empirically. */
#define OTEL_THREAD_INSTRUMENTATION_DEMANGLED_NAME   (typeid(otel_thread_instrumentation).name() + 2)
#define OTEL_DBG_THREAD_INSTRUMENTATION()            OTELC_DBG(DEBUG, "thread: '%s' cpu_id: %d", thread_name.c_str(), cpu_id)
#define OTEL_THREAD_GET_ID(s)                        OTELC_DBG_IFDEF(std::ostringstream s; s << std::this_thread::get_id(), )

/* Maximum CPU ID for thread affinity.  The OTEL_MAX_CPU_IDL and
 * OTEL_MAX_CPU_IDLL macros guard against INT64_C() token-pasting:
 * INT64_C(OTEL_MAX_CPU_ID) may expand to OTEL_MAX_CPU_IDL on LP64
 * platforms (L suffix) or OTEL_MAX_CPU_IDLL on ILP32/LLP64
 * platforms (LL suffix). */
constexpr int64_t OTEL_MAX_CPU_ID = 4095;
#define OTEL_MAX_CPU_IDL                             OTEL_MAX_CPU_ID
#define OTEL_MAX_CPU_IDLL                            OTEL_MAX_CPU_ID

constexpr size_t OTEL_THREAD_NAME_SIZE = 16; /* Including the terminating null character. */

class otel_thread_instrumentation final : public otel_sdk_common::ThreadInstrumentation {
public:
	otel_thread_instrumentation();

	otel_thread_instrumentation(const std::string &thread_name_, int cpu_id_ = -1)
		: thread_name(thread_name_), cpu_id(cpu_id_)
	{
		OTELC_FUNCPP("", OTEL_THREAD_INSTRUMENTATION_DEMANGLED_NAME);

		OTEL_DBG_THREAD_INSTRUMENTATION();

		OTELC_RETURN();
	}

	~otel_thread_instrumentation() override;

#ifdef ENABLE_THREAD_INSTRUMENTATION_PREVIEW
	void OnStart() override;
	void OnEnd() override;
	void BeforeWait() override;
	void AfterWait() override;
	void BeforeLoad() override;
	void AfterLoad() override;
#endif

private:
	std::string thread_name;
	int         cpu_id = -1;
};

#endif /* _OPENTELEMETRY_C_WRAPPER_THREADS_H_ */

/*
 * Local variables:
 *  c-indent-level: 8
 *  c-basic-offset: 8
 * End:
 *
 * vi: noexpandtab shiftwidth=8 tabstop=8
 */
