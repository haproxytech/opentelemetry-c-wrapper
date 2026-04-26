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
./build-bundle.sh [prefix-dir [install-dir [lib-type]]]
```

The `lib-type` argument controls how the OTel C++ SDK is built: `dynamic`
(default) produces shared libraries, `static` produces static archives.  Use
`static` when the OTel C wrapper itself will be linked statically.

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

**Static library build:**

The wrapper can be built as a static archive (`.a`) instead of a shared library.
This requires the OTel C++ SDK to be compiled as static libraries as well (see
`lib-type` argument above).

With autotools, both static and shared libraries are built by default.  To build
only a static library, pass `--disable-shared`:

```sh
./configure --prefix=/opt --with-opentelemetry=/opt --disable-shared
```

With CMake, use the `BUILD_STATIC` option:

```sh
cmake -DCMAKE_INSTALL_PREFIX=/opt -DOPENTELEMETRY_DIR=/opt -DBUILD_STATIC=ON ..
```

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
    struct otelc_ctx    *ctx;
    struct otelc_tracer *tracer;
    struct otelc_span   *span;
    char                *err = NULL;

    /* Initialize the library; "default" selects the named signal entry */
    ctx = otelc_init("otel-cfg.yml", "default", &err);
    if (ctx == NULL) {
        fprintf(stderr, "Failed to init: %s\n", err);
        free(err);
        return 1;
    }

    /* Create and start a tracer bound to the context */
    tracer = otelc_tracer_create(ctx, &err);
    if (tracer == NULL) {
        fprintf(stderr, "Failed to create tracer: %s\n", err);
        free(err);
        otelc_deinit(&ctx, NULL, NULL, NULL);
        return 1;
    }
    tracer->ops->start(tracer);

    /* Start a span, do work, end the span */
    span = tracer->ops->start_span(tracer, "my-operation");
    /* ... */
    span->ops->end(&span);

    /* Clean up the tracer and the context */
    otelc_deinit(&ctx, &tracer, NULL, NULL);
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
    struct otelc_ctx   *ctx;
    struct otelc_meter *meter;
    struct otelc_value  value;
    char               *err = NULL;
    int64_t             counter;

    ctx = otelc_init("otel-cfg.yml", "default", &err);

    meter = otelc_meter_create(ctx, &err);
    meter->ops->start(meter);

    counter = meter->ops->create_instrument(meter, "requests", "Total request count", "1", OTELC_METRIC_INSTRUMENT_COUNTER_UINT64, NULL);

    value.u_type         = OTELC_VALUE_UINT64;
    value.u.value_uint64 = 1;
    meter->ops->update_instrument(meter, counter, &value);

    otelc_deinit(&ctx, NULL, &meter, NULL);
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
    struct otelc_ctx    *ctx;
    struct otelc_logger *logger;
    char                *err = NULL;

    ctx = otelc_init("otel-cfg.yml", "default", &err);

    logger = otelc_logger_create(ctx, &err);
    logger->ops->start(logger);

    logger->ops->log_span(logger, OTELC_LOG_SEVERITY_INFO, 0, NULL, NULL, NULL, NULL, 0, "Application started successfully");

    otelc_deinit(&ctx, NULL, NULL, &logger);
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

1. `ctx = otelc_init(cfgfile, name, &err)` -- parse the YAML configuration
   and create a library context.  The `name` selects which named signal
   entry is loaded under each signal section; if no entry matches, the
   `default` entry is used.
2. `otelc_*_create(ctx, &err)` -- allocate a signal instance bound to the
   context.
3. `instance->ops->start(instance)` -- start the pipeline.
4. *(use the signal)* -- create spans, record metrics, emit logs.
5. `otelc_deinit(&ctx, &tracer, &meter, &logger)` -- shut down and free
   the context together with any registered signal instances.

The library does not keep global state, so multiple contexts may coexist in
the same process, each with its own configuration and named signal selection.
The helper `otelc_close_cfg(ctx)` releases the parsed YAML document attached
to a context independently of the providers.

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

The `signals` section groups its `traces`, `metrics`, and `logs` subtrees
by name, so a single configuration can hold several independent definitions
per signal type.  When the library context is created, the `name` argument
to `otelc_init()` selects the entry to load.  If no matching entry exists,
the entry called `default` is used as a fallback; if neither is present,
initialization fails.

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
    default:
      scope_name: "my-application"
      exporters:  my_exporter
      samplers:   my_sampler
      processors: my_processor
      providers:  my_provider
```

The same `signals/traces/<name>` layout applies to `metrics` and `logs`.
Multiple named entries can coexist:

```yaml
signals:
  traces:
    default:
      scope_name: "my-application"
      ...
    debug:
      scope_name: "my-application/debug"
      ...
```

A program calling `otelc_init(cfgfile, "debug", &err)` then loads the
`debug` entry; any other name falls back to `default`.

A complete example covering all three signals is in `test/otel-cfg.yml`.

### Thread Settings

Components that spawn background threads (batch processors, OTLP File and HTTP
exporters, and periodic metric readers) accept two optional settings:

| Setting       | Description                                                     |
|---------------|-----------------------------------------------------------------|
| `thread_name` | OS thread name (truncated to 15 characters)                     |
| `cpu_id`      | Bound thread to a CPU core (0-OTEL_MAX_CPU_ID, or -1 for unset) |

Example:

```yaml
processors:
  my_processor:
    type:        batch
    thread_name: "proc/batch trac"
    cpu_id:      2
```

The `cpu_id` setting uses `pthread_setaffinity_np()` and is only effective on
Linux (requires `sched.h`).  The OTLP/gRPC exporter accepts both settings for
configuration consistency, but they are not applied because the OTel C++ SDK
does not provide runtime options for gRPC exporter threads.

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

SDK background threads can optionally be bound to a specific CPU core via the
`cpu_id` YAML setting.  See the [Thread Settings](#thread-settings) section
for details.

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
