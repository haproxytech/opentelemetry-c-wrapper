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
#ifndef _OPENTELEMETRY_C_WRAPPER_PROVIDER_H_
#define _OPENTELEMETRY_C_WRAPPER_PROVIDER_H_

#define OTEL_TRACER_PROVIDER()   OTEL_CAST_DYNAMIC(otel_sdk_trace::TracerProvider *, otel_trace::Provider::GetTracerProvider().get())


int  otel_tracer_provider_create(struct otelc_tracer *tracer, std::unique_ptr<otel_sdk_trace::SpanProcessor> &processor, std::unique_ptr<otel_sdk_trace::Sampler> &sampler, std::unique_ptr<otel_trace::TracerProvider> &provider);
int  otel_tracer_provider_get(struct otelc_tracer *tracer, otel_nostd::shared_ptr<otel_trace::TracerProvider> &provider);
void otel_tracer_provider_destroy(void);

#endif /* _OPENTELEMETRY_C_WRAPPER_PROVIDER_H_ */

/*
 * Local variables:
 *  c-indent-level: 8
 *  c-basic-offset: 8
 * End:
 *
 * vi: noexpandtab shiftwidth=8 tabstop=8
 */
