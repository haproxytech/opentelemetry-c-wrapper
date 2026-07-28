# opentelemetry-c-wrapper

A C wrapper for the C++ OpenTelemetry API, enabling C integration with the
OpenTelemetry observability framework.

## Overview

The OpenTelemetry (OTel) C wrapper library provides a pure C API on top of the
official
[OpenTelemetry C++ client](https://github.com/open-telemetry/opentelemetry-cpp).
It was developed by [HAProxy Technologies](https://www.haproxy.com/) for use in
the [HAProxy OTel filter](https://github.com/haproxytech/haproxy-opentelemetry),
but is suitable for any C application that needs to export telemetry data.  The
build pins the underlying OTel C++ client to
[1.28.0](https://github.com/open-telemetry/opentelemetry-cpp/releases/tag/v1.28.0)
because the patch set it applies to the SDK is prepared for exactly that
release, as the [Build Instructions](#build-instructions) explain; the wrapper
itself also compiles against an older client, with the parts that require the
newer SDK compiled out.

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
with options incompatible with the OTel C wrapper.  The version of the OTel
C++ SDK used is set to 1.28.0 because the `*-opentelemetry-cpp-1.28.0.patch`
set in `scripts/build/` is prepared for exactly that SDK release.  Besides build
adjustments, the patches extend the SDK exporters with methods that the wrapper
requires: `MaybeSpawnBackgroundThread()` pre-spawns the exporter's background
threads and connections, which the unpatched SDK creates lazily on the first
export, so that no thread creation or connection setup is deferred until then,
while `SetBackgroundWaitFor()` makes the idle timeout of the OTLP/HTTP exporter
background thread configurable.  Another SDK release cannot be used without
porting the patch set.

If none of the attached `build-*.sh` scripts is used, the opentelemetry-cpp
patches in `scripts/build/` must be applied to the OpenTelemetry C++ source
tree before compilation and the same CMake configuration options found in the
`scripts/build/opentelemetry-cpp-1.28.0-install.sh` script must be used.

In particular, the wrapper requires an OTel C++ SDK built with ABI version 2
(`-DWITH_ABI_VERSION_2=ON`); the wrapper build verifies this at configuration
time.

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
`--with-libfyaml` (autotools) or `-DWITH_LIBFYAML=ON` (CMake); either option
overrides the rapidyaml default.  Requesting both parsers explicitly, with
`--with-libfyaml` and `--with-rapidyaml` together, fails at configuration time.

## Testing

Compile and run the test suite:

```sh
make test
cd test
./otel-c-wrapper-test --help
./otel-c-wrapper-test --runcount=10 --threads=8
```

Every program reads `otel-cfg.yml` and looks up the signal entry named
`default`; the `--config` and `--name` options override both.  Each prints a
PASS or FAIL line per test case and exits non-zero when a case fails.

Signal-specific test programs (`test-tracer`, `test-meter`, `test-logger`,
`test-yaml`, `test-multi`) are also built by `make test`.

The names in the test directory are libtool wrapper scripts.  The real binaries
sit in `test/.libs` and carry an rpath to the installed library, so running one
of them directly picks up the installed copy rather than the one just built.
Run the wrapper scripts instead, as `test/gdb.sh` does for the debugger.

Further tooling lives beside the programs: `speed.sh`, `speed-check.sh` and
`speed-show.sh` measure the throughput, and `fuzz-yaml.sh` runs a libFuzzer
harness over the configuration loader.  The documents
[`test/README-speed_check`](test/README-speed_check) and
[`test/README-fuzz-yaml`](test/README-fuzz-yaml) describe the last two.

For integration testing with an OTel Collector and a backend such as
Elasticsearch/Kibana, use the Docker Compose setup in `test/elastic-apm/`.

A reference [OpenTelemetry Collector](https://github.com/open-telemetry/opentelemetry-collector)
configuration is provided in `test/otelcol/`.  It receives all three signals
over OTLP/gRPC and OTLP/HTTP, and traces over the Jaeger and Zipkin protocols
as well.  Traces leave through the debug exporter and over OTLP/HTTP to an
endpoint on the local network, while metrics and logs reach the debug exporter
alone.

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
        OTELC_SFREE(err);
        return 1;
    }

    /* Create a tracer bound to the context */
    tracer = otelc_tracer_create(ctx, &err);
    if (tracer == NULL) {
        fprintf(stderr, "Failed to create tracer: %s\n", err);
        OTELC_SFREE(err);
        otelc_deinit(&ctx, NULL, NULL, NULL);
        return 1;
    }

    /* Build the trace pipeline described by the configuration */
    if (tracer->ops->start(tracer) != OTELC_RET_OK) {
        fprintf(stderr, "Failed to start tracer: %s\n", tracer->err);
        otelc_deinit(&ctx, &tracer, NULL, NULL);
        return 1;
    }

    /* Start a new span */
    span = tracer->ops->start_span(tracer, "my-operation");
    if (span == NULL) {
        fprintf(stderr, "Failed to start span: %s\n", tracer->err);
        otelc_deinit(&ctx, &tracer, NULL, NULL);
        return 1;
    }

    /* ... perform work ... */

    /* End the span; the operation also clears the pointer */
    span->ops->end(&span);

    /* Clean up the tracer and the context, then the process-wide hooks */
    otelc_deinit(&ctx, &tracer, NULL, NULL);
    otelc_lib_shutdown();

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
    int64_t             counter_id;

    /* Initialize the library; "default" selects the named signal entry */
    ctx = otelc_init("otel-cfg.yml", "default", &err);
    if (ctx == NULL) {
        fprintf(stderr, "Failed to init: %s\n", err);
        OTELC_SFREE(err);
        return 1;
    }

    /* Create a meter bound to the context */
    meter = otelc_meter_create(ctx, &err);
    if (meter == NULL) {
        fprintf(stderr, "Failed to create meter: %s\n", err);
        OTELC_SFREE(err);
        otelc_deinit(&ctx, NULL, NULL, NULL);
        return 1;
    }

    /* Build the metric pipeline described by the configuration */
    if (meter->ops->start(meter) != OTELC_RET_OK) {
        fprintf(stderr, "Failed to start meter: %s\n", meter->err);
        otelc_deinit(&ctx, NULL, &meter, NULL);
        return 1;
    }

    /* Create a counter instrument; the returned id addresses it later */
    counter_id = meter->ops->create_instrument(meter,
        "requests", "Total request count", "1",
        OTELC_METRIC_INSTRUMENT_COUNTER_UINT64, NULL);
    if (counter_id == OTELC_RET_ERROR) {
        fprintf(stderr, "Failed to create instrument: %s\n", meter->err);
        otelc_deinit(&ctx, NULL, &meter, NULL);
        return 1;
    }

    /* Record a measurement */
    value.u_type         = OTELC_VALUE_UINT64;
    value.u.value_uint64 = 1;
    (void)meter->ops->update_instrument(meter, counter_id, &value);

    /* Clean up the meter and the context, then the process-wide hooks */
    otelc_deinit(&ctx, NULL, &meter, NULL);
    otelc_lib_shutdown();

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

    /* Initialize the library; "default" selects the named signal entry */
    ctx = otelc_init("otel-cfg.yml", "default", &err);
    if (ctx == NULL) {
        fprintf(stderr, "Failed to init: %s\n", err);
        OTELC_SFREE(err);
        return 1;
    }

    /* Create a logger bound to the context */
    logger = otelc_logger_create(ctx, &err);
    if (logger == NULL) {
        fprintf(stderr, "Failed to create logger: %s\n", err);
        OTELC_SFREE(err);
        otelc_deinit(&ctx, NULL, NULL, NULL);
        return 1;
    }

    /* Build the log pipeline described by the configuration */
    if (logger->ops->start(logger) != OTELC_RET_OK) {
        fprintf(stderr, "Failed to start logger: %s\n", logger->err);
        otelc_deinit(&ctx, NULL, NULL, &logger);
        return 1;
    }

    /* Emit a record; the NULL span leaves it uncorrelated with any trace */
    (void)logger->ops->log_span(logger, OTELC_LOG_SEVERITY_INFO,
        0, NULL, NULL, NULL, NULL, NULL, 0,
        "Application started successfully");

    /* Clean up the logger and the context, then the process-wide hooks */
    otelc_deinit(&ctx, NULL, NULL, &logger);
    otelc_lib_shutdown();

    return 0;
}
```

## Library API

The API is organized around instance structs that each carry a pointer to an
operations vtable, one pair per signal:

- `struct otelc_tracer` -- creates trace spans and propagates context.
- `struct otelc_meter` -- creates and records metric instruments.
- `struct otelc_logger` -- emits structured log records.

A tracer hands out two more handle types, which follow the same convention but
carry no telemetry state of their own:

- `struct otelc_span` -- one started, not yet ended, trace span.
- `struct otelc_span_context` -- a span identity, built from raw IDs or
  extracted from a carrier.

Each signal instance carries an `err` member with the text of the last error it
recorded, a `scope_name` member, an `enabled` gate, a `ctx` back-pointer and the
`ops` pointer.  The `err` string belongs to the instance and is released with
it; the strings returned through an `err` argument belong to the caller and go
to `OTELC_SFREE()`.  The library context itself is opaque.

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

Both macros are statement expressions over `__typeof__` and therefore need GCC
or Clang; the plain call through the `ops` pointer works with any compiler.

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
   the context together with any registered signal instances.  Only per-context
   state is touched; the SDK internal log handler and the callbacks installed
   via `otelc_ext_init()` remain valid for any other live context.
6. `otelc_lib_shutdown()` -- optional, call once after the final context has
   been destroyed to reset the process-wide hooks installed via
   `otelc_log_set_handler()` and `otelc_ext_init()` to their defaults.
   Required before unloading caller code that owns any of those callbacks;
   otherwise optional.

All configuration and provider state is per-context, so multiple contexts may
coexist in the same process, each with its own configuration and named signal
selection.  The helper `otelc_close_cfg(ctx)` releases the parsed YAML document
attached to a context independently of the providers.

### Return Conventions

Functions that can fail return `OTELC_RET_OK` (0) on success or
`OTELC_RET_ERROR` (-1) on failure.  Functions that create resources return a
pointer on success or `NULL` on failure.

Two families depart from that pair, and both are recognized by testing for
`OTELC_RET_ERROR` rather than for `OTELC_RET_OK`, since a false answer and a
success share the value zero: the `enabled` predicates return true or false,
and `create_instrument()` returns a non-negative instrument ID while
`otelc_ctx_nstate_get()` returns an `otelc_ctx_name_t` value.

### Utility Types

- `struct otelc_value` -- tagged union carrying a bool, a signed or unsigned
  32-bit or 64-bit integer, a double, a string or binary data, with a null
  variant for the absent value.
- `struct otelc_kv` -- key-value pair (key string + otelc_value).
- `struct otelc_text_map` -- dynamic array of key-value string pairs, each pair
  carrying flags that say whether the map duplicates or adopts the key and the
  value.

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

Besides these, the document accepts the optional top-level scalar key
`handle_map_shards`, which sets the shard count of the span handle maps; its
value must be a power of two in the range 1..65536 and takes effect on the
first `otelc_init()` call.

The `signals` section groups its `traces`, `metrics`, and `logs` subtrees by
name, so a single configuration can hold several independent definitions per
signal type.  When the library context is created, the `name` argument given
to `otelc_init()` selects the entry to load.  If no matching entry exists, the
entry called `default` is used as a fallback.  When that is also absent but the
subtree keeps its settings directly under `signals/<signal>` (the legacy layout
without the naming level, recognized by the `scope_name` key), that subtree
itself is used.  If no variant is present, creating the corresponding signal
fails.  The outcome of the lookup is recorded per signal section and can be
read back with `otelc_ctx_nstate_get()`; a section that is missing from the
document altogether is recorded as absent.

Minimal configuration exporting traces to stdout:

```yaml
exporters:
  my_exporter:
    type:     ostream
    filename: stdout

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

A program calling `otelc_init(cfgfile, "debug", &err)` then loads the `debug`
entry; any other name falls back to `default`.

A complete example covering all three signals is in `test/otel-cfg.yml`.

The `signals/logs` subtree accepts an optional `min_severity` key that sets
the initial minimum log severity (for example `INFO`); the threshold can be
changed at runtime with the logger's `set_min_severity` operation.

### Thread Settings

Components that spawn background threads (batch processors, OTLP File and HTTP
exporters, and periodic metric readers) accept two optional settings:

| Setting       | Description                                                     |
|---------------|-----------------------------------------------------------------|
| `thread_name` | OS thread name (truncated to 15 characters)                     |
| `cpu_id`      | Bind thread to a CPU core (0-4095, or -1 for unset)             |

Example:

```yaml
processors:
  my_processor:
    type:        batch
    thread_name: "proc/batch trac"
    cpu_id:      2
```

Both settings reach the thread through the SDK thread instrumentation
interface, which the SDK build must enable with
`ENABLE_THREAD_INSTRUMENTATION_PREVIEW`; without that flag the values are still
read and validated, but never applied.  The `cpu_id` setting uses
`pthread_setaffinity_np()` and is only effective on Linux (requires `sched.h`).
The OTLP/gRPC exporter accepts both settings for configuration consistency, but
they are not applied because the OTel C++ SDK does not provide runtime options
for gRPC exporter threads.

The Elasticsearch log exporter also spawns a background thread, but accepts
neither setting; its thread runs with the process defaults.

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
thread-safe and can be called concurrently on the same instance.  The error text
a failing operation leaves in the instance is replaced under a lock, so
concurrent failures cannot corrupt it.

Spans are stored in a sharded map whose independently locked shards distribute
contention; the shard count is set with the top-level `handle_map_shards` YAML
key.  That shared-handle model is what the default build selects, and it is what
allows a span started on one thread to be ended on another.  The alternative
model, chosen at compile time, keeps the handles thread-local: it removes the
locking altogether but confines each span to the thread that made it, as
[`README-configuration`](README-configuration) describes.

Before `start()` or `destroy()` runs on a tracer, meter or logger, the caller
has to drain every concurrent operation on that same instance and end the spans
it produced; a call still in flight when either of them runs reads state that is
being replaced or freed.  Instances are independent of one another, so the
ordering is required per instance rather than across the process.

Two calls are process-wide instead: `otelc_ext_init()` installs the allocation
and thread-id callbacks and has to run before anything that uses them, and
`otelc_lib_shutdown()` clears them again, so it has to run only after every
context and every instance has been destroyed.

Each thread is identified internally by a numeric ID; an application supplies
its own thread-ID function through `otelc_ext_init()`, and without one the
library assigns the identifiers itself.

SDK background threads can optionally be bound to a specific CPU core via the
`cpu_id` YAML setting.  See the [Thread Settings](#thread-settings) section
for details.

## Documentation

- [`README`](README) -- build instructions, API overview, configuration
  reference, and usage examples.
- [`MEMO`](MEMO) -- in-depth design notes on multithreading, span lifecycle,
  context propagation, and memory management.
- [`README-sharded_map`](README-sharded_map) -- analysis of span handle
  management strategies.
- [`README-meter_performance`](README-meter_performance) -- meter locking,
  attribute handling, and instrument lookup performance analysis.
- [`README-naming_convention`](README-naming_convention) -- function naming
  patterns for variadic, key-value, and array-based argument styles.
- [`README-configuration`](README-configuration) -- compile-time configuration
  macros and their effects on threading, context propagation, and provider
  architecture.
- [`test/README-fuzz-yaml`](test/README-fuzz-yaml) -- fuzzing harness for the
  configuration loader.
- [`test/README-speed_check`](test/README-speed_check) -- throughput regression
  checking.
- [`ChangeLog`](ChangeLog) -- release notes, grouped by package version.
- [`TODO`](TODO) -- implemented features and planned enhancements.

## License

This project is licensed under the [Apache License 2.0](LICENSE).

Copyright (C) 2026 HAProxy Technologies.
