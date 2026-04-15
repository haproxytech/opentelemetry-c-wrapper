# opentelemetry-c-wrapper

A C wrapper for the C++ OpenTelemetry API, enabling C integration with the
OpenTelemetry observability framework.

## Overview

The OpenTelemetry (OTel) C wrapper library provides a pure C API on top of the
official
[OpenTelemetry C++ client](https://github.com/open-telemetry/opentelemetry-cpp).
It was developed by [HAProxy Technologies](https://www.haproxy.com/) for use in
the HAProxy OTel filter, but is suitable for any C application that needs to
export telemetry data.

The library supports three OTel signals:

- **Traces** -- span creation, context propagation, baggage, sampling, events,
  links, and exceptions.
- **Metrics** -- synchronous and observable instruments (counters, histograms,
  gauges, up-down counters) with views.
- **Logs** -- structured log records with severity filtering and span
  correlation.

All SDK components are configured through a single YAML file.

## Build Instructions

### Prerequisites

Install required system packages using the provided script (run as root):

```sh
cd scripts/build
./linux-update.sh
```

Tested Linux distributions (amd64): Debian 11/12/13, Ubuntu 20.04/22.04/24.04/25.10,
Tuxedo 24.04, RHEL 8.10/9.5/10.0, Rocky 9.5, and openSUSE Leap 15.5/15.6.

### Building the OTel C++ SDK and Dependencies

Use the bundled build script to compile and install all prerequisite libraries
into `/opt` (or another prefix of your choice):

```sh
cd scripts/build
./build-bundle.sh [prefix-dir]
```

By default, libraries are installed under `/opt`.  A sequential alternative
(`build.sh`) is also available.

**Important:** Use the provided build scripts rather than relying on
system-installed dependency packages, which are likely outdated or compiled
with options incompatible with the OTel C wrapper.

If none of the attached `build-*.sh` scripts is used, the patches in
`scripts/build/` must be applied to the OpenTelemetry C++ source tree
before compilation and the same CMake configuration options found in
`scripts/build/opentelemetry-cpp-1.26.0-install.sh` must be used.

### Building the Wrapper Library

**Autotools** (recommended):

```sh
./scripts/bootstrap
./configure --prefix=/opt --with-opentelemetry=/opt
make -j8
make install
```

To build the debug variant (enables detailed internal logging):

```sh
./scripts/distclean
./scripts/bootstrap
./configure --prefix=/opt --with-opentelemetry=/opt --enable-debug
make -j8
make install
```

**CMake** (alternative):

```sh
mkdir build && cd build
cmake -DCMAKE_INSTALL_PREFIX=/opt -DOPENTELEMETRY_DIR=/opt ..
make -j8
make install
```

Add `-DENABLE_DEBUG=ON` for the debug variant.

### YAML Parser Selection

The library requires a YAML parser.  By default it uses
[rapidyaml](https://github.com/biojppm/rapidyaml), which is already built as
part of the OTel C++ SDK dependencies.  Alternatively,
[libfyaml](https://github.com/pantoniou/libfyaml) can be selected with
`--with-libfyaml` (autotools) or `-DWITH_LIBFYAML=ON` (CMake).  Both parsers
cannot be used at the same time.

## Testing

Compile and run the test suite:

```sh
make test
cd test
./otel-c-wrapper-test --help
./otel-c-wrapper-test --runcount=10 --threads=8
```

The test program uses `otel-cfg.yml` as its library configuration file.

Signal-specific test programs (`test-tracer`, `test-meter`, `test-logger`,
`test-yaml`) are also built by `make test`.

For integration testing with an OTel Collector and a backend such as
Elasticsearch/Kibana, use the Docker Compose setup in `test/elastic-apm/`.

A reference [OpenTelemetry Collector](https://github.com/open-telemetry/opentelemetry-collector)
configuration is provided in `test/otelcol/`.  It collects all three signals
(traces, metrics, and logs) over OTLP/gRPC and OTLP/HTTP, and exports traces
to a Jaeger instance reachable at a local IP address via OTLP/HTTP.

## Quick Start

### Tracing

```c
#include <stdio.h>
#include <stdlib.h>
#include <opentelemetry-c-wrapper/include.h>

int main(void)
{
    struct otelc_tracer *tracer;
    struct otelc_span   *span;
    char                *err = NULL;

    /* Initialize the library */
    if (otelc_init("otel-cfg.yml", &err) != OTELC_RET_OK) {
        fprintf(stderr, "Failed to init: %s\n", err);
        free(err);
        return 1;
    }

    /* Create and start a tracer */
    tracer = otelc_tracer_create(&err);
    if (tracer == NULL) {
        fprintf(stderr, "Failed to create tracer: %s\n", err);
        free(err);
        otelc_deinit(NULL, NULL, NULL);
        return 1;
    }
    tracer->ops->start(tracer);

    /* Start a span, do work, end the span */
    span = tracer->ops->start_span(tracer, "my-operation");
    /* ... */
    span->ops->end(&span);

    /* Clean up */
    otelc_deinit(&tracer, NULL, NULL);
    return 0;
}
```

### Metrics

```c
#include <stdio.h>
#include <stdlib.h>
#include <opentelemetry-c-wrapper/include.h>

int main(void)
{
    struct otelc_meter *meter;
    struct otelc_value  value;
    char               *err = NULL;
    int64_t             counter;

    otelc_init("otel-cfg.yml", &err);

    meter = otelc_meter_create(&err);
    meter->ops->start(meter);

    counter = meter->ops->create_instrument(meter, "requests", "Total request count", "1", OTELC_METRIC_INSTRUMENT_COUNTER_UINT64, NULL);

    value.u_type         = OTELC_VALUE_UINT64;
    value.u.value_uint64 = 1;
    meter->ops->update_instrument(meter, counter, &value);

    otelc_deinit(NULL, &meter, NULL);
    return 0;
}
```

### Logging

```c
#include <stdio.h>
#include <stdlib.h>
#include <opentelemetry-c-wrapper/include.h>

int main(void)
{
    struct otelc_logger *logger;
    char                *err = NULL;

    otelc_init("otel-cfg.yml", &err);

    logger = otelc_logger_create(&err);
    logger->ops->start(logger);

    logger->ops->log_span(logger, OTELC_LOG_SEVERITY_INFO, 0, NULL, NULL, NULL, NULL, 0, "Application started successfully");

    otelc_deinit(NULL, NULL, &logger);
    return 0;
}
```

## Library API

The API is organized around instance structs that each carry a pointer to an
operations vtable, one pair per signal:

- `struct otelc_tracer` -- creates trace spans and propagates context.
- `struct otelc_meter` -- creates and records metric instruments.
- `struct otelc_logger` -- emits structured log records.

Operations are invoked through the `ops` pointer:

```c
tracer->ops->start_span(tracer, "name");
```

Convenience macros `OTELC_OPS()` and `OTELC_OPSR()` (the latter passes `&ptr`
so the callee can NULL the pointer on destroy/end) are provided in
`<opentelemetry-c-wrapper/define.h>`:

```c
OTELC_OPS(tracer, start_span, "name");
OTELC_OPSR(span, end);
```

### Lifecycle

1. `otelc_init(cfgfile, &err)` -- parse the YAML configuration.
2. `otelc_*_create(&err)` -- allocate a signal instance.
3. `instance->ops->start(instance)` -- start the pipeline.
4. *(use the signal)* -- create spans, record metrics, emit logs.
5. `otelc_deinit(&tracer, &meter, &logger)` -- shut down and free.

### Return Conventions

Functions that can fail return `OTELC_RET_OK` (0) on success or
`OTELC_RET_ERROR` (-1) on failure.  Functions that create resources return a
pointer on success or `NULL` on failure.

### Utility Types

- `struct otelc_value` -- tagged union (bool, integer, double, string).
- `struct otelc_kv` -- key-value pair (key string + otelc_value).
- `struct otelc_text_map` -- dynamic array of key-value string pairs.

Including `<opentelemetry-c-wrapper/include.h>` pulls in all public headers.

## YAML Configuration

The YAML file passed to `otelc_init()` contains these top-level sections:

| Section        | Purpose                                       |
|----------------|-----------------------------------------------|
| `exporters`    | Where telemetry is sent (OTLP, ostream, etc.) |
| `readers`      | Periodic metric collection intervals          |
| `samplers`     | Trace sampling strategy                       |
| `processors`   | Batching before export (batch or single)      |
| `providers`    | Resource attributes (service name, etc.)      |
| `signals`      | Binds the above components per signal type    |

Minimal configuration exporting traces to stdout:

```yaml
exporters:
  my_exporter:
    type:     ostream
    filename: /dev/stdout

processors:
  my_processor:
    type: single

samplers:
  my_sampler:
    type: always_on

providers:
  my_provider:
    resources:
      - service.name: "my-service"

signals:
  traces:
    scope_name: "my-application"
    exporters:  my_exporter
    samplers:   my_sampler
    processors: my_processor
    providers:  my_provider
```

A complete example covering all three signals is in `test/otel-cfg.yml`.

### Supported Exporters

| Exporter      | Traces | Metrics | Logs |
|---------------|--------|---------|------|
| OTLP/gRPC     | yes    | yes     | yes  |
| OTLP/HTTP     | yes    | yes     | yes  |
| OTLP/File     | yes    | yes     | yes  |
| OStream       | yes    | yes     | yes  |
| In-Memory     | yes    | yes     | --   |
| Zipkin        | yes    | --      | --   |
| Elasticsearch | --     | --      | yes  |

## Thread Safety

All data-plane operations (creating spans, recording metrics, emitting logs) are
thread-safe and can be called concurrently.  Spans are stored in a 64-shard map
with independent locks to distribute contention.

Lifecycle operations (`otelc_init`, `otelc_*_create`, `start`, `destroy`,
`otelc_deinit`) must be called from a single thread, typically during program
startup and shutdown.

Applications can provide a custom thread-ID function via `otelc_ext_init()`
before calling `otelc_init()`.

## Documentation

- `README` -- build instructions, API overview, configuration reference, and
  usage examples.
- `MEMO` -- in-depth design notes on multithreading, span lifecycle, context
  propagation, and memory management.
- `README-sharded_map` -- analysis of span handle management strategies.
- `README-naming_convention` -- function naming patterns for variadic,
  key-value, and array-based argument styles.
- `README-configuration` -- compile-time configuration macros and their
  effects on threading, context propagation, and provider architecture.
- `TODO` -- implemented features and planned enhancements.

## License

This project is licensed under the [Apache License 2.0](LICENSE).

Copyright (C) 2026 HAProxy Technologies.
