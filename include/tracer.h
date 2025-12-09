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
#ifndef _OPENTELEMETRY_C_WRAPPER_TRACER_H_
#define _OPENTELEMETRY_C_WRAPPER_TRACER_H_

#define OTEL_YAML_TRACER_PREFIX              "/signals/traces"
#define OTEL_YAML_SAMPLERS                   "/samplers"
#define OTEL_YAML_PROVIDERS                  "/providers"
#define OTEL_YAML_PROCESSORS                 "/processors"
#define OTEL_YAML_EXPORTERS                  "/exporters"

#define OTEL_TRACER_ERROR(f, ...)            do { (void)otelc_sprintf(&(tracer->err), f, ##__VA_ARGS__); OTELC_DBG(OTEL, "%s", tracer->err); } while (0)
#define OTEL_TRACER_ERETURN(f, ...)          do { OTEL_TRACER_ERROR(f, ##__VA_ARGS__); OTELC_RETURN(); } while (0)
#define OTEL_TRACER_ERETURN_EX(t,r,f, ...)   do { OTEL_TRACER_ERROR(f, ##__VA_ARGS__); OTELC_RETURN##t(r); } while (0)
#define OTEL_TRACER_ERETURN_INT(f, ...)      OTEL_TRACER_ERETURN_EX(_INT, OTELC_RET_ERROR, f, ##__VA_ARGS__)
#define OTEL_TRACER_ERETURN_PTR(f, ...)      OTEL_TRACER_ERETURN_EX(_PTR, nullptr, f, ##__VA_ARGS__)

#endif /* _OPENTELEMETRY_C_WRAPPER_TRACER_H_ */

/*
 * Local variables:
 *  c-indent-level: 8
 *  c-basic-offset: 8
 * End:
 *
 * vi: noexpandtab shiftwidth=8 tabstop=8
 */
