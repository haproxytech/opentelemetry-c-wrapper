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
#ifndef _OPENTELEMETRY_C_WRAPPER_LOGGER_H_
#define _OPENTELEMETRY_C_WRAPPER_LOGGER_H_

#define OTEL_YAML_LOGGER_PREFIX             "/signals/logs"

#define OTEL_LOGGER_ERROR(f, ...)            OTEL_SIGNAL_ERROR(logger->err, f, ##__VA_ARGS__)
#define OTEL_LOGGER_RETURN(f, ...)           OTEL_RETURN(logger, f, ##__VA_ARGS__)
#define OTEL_LOGGER_RETURN_EX(t,r,f, ...)    OTEL_RETURN_EX(logger, t, (r), f, ##__VA_ARGS__)
#define OTEL_LOGGER_RETURN_INT(f, ...)       OTEL_RETURN_INT(logger, f, ##__VA_ARGS__)
#define OTEL_LOGGER_RETURN_PTR(f, ...)       OTEL_RETURN_PTR(logger, f, ##__VA_ARGS__)

/***
 * Per-instance implementation state for a logger.  Holds the SDK LoggerProvider
 * and the SDK Logger obtained from it.  Each shared_ptr is owned by the
 * instance, so multiple loggers can coexist without sharing process-wide state.
 */
struct otel_logger_impl {
	otel_nostd::shared_ptr<otel_logs::LoggerProvider> provider;
	otel_nostd::shared_ptr<otel_logs::Logger>         logger;
};

#endif /* _OPENTELEMETRY_C_WRAPPER_LOGGER_H_ */

/*
 * Local variables:
 *  c-indent-level: 8
 *  c-basic-offset: 8
 * End:
 *
 * vi: noexpandtab shiftwidth=8 tabstop=8
 */
