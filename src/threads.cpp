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


/***
 * NAME
 *   otel_thread_instrumentation::otel_thread_instrumentation - constructs a default thread instrumentation instance
 *
 * SYNOPSIS
 *   otel_thread_instrumentation::otel_thread_instrumentation()
 *
 * ARGUMENTS
 *   This function takes no arguments.
 *
 * DESCRIPTION
 *   Constructs a default thread instrumentation instance with an empty thread
 *   name.
 *
 * RETURN VALUE
 *   This is a constructor and does not return a value.
 */
otel_thread_instrumentation::otel_thread_instrumentation()
{
	OTELC_FUNCPP("", OTEL_THREAD_INSTRUMENTATION_DEMANGLED_NAME);

	OTEL_DBG_THREAD_INSTRUMENTATION();

	OTELC_RETURN();
}


/***
 * NAME
 *   otel_thread_instrumentation::~otel_thread_instrumentation - destroys the thread instrumentation instance
 *
 * SYNOPSIS
 *   otel_thread_instrumentation::~otel_thread_instrumentation()
 *
 * ARGUMENTS
 *   This function takes no arguments.
 *
 * DESCRIPTION
 *   Destroys the thread instrumentation instance and releases any associated
 *   resources.
 *
 * RETURN VALUE
 *   This is a destructor and does not return a value.
 */
otel_thread_instrumentation::~otel_thread_instrumentation()
{
	OTELC_FUNCPP("", OTEL_THREAD_INSTRUMENTATION_DEMANGLED_NAME);

	OTEL_DBG_THREAD_INSTRUMENTATION();

	OTELC_RETURN();
}


#ifdef ENABLE_THREAD_INSTRUMENTATION_PREVIEW

/***
 * NAME
 *   otel_thread_instrumentation::OnStart - handles thread start notification
 *
 * SYNOPSIS
 *   void otel_thread_instrumentation::OnStart(void)
 *
 * ARGUMENTS
 *   This function takes no arguments.
 *
 * DESCRIPTION
 *   Called when a managed thread starts execution.  If a thread name
 *   has been configured, sets the operating system thread name using
 *   pthread_setname_np(), truncating the name to fit the platform limit
 *   of OTEL_THREAD_NAME_SIZE characters.  If a CPU identifier has been
 *   configured (cpu_id >= 0), bounds the thread to that CPU core using
 *   pthread_setaffinity_np().
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
void otel_thread_instrumentation::OnStart(void)
{
	OTELC_FUNCPP("", OTEL_THREAD_INSTRUMENTATION_DEMANGLED_NAME);

	OTEL_DBG_THREAD_INSTRUMENTATION();

	if (!thread_name.empty())
		(void)pthread_setname_np(pthread_self(), thread_name.substr(0, OTEL_THREAD_NAME_SIZE - 1).c_str());

#ifdef HAVE_SCHED_H
	/* If binding the thread to a CPU fails, only the error is logged. */
	if (cpu_id >= 0) {
		cpu_set_t cpuset;
		int       rc;

		CPU_ZERO(&cpuset);
		CPU_SET(cpu_id, &cpuset);
		rc = pthread_setaffinity_np(pthread_self(), sizeof(cpuset), &cpuset);
		if (rc != 0)
			OTELC_DBG(DEBUG, "Failed to bound thread to cpu %d: %s", cpu_id, otel_strerror(rc));
	}
#endif

	OTELC_RETURN();
}


/***
 * NAME
 *   otel_thread_instrumentation::OnEnd - handles thread end notification
 *
 * SYNOPSIS
 *   void otel_thread_instrumentation::OnEnd(void)
 *
 * ARGUMENTS
 *   This function takes no arguments.
 *
 * DESCRIPTION
 *   Called when a managed thread finishes execution.  This callback is reserved
 *   for future use and currently performs no operation.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
void otel_thread_instrumentation::OnEnd(void)
{
	OTELC_FUNCPP("", OTEL_THREAD_INSTRUMENTATION_DEMANGLED_NAME);

	OTEL_DBG_THREAD_INSTRUMENTATION();

	OTELC_RETURN();
}


/***
 * NAME
 *   otel_thread_instrumentation::BeforeWait - handles pre-wait notification
 *
 * SYNOPSIS
 *   void otel_thread_instrumentation::BeforeWait(void)
 *
 * ARGUMENTS
 *   This function takes no arguments.
 *
 * DESCRIPTION
 *   Called before a managed thread enters a wait state.  This callback is
 *   reserved for future use and currently performs no operation.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
void otel_thread_instrumentation::BeforeWait(void)
{
	OTELC_FUNCPP("", OTEL_THREAD_INSTRUMENTATION_DEMANGLED_NAME);

	OTEL_DBG_THREAD_INSTRUMENTATION();

	OTELC_RETURN();
}


/***
 * NAME
 *   otel_thread_instrumentation::AfterWait - handles post-wait notification
 *
 * SYNOPSIS
 *   void otel_thread_instrumentation::AfterWait(void)
 *
 * ARGUMENTS
 *   This function takes no arguments.
 *
 * DESCRIPTION
 *   Called after a managed thread exits a wait state.  This callback is
 *   reserved for future use and currently performs no operation.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
void otel_thread_instrumentation::AfterWait(void)
{
	OTELC_FUNCPP("", OTEL_THREAD_INSTRUMENTATION_DEMANGLED_NAME);

	OTEL_DBG_THREAD_INSTRUMENTATION();

	OTELC_RETURN();
}


/***
 * NAME
 *   otel_thread_instrumentation::BeforeLoad - handles pre-load notification
 *
 * SYNOPSIS
 *   void otel_thread_instrumentation::BeforeLoad(void)
 *
 * ARGUMENTS
 *   This function takes no arguments.
 *
 * DESCRIPTION
 *   Called before a managed thread performs a load operation.  This callback
 *   is reserved for future use and currently performs no operation.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
void otel_thread_instrumentation::BeforeLoad(void)
{
	OTELC_FUNCPP("", OTEL_THREAD_INSTRUMENTATION_DEMANGLED_NAME);

	OTEL_DBG_THREAD_INSTRUMENTATION();

	OTELC_RETURN();
}


/***
 * NAME
 *   otel_thread_instrumentation::AfterLoad - handles post-load notification
 *
 * SYNOPSIS
 *   void otel_thread_instrumentation::AfterLoad(void)
 *
 * ARGUMENTS
 *   This function takes no arguments.
 *
 * DESCRIPTION
 *   Called after a managed thread completes a load operation.  This callback
 *   is reserved for future use and currently performs no operation.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
void otel_thread_instrumentation::AfterLoad(void)
{
	OTELC_FUNCPP("", OTEL_THREAD_INSTRUMENTATION_DEMANGLED_NAME);

	OTEL_DBG_THREAD_INSTRUMENTATION();

	OTELC_RETURN();
}

#endif /* ENABLE_THREAD_INSTRUMENTATION_PREVIEW */

/*
 * Local variables:
 *  c-indent-level: 8
 *  c-basic-offset: 8
 * End:
 *
 * vi: noexpandtab shiftwidth=8 tabstop=8
 */
