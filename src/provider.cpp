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
 *   otel_tracer_provider_create - creates a new tracer provider
 *
 * SYNOPSIS
 *   int otel_tracer_provider_create(struct otelc_tracer *tracer, std::unique_ptr<otel_sdk_trace::SpanProcessor> &processor, std::unique_ptr<otel_sdk_trace::Sampler> &sampler, std::unique_ptr<otel_trace::TracerProvider> &provider)
 *
 * ARGUMENTS
 *   tracer    - tracer instance
 *   processor - the span processor to be used by the provider
 *   sampler   - the sampler to be used by the provider
 *   provider  - unique pointer to store the created tracer provider
 *
 * DESCRIPTION
 *   Creates a new tracer provider with the specified span processor and sampler.
 *   The provider is responsible for creating and managing tracer instances, and
 *   it is configured with resource attributes detected from the environment and
 *   the YAML configuration file.
 *
 * RETURN VALUE
 *   Returns OTELC_RET_OK on success, or OTELC_RET_ERROR in case of an error.
 */
int otel_tracer_provider_create(struct otelc_tracer *tracer, std::unique_ptr<otel_sdk_trace::SpanProcessor> &processor, std::unique_ptr<otel_sdk_trace::Sampler> &sampler, std::unique_ptr<otel_trace::TracerProvider> &provider)
{
	otel_sdk_resource::Resource resource{};

	OTELC_FUNC("%p, <processor>, <sampler>, <provider>", tracer);

	if (OTEL_NULL(tracer))
		OTELC_RETURN_INT(OTELC_RET_ERROR);

	if (otel_resource_create("OpenTelemetry tracer provider", OTEL_YAML_TRACER_PREFIX OTEL_YAML_PROVIDERS, resource, &(tracer->err)) == OTELC_RET_ERROR)
		OTELC_RETURN_INT(OTELC_RET_ERROR);

#ifdef OTELC_USE_MULTIPLE_PROCESSORS
	std::vector<std::unique_ptr<otel_sdk_trace::SpanProcessor>> processors;
	try {
		OTEL_DBG_THROW();
		processors.push_back(std::move(processor));
	}
	OTEL_CATCH_ERETURN( , OTEL_TRACER_ERETURN_INT, "Unable to add processor")

	auto context_maybe = otel::make_unique_nothrow<otel_sdk_trace::TracerContext>(std::move(processors), std::move(resource), std::move(sampler));
	if (OTEL_NULL(context_maybe))
		OTEL_TRACER_ERETURN_INT("Unable to create OpenTelemetry tracer context");
	auto provider_maybe = otel_nostd::unique_ptr<otel_trace::TracerProvider>(otel::make_unique_nothrow<otel_sdk_trace::TracerProvider>(std::move(context_maybe)).release());
#else
	auto provider_maybe = otel_nostd::unique_ptr<otel_trace::TracerProvider>(otel::make_unique_nothrow<otel_sdk_trace::TracerProvider>(std::move(processor), std::move(resource), std::move(sampler)).release());
#endif /* OTELC_USE_MULTIPLE_PROCESSORS */

	if (OTEL_NULL(provider_maybe))
		OTEL_TRACER_ERETURN_INT("Unable to create OpenTelemetry tracer provider");

	provider = std::move(provider_maybe);

	OTELC_RETURN_INT(OTELC_RET_OK);
}


/***
 * NAME
 *   otel_tracer_provider_get - gets the global tracer provider
 *
 * SYNOPSIS
 *   int otel_tracer_provider_get(struct otelc_tracer *tracer, otel_nostd::shared_ptr<otel_trace::TracerProvider> &provider)
 *
 * ARGUMENTS
 *   tracer   - tracer instance
 *   provider - shared pointer to store the global tracer provider
 *
 * DESCRIPTION
 *   Retrieves the currently configured global tracer provider.  This allows
 *   different parts of an application to access the same tracer provider
 *   instance without having to pass it around explicitly.
 *
 * RETURN VALUE
 *   Returns OTELC_RET_OK on success, or OTELC_RET_ERROR in case of an error.
 */
int otel_tracer_provider_get(struct otelc_tracer *tracer, otel_nostd::shared_ptr<otel_trace::TracerProvider> &provider)
{
	OTELC_FUNC("%p, <provider>", tracer);

	if (OTEL_NULL(tracer))
		OTELC_RETURN_INT(OTELC_RET_ERROR);

	auto provider_maybe = otel_trace::Provider::GetTracerProvider();
	if (OTEL_NULL(provider_maybe))
		OTEL_TRACER_ERETURN_INT("Unable to get OpenTelemetry tracer provider");

	provider = std::move(provider_maybe);

	OTELC_RETURN_INT(OTELC_RET_OK);
}


/***
 * NAME
 *   otel_tracer_provider_destroy - destroys the global tracer provider
 *
 * SYNOPSIS
 *   void otel_tracer_provider_destroy(void)
 *
 * ARGUMENTS
 *   This function takes no arguments.
 *
 * DESCRIPTION
 *   Resets the global tracer provider, effectively disabling tracing.  This
 *   should be called during application shutdown to ensure that all tracing
 *   resources are properly released.
 *
 * RETURN VALUE
 *   This function does not return a value.
 */
void otel_tracer_provider_destroy(void)
{
	const std::shared_ptr<otel_trace::TracerProvider> none;

	OTELC_FUNC("");

	const auto provider_sdk = OTEL_TRACER_PROVIDER();
	if (!OTEL_NULL(provider_sdk))
		(void)provider_sdk->ForceFlush(std::chrono::microseconds{5000000});

	otel_trace::Provider::SetTracerProvider(none);

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
