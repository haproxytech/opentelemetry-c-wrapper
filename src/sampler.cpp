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
 *   otel_sampler_create - creates a new sampler instance
 *
 * SYNOPSIS
 *   int otel_sampler_create(struct otelc_tracer *tracer, std::unique_ptr<otel_sdk_trace::Sampler> &sampler)
 *
 * ARGUMENTS
 *   tracer  - tracer instance
 *   sampler - unique pointer to store the created sampler
 *
 * DESCRIPTION
 *   Creates a new sampler based on the configuration provided in the YAML file.
 *   The sampler determines which traces are recorded and sent to the backend,
 *   which is a key mechanism for controlling the volume of telemetry data.
 *
 * RETURN VALUE
 *   Returns OTELC_RET_OK on success, or OTELC_RET_ERROR in case of an error.
 */
int otel_sampler_create(struct otelc_tracer *tracer, std::unique_ptr<otel_sdk_trace::Sampler> &sampler)
{
	std::unique_ptr<otel_sdk_trace::Sampler> sampler_maybe;
	char                                     type[OTEL_YAML_BUFSIZ] = "trace_id_ratio_based";
	double                                   ratio = 1.0;
	int                                      rc;

	OTELC_FUNC("%p, <sampler>", tracer);

	if (OTEL_NULL(tracer))
		OTELC_RETURN_INT(OTELC_RET_ERROR);

	rc = yaml_get_node(tracer->ctx->fyd, &(tracer->err), 0, "OpenTelemetry sampler", OTEL_YAML_TRACER_PREFIX OTEL_YAML_SAMPLERS, nullptr,
	                   OTEL_YAML_ARG_STR(1, SAMPLERS, type),
	                   OTEL_YAML_END);
	if (rc == OTELC_RET_ERROR)
		OTELC_RETURN_INT(OTELC_RET_ERROR);

	if (strcasecmp(type, "always_on") == 0) {
		sampler_maybe = otel::make_unique_nothrow<otel_sdk_trace::AlwaysOnSampler>();
	}
	else if (strcasecmp(type, "always_off") == 0) {
		sampler_maybe = otel::make_unique_nothrow<otel_sdk_trace::AlwaysOffSampler>();
	}
	else if (strcasecmp(type, "trace_id_ratio_based") == 0) {
		rc = yaml_get_node(tracer->ctx->fyd, &(tracer->err), 0, "OpenTelemetry sampler", OTEL_YAML_TRACER_PREFIX OTEL_YAML_SAMPLERS, nullptr,
		                   OTEL_YAML_ARG_DOUBLE(1, SAMPLERS, ratio, 0.0, 1.0),
		                   OTEL_YAML_END);
		if (rc == OTELC_RET_ERROR)
			OTELC_RETURN_INT(OTELC_RET_ERROR);

		sampler_maybe = otel::make_unique_nothrow<otel_sdk_trace::TraceIdRatioBasedSampler>(ratio);

#ifdef DEBUG
		/***
		 * From function trace_id_ratio.cc:111:TraceIdRatioBasedSampler().
		 */
		if (!OTEL_NULL(sampler_maybe)) {
			const std::string str_ratio{"TraceIdRatioBasedSampler{" + std::to_string(ratio) + "}"};

			if (str_ratio != sampler_maybe->GetDescription())
				OTEL_TRACER_RETURN_INT("Invalid OpenTelemetry sampler ratio: %f", ratio);
		}
#endif /* DEBUG */
	}
	else if (strcasecmp(type, "parent_based") == 0) {
		/***
		 * Build optional per-state delegate samplers.  When a YAML
		 * field is empty the SDK-default sampler is used.  Null
		 * shared_ptrs must not be passed to the ParentBasedSampler
		 * constructor because they override the built-in defaults.
		 */
		std::unique_ptr<otel_sdk_trace::Sampler> delegate_sampler;
		std::shared_ptr<otel_sdk_trace::Sampler> remote_sampled_sampler     = std::make_shared<otel_sdk_trace::AlwaysOnSampler>();
		std::shared_ptr<otel_sdk_trace::Sampler> remote_not_sampled_sampler = std::make_shared<otel_sdk_trace::AlwaysOffSampler>();
		std::shared_ptr<otel_sdk_trace::Sampler> local_sampled_sampler      = std::make_shared<otel_sdk_trace::AlwaysOnSampler>();
		std::shared_ptr<otel_sdk_trace::Sampler> local_not_sampled_sampler  = std::make_shared<otel_sdk_trace::AlwaysOffSampler>();
		char                                     delegate[OTEL_YAML_BUFSIZ] = "always_on";
		char                                     remote_sampled[OTEL_YAML_BUFSIZ] = "", remote_not_sampled[OTEL_YAML_BUFSIZ] = "";
		char                                     local_sampled[OTEL_YAML_BUFSIZ] = "", local_not_sampled[OTEL_YAML_BUFSIZ] = "";

		rc = yaml_get_node(tracer->ctx->fyd, &(tracer->err), 0, "OpenTelemetry parent_based sampler", OTEL_YAML_TRACER_PREFIX OTEL_YAML_SAMPLERS, nullptr,
		                   OTEL_YAML_ARG_STR(0, SAMPLERS, delegate),
		                   OTEL_YAML_ARG_STR(0, SAMPLERS, remote_sampled),
		                   OTEL_YAML_ARG_STR(0, SAMPLERS, remote_not_sampled),
		                   OTEL_YAML_ARG_STR(0, SAMPLERS, local_sampled),
		                   OTEL_YAML_ARG_STR(0, SAMPLERS, local_not_sampled),
		                   OTEL_YAML_END);
		if (rc == OTELC_RET_ERROR)
			OTELC_RETURN_INT(OTELC_RET_ERROR);

		if (strcasecmp(delegate, "always_on") == 0) {
			delegate_sampler = otel::make_unique_nothrow<otel_sdk_trace::AlwaysOnSampler>();
		}
		else if (strcasecmp(delegate, "always_off") == 0) {
			delegate_sampler = otel::make_unique_nothrow<otel_sdk_trace::AlwaysOffSampler>();
		}
		else if (strcasecmp(delegate, "trace_id_ratio_based") == 0) {
			rc = yaml_get_node(tracer->ctx->fyd, &(tracer->err), 0, "OpenTelemetry parent_based sampler ratio", OTEL_YAML_TRACER_PREFIX OTEL_YAML_SAMPLERS, nullptr,
			                   OTEL_YAML_ARG_DOUBLE(1, SAMPLERS, ratio, 0.0, 1.0),
			                   OTEL_YAML_END);
			if (rc == OTELC_RET_ERROR)
				OTELC_RETURN_INT(OTELC_RET_ERROR);

			delegate_sampler = otel::make_unique_nothrow<otel_sdk_trace::TraceIdRatioBasedSampler>(ratio);
		}
		else {
			OTEL_TRACER_RETURN_INT("Invalid parent_based sampler delegate type: '%s'", delegate);
		}

		if (OTEL_NULL(delegate_sampler))
			OTEL_TRACER_RETURN_INT("Unable to create delegate sampler for ParentBasedSampler");

		if (OTELC_STR_IS_VALID(remote_sampled)) {
			if (strcasecmp(remote_sampled, "always_on") == 0)
				remote_sampled_sampler = std::make_shared<otel_sdk_trace::AlwaysOnSampler>();
			else if (strcasecmp(remote_sampled, "always_off") == 0)
				remote_sampled_sampler = std::make_shared<otel_sdk_trace::AlwaysOffSampler>();
			else
				OTEL_TRACER_RETURN_INT("Invalid remote_sampled delegate: '%s'", remote_sampled);
		}
		if (OTELC_STR_IS_VALID(remote_not_sampled)) {
			if (strcasecmp(remote_not_sampled, "always_on") == 0)
				remote_not_sampled_sampler = std::make_shared<otel_sdk_trace::AlwaysOnSampler>();
			else if (strcasecmp(remote_not_sampled, "always_off") == 0)
				remote_not_sampled_sampler = std::make_shared<otel_sdk_trace::AlwaysOffSampler>();
			else
				OTEL_TRACER_RETURN_INT("Invalid remote_not_sampled delegate: '%s'", remote_not_sampled);
		}
		if (OTELC_STR_IS_VALID(local_sampled)) {
			if (strcasecmp(local_sampled, "always_on") == 0)
				local_sampled_sampler = std::make_shared<otel_sdk_trace::AlwaysOnSampler>();
			else if (strcasecmp(local_sampled, "always_off") == 0)
				local_sampled_sampler = std::make_shared<otel_sdk_trace::AlwaysOffSampler>();
			else
				OTEL_TRACER_RETURN_INT("Invalid local_sampled delegate: '%s'", local_sampled);
		}
		if (OTELC_STR_IS_VALID(local_not_sampled)) {
			if (strcasecmp(local_not_sampled, "always_on") == 0)
				local_not_sampled_sampler = std::make_shared<otel_sdk_trace::AlwaysOnSampler>();
			else if (strcasecmp(local_not_sampled, "always_off") == 0)
				local_not_sampled_sampler = std::make_shared<otel_sdk_trace::AlwaysOffSampler>();
			else
				OTEL_TRACER_RETURN_INT("Invalid local_not_sampled delegate: '%s'", local_not_sampled);
		}

		sampler_maybe = otel::make_unique_nothrow<otel_sdk_trace::ParentBasedSampler>(std::move(delegate_sampler), remote_sampled_sampler, remote_not_sampled_sampler, local_sampled_sampler, local_not_sampled_sampler);
	}
	else {
		OTEL_TRACER_RETURN_INT("Invalid sampler type: '%s'", type);
	}

	if (OTEL_NULL(sampler_maybe))
		OTEL_TRACER_RETURN_INT("Unable to create OpenTelemetry sampler");

	sampler = std::move(sampler_maybe);

	OTELC_RETURN_INT(OTELC_RET_OK);
}

/*
 * Local variables:
 *  c-indent-level: 8
 *  c-basic-offset: 8
 * End:
 *
 * vi: noexpandtab shiftwidth=8 tabstop=8
 */
