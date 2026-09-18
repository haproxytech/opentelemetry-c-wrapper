# Security Policy

## Supported Versions

Security fixes go into the next release on the current 3.x line.  There are no
long term support branches and no backports to earlier releases, so the fix
for a confirmed issue reaches you by upgrading.

| **Version**          | **Supported** |
|:---------------------|:-------------:|
| Latest 3.x release   | Yes           |
| Earlier 3.x releases | No            |
| 2.x and older        | No            |

Please reproduce an issue against the latest release when you can.  The list
of changes in each release is in the [`ChangeLog`](ChangeLog), so a bug that
is already fixed is usually visible there.

## Reporting a Vulnerability

Do not report a suspected vulnerability through a public GitHub issue or a pull
request, and do not put the details in one.

Report it through GitHub's private vulnerability reporting:

https://github.com/haproxytech/opentelemetry-c-wrapper/security/advisories/new

If that form is not reachable for you, open a plain issue saying only that you
have a security report, with no details in it, and a maintainer will arrange
a private channel.

Include as much of this as you have:

- What the issue is and what an attacker gets out of it.
- The affected release or commit.
- The YAML configuration in use, when it matters, and which parser reads it
  (rapidyaml or libfyaml).
- Steps to reproduce, and a minimal proof of concept if you have one.
- The platform, the compiler, and the build type (autotools or CMake, debug
  or release, shared or thread local handles).
- The version of the OpenTelemetry C++ SDK the library was built against, and
  whether the patch set from `scripts/build/` was applied to it.
- Any mitigation or fix you already have.

## What to Expect

A maintainer confirms the report, asks for whatever is missing to reproduce it,
and then says whether it is accepted, with the reasoning either way.  A small
team maintains the project, so answers come in days rather than hours.

An accepted report gets a severity, a fix, and a release: the next regular one,
or a release of its own when the issue is bad enough to warrant it.  The fix
is announced in a GitHub security advisory and in the ChangeLog, and you are
credited in both unless you ask not to be.  The project does not pay bounties.

## Scope

The library reads input from two very different places.  The YAML configuration
file comes from the operator.  The `traceparent`, `tracestate` and `baggage`
propagation headers, and the attribute values the host application passes in,
often arrive straight from a remote request, which is the normal case behind
HAProxy.  Neither source may corrupt memory, and both have fuzzing harnesses
under `test/`.

In scope:

- Memory safety: buffer overflows, out of bounds access, use after free and
  double free, and reads of uninitialized memory.
- Crashes or memory corruption caused by malformed propagation headers, carrier
  contents, or attribute values.
- Crashes in the YAML loader on a malformed or hostile configuration file.
- Data races and lifetime bugs in operations the documentation calls thread
  safe, where they corrupt memory or leak data between instances.
- Allocation that remote input can drive without bound.
- Telemetry sent somewhere other than the configured endpoint, or a TLS setting
  that is not applied the way the configuration asks for it.
- Process data that reaches exported telemetry although the configuration never
  asked for it.
- A dependency flaw that this library's use of the dependency makes reachable.

Out of scope:

- Crashes reached by breaking the API contract, such as calling into an instance
  while `start()` or `destroy()` runs on it, or ending a span on another thread
  in the thread local handle build.  The [`README`](README) and [`MEMO`](MEMO)
  state those rules.
- A configuration doing what it asks for: an exporter aimed at the wrong host,
  verification switched off, or a queue large enough to exhaust memory, is a
  deployment decision and not a flaw in the library.
- Behaviour that appears only in a debug build, the debug allocator and the
  tracing macros included.  Report that as an ordinary issue.
- The scripts under `scripts/build/`, which install packages and libraries as
  root on a machine you control.  They are development tooling.
- Findings in the OpenTelemetry C++ SDK or another dependency that this library
  does not make reachable.  See below.

## Dependencies

The library is built against the OpenTelemetry C++ SDK pinned to 1.28.0 with
the patch set from `scripts/build/` applied, and that build pulls in protobuf,
gRPC, Abseil, c-ares, curl, and AWS-LC or OpenSSL.  The configuration is parsed
by rapidyaml, or by libfyaml when the build asks for it.

Report a flaw in one of those to its own project first.  Report it here as well
when the wrapper's use of the dependency is what makes it exploitable, or when
the pinned version has to move for users to get the fix.

## Coordinated Disclosure

Give the maintainers time to investigate and ship a fix before publishing the
details.  A confirmed issue is normally fixed in a release within 30 days, and
the advisory goes out with that release, so the wait is usually shorter.  Ninety
days from the report is the outside limit.
