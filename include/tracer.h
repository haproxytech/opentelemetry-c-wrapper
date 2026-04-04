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

#define OTEL_TRACER_ERROR(f, ...)            OTEL_SIGNAL_ERROR(tracer->err, f, ##__VA_ARGS__)
#define OTEL_TRACER_RETURN(f, ...)           OTEL_RETURN(tracer, f, ##__VA_ARGS__)
#define OTEL_TRACER_RETURN_EX(t,r,f, ...)    OTEL_RETURN_EX(tracer, t, (r), f, ##__VA_ARGS__)
#define OTEL_TRACER_RETURN_INT(f, ...)       OTEL_RETURN_INT(tracer, f, ##__VA_ARGS__)
#define OTEL_TRACER_RETURN_PTR(f, ...)       OTEL_RETURN_PTR(tracer, f, ##__VA_ARGS__)

#endif /* _OPENTELEMETRY_C_WRAPPER_TRACER_H_ */

/*
 * Local variables:
 *  c-indent-level: 8
 *  c-basic-offset: 8
 * End:
 *
 * vi: noexpandtab shiftwidth=8 tabstop=8
 */
