# Working on asio

webcpp's rules apply here: read the superproject's
[AGENTS.md](../../AGENTS.md)
(<https://github.com/webcpporg/webcpp/blob/main/AGENTS.md>) first. This file
holds only what is specific to asio. Also read the superproject's
[CODING-GUIDELINES.md](../../CODING-GUIDELINES.md), which holds the shared
code and design rules.

## What asio is

A library of webcpp's own on Boost.Asio, not a port: `meta/libraries.json`
has `"port-of": null`, and asio has no oracle, no twins and no "Differences"
appendix. Its page, `doc/asio.adoc`, with the Doc Comments of its headers,
and its specification are what a change is checked against.

Natively equivalent public types are Boost.Asio itself. On Wasm, explicit
library-owned headers select a shipped backend through config. On Emscripten
the run family suspends into the browser and yields between bounded turns;
browser events reach the context through a posting bridge.

## Distribution contract approved on 2026-11-06

This contract supersedes consumer-time overlays in the historical measurements
below. Use `webcpp/asio/` headers and `webcpp::asio` names for portable consumers.
Do not patch installed Boost or put replacement Boost headers first. The
maintainer-only `upstream/sync.py` verifies and regenerates pinned imports;
application builds consume the already shipped files. Preserve its provenance
and regenerate rather than hand-editing those files.

Native equivalent types retain actual Boost identity, including upstream
limitations. Wasm Asio, Cobalt and Beast share one selected backend. Cobalt owns
its core, runner and SSL sources in `libs/cobalt`; Asio's old Cobalt targets are
compatibility aliases. The owner approved a corrected native Cobalt SSL wrapper
as the sole native Cobalt identity exception, not arbitrary core corrections.

The migration plan and ledger in the superproject record current evidence.
The dated benchmark tables below remain historical until measured again.
Do not infer the full application matrix from a timer or TLS witness.

## Cautions

- **Asyncify's stack is measured, not guessed.** `-sASYNCIFY_STACK_SIZE`
  is 8192 unless a program raises it with `<asyncify-stack-size>`, declared
  in `build.jam`: twice 4096, the smallest power of two with which every
  test passed in every build b2 links today (2026-10-11, Emscripten 6.0.11).
  The deepest is release, compiled with `-O3` and linked without
  optimization, as b2's emscripten toolset links it: 3,540 bytes in
  `bridge_asyncify`, against 1,272 in a debug build. Linked with `-O3`, the
  deepest test wrote 416 and the run family's deepest path 384, against
  2,404 linked without. The asyncify lane builds debug alone, so the
  measurement covers the other builds itself: the bytes are read from
  Asyncify's own data when an unwind stops, and `asyncify_stack` does it in
  debug, release and release linked with `-O3`, for every suspension of the
  run family, failing when one leaves less than half of the stack. A change
  that adds a way to suspend (a coroutine's, a promise's) measures it there,
  and a test that overflows traps with `RuntimeError: unreachable`, every
  time. When b2's release link gets its `-O`, measure again: the deepest
  build becomes debug's.
- **The stack test must reach the intended wait.** Its reactor cases hold
  work until a JavaScript macrotask posts the completing handler, delay entry
  by 25 ms and require a nonzero-timeout poll. An already-expired C++ timer
  can suspend in the initial poll without reaching the deeper wait. The
  test reads `doPollAsync` as well as Asyncify's private runtime; a runtime
  change that removes either observation must fail rather than skip it.
  The 2026-10-14 re-review measured maxima of 1164/844 bytes in debug,
  2896/1940 in release and 384/288 in release linked with `-O3`, with/without
  exceptions respectively. `run_until` has its own yield measurement too.
- **The run family is called on `main`'s stack alone**, or on the stack of
  another export the program lists for suspension. Anywhere else its first
  suspension fails: under JSPI it throws a `SuspendError`, and under
  Asyncify it starts an unwind that corrupts a suspended `main`. Nothing in
  asio, and nothing in a library built on it, calls `run`, `run_one`,
  `poll` or `dispatch` from a JavaScript event: a library is given an
  executor, and a program's own `run` says why after `lint-run:`.
- **The bridge only posts.** Delivering a browser event looks its
  registration up and posts its handler to the `io_context`; it never polls,
  runs or dispatches. While `run` is suspended, an event sees
  `running_in_this_thread()` true, so a `dispatch` there would run its
  handler inside the event, on the wrong stack; and under Asyncify a wake
  must come from JavaScript with no compiled code on the stack, which only a
  post gives.
- **A test ends with `webcpp::test::exit_status(...)`**: every test's `main`
  returns `webcpp::test::exit_status(boost::report_errors())`, from the
  superproject's `<webcpp/test/exit_status.hpp>`. Under Asyncify a `main`
  that suspended and then returns a failure exits 0, and one that never
  finishes exits 0 on both variants; the superproject's test support fails a
  test whose `main` did not finish, and `exit_status` is how a `main` tells
  it that it did, and with which status. A test that returns
  `boost::report_errors()` alone reads as passed whatever it found.
- **Explicit runtime sources.** The Emscripten bridge is `src/browser_src.cpp`;
  WASIp2 uses `src/wasip2/pollable.cpp`, and WASIp3 uses
  `src/wasip3/waitable.cpp`. B2 and external builds select the same files.
  Link the browser bridge as an object so its JavaScript imports survive.
  Verify each WASI bridge's private SDK ABI before compiling. Cobalt's runner
  is owned by Cobalt and must be linked only once.

## The turn limit, measured in Chrome

On 2026-10-14, macOS arm64, HeadlessChrome 153.0.8010.12 from the local
Playwright cache, Emscripten 6.0.11, Boost 1.92 and b2 debug builds:

| Run | Variant | Animation callbacks | Handlers | Full turns | Turn p95 (ms) | Maximum (ms) |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | jspi | 60 | 1724312 | 1724 | 0.700 | 1.800 |
| 1 | asyncify | 61 | 652723 | 652 | 1.600 | 9.200 |
| 2 | jspi | 60 | 1734454 | 1734 | 0.700 | 1.800 |
| 2 | asyncify | 61 | 644126 | 644 | 1.600 | 9.200 |
| 3 | jspi | 60 | 1744071 | 1744 | 0.700 | 1.900 |
| 3 | asyncify | 61 | 653795 | 653 | 1.600 | 8.900 |

Each run rebuilt the browser lane from scratch with `-a -j1`, using a
separate build directory. The programs self-post trivial handlers for one
second, with the then-default 1,000 handlers per turn. Callback entry times
come from `performance.now()`. Full turns end at the yield's message post;
the last partial turn is excluded. Both unbounded Boost controls had zero
animation callbacks in every run. These observations show animation
opportunities, not proof that particular pixels were painted.

The three runs also passed idle and loader checks on both variants: one
`setTimeout(300000)` during each three-second idle observation, and three
handlers from the artifact actually selected by the loader. The JSPI
factory awaits `main`, so the test identifies the imported URL before
awaiting that factory; an intentionally idle main never returns.

The p95 stayed below the task's 8 ms stop threshold, so that task retained
1,000 at the time. The maximum is recorded without hiding outliers. These are local
measurements, not a guarantee for heavy user handlers or other hardware.
The CI-pinned Chrome version was not executed in this session; no CI ran.
The driver fails when p95 exceeds 8 ms or no animation callbacks occur;
changing the default or replacing that witness requires the owner's decision.

On 2026-10-15, the owner selected 100 as the new default after reviewing the
Release yield benchmark in doc/yield-cost.adoc. This favors more frequent
browser scheduling opportunities over throughput for trivial handlers; it is
not a measured optimum or a time bound. Keep the historical results above
unchanged. The compiler definition and per-context overrides remain supported.

## Cobalt entry point and suspension stack

The current runner is `/webcpp/cobalt//cobalt`; Asio forwards its old target.
On Emscripten it drives WebCPP's bounded context; on native it uses the unchanged
upstream core and runner. The shipped Wasm implementation supplies the missing
no-EH promise hook. Native Boost retains that upstream limitation; do not
silently apply the Wasm correction there. The informational issue 287 warning
remains applicable without exceptions.

The Cobalt stack probe measures real Asyncify unwinds from the runner through
the yield, a Cobalt timer wait and an Asio timer wait. The timer waits cannot
expire: only a JavaScript macrotask posts their cancellation, so the test must
reach a blocking reactor poll. Measurements on 2026-10-14, Emscripten 6.0.11:

| Build | Exceptions | Yield bytes | Cobalt wait bytes | Asio wait bytes |
| --- | --- | ---: | ---: | ---: |
| Debug | on | 1016 | 1032 | 1032 |
| Debug | off | 684 | 700 | 700 |
| Release, default link | on | 968 | 1088 | 1088 |
| Release, default link | off | 748 | 956 | 956 |
| Release, link -O3 | on | 376 | 384 | 384 |
| Release, link -O3 | off | 228 | 228 | 228 |

The existing 8192-byte stack remains sufficient for these paths with more than
half free. These measurements are suspension storage, not binary sizes or a
bound on arbitrary application frames. The ordinary run-family probe remains
in the suite and shares the observation helper in test/asyncify_watch.hpp.

The cobalt and cobalt_asyncify local lanes in `libs/cobalt/test` check exact process statuses 7 and 0
with a driver; run-fail alone would also accept a trap. Debug Asyncify's explicit
std::exit can emit Emscripten's runtime-retained notice under EXIT_RUNTIME=0.
The driver permits exactly that notice, not arbitrary stderr. This adapter
preserves the configured runtime lifetime instead of forcing shutdown.

On WASIp3 the runner must not include or instantiate the POSIX signal service.
Initialize the upstream promise's `signal_set` pointer to null and omit its
cleanup there. Keep the executor, initial post, return status and internal
cancellation machinery. Automatic SIGINT/SIGTERM cancellation is unavailable;
host-driven graceful shutdown requires a separate application protocol.
The runner-only compile test is supplemented by public-target runtime tests:
`cobalt_wasi`, `cobalt_network_wasi`, `cobalt_io`, the `cobalt_wasip3` status
lane and the recurring `threads` lane in the cooperative profile.

## WASIp2 port

The owner approved P2 Asio core and supplied-stream WebSocket on 2026-11-03.
`/webcpp/asio//asio` supports `threading=single`, EH on/off, through the shared
shipped backend. P2 also supplies the source-built OpenSSL/CA targets,
Cobalt and its SSL wrapper; preserve their dedicated EH-on/off peer and
cancellation tests in `libs/cobalt/test/wasip2` and `libs/cobalt/test/tls`.
P2 threading remains unavailable
in the pinned SDK. Never infer new feature support from core support alone.
The P2 reactor reuses libc select/poll and omits the unavailable pipe interrupter.
`src/wasip2` owns the checked SDK 34.0 private descriptor-table bridge
and the asynchronous DNS extension. Revalidate its libc ABI before changing SDKs.
Preserve deferred initiation, handler associations, operation cancellation and
resource cleanup. DNS must not start a worker or block inside async initiation.
The P2 CLI stream requests output flush before switching protocol direction;
keep the real large-frame regression. Cancellation cannot preempt blocking host
imports. Run `libs/asio/test/wasip2` and its `supplied-stream` lane in both exception
profiles, plus native/P3 regressions. Keep the feature matrix and AsciiDoc guide
consistent with actual runtime evidence.

## WASIp3 port

`/webcpp/asio//asio` selects the shipped Wasm headers and `src/wasip3` SDK bridge.
Never edit installed Boost. Rebase and verify the maintainer importer's pinned
inputs and corrections explicitly when changing Boost versions. A consumer
build must not regenerate imports or select a replacement Boost include tree.

`threading=single` disables Asio threads and accepts exceptions on/off.
`threading=multi` selects wasi-sdk's experimental cooperative sysroot with
`-pthread`; SDK 34.0 provides only its no-exceptions C++ runtime. Use Wasmtime
50.0.1, the project-wide runtime; earlier tests found that 47.0.3 cannot load
the cooperative binary.
Cooperative threads do not promise CPU parallelism.

Preserve datagram boundaries: WASIp3 `send_to` and `async_send_to` concatenate
multiple nonempty UDP buffers into one datagram until wasi-libc provides
`sendmsg`. Gather before Asio's native iovec limit, use the context allocator
for synchronous storage and the handler's associated allocator for asynchronous
storage, and release operation storage before invoking the handler. Connected
`send`/`async_send` use the same gather path; multi-buffer receives scatter one
received datagram, retaining truncation semantics. The scalar/empty-buffer path
must work without heap scratch buffers. TCP partial transfers remain valid.
Both P3 thread profiles use native asynchronous DNS and the checked private
waitable-descriptor bridge. Never block initiation or use a polling timer for
lookup progress. Validate the SDK/libc ABI snapshot before using the bridge.
Keep deferred initiation, cancellation, associated executors and allocator
release-before-callback witnesses in `test/capability_wasi`.

Keep `doc/capabilities.adoc` current for every feature/target/variant change.
`/webcpp/cobalt//cobalt` selects the shipped Cobalt implementation for WASI.
Headers and compiled sources receive the same Asio configuration. Unsupported
signal, serial and seqpacket APIs are omitted; `webcpp::cobalt::thread` requires
Asio thread support. The issue-287 warning remains nonfatal. TLS uses the shared
provider with Cobalt tokens or the optional corrected SSL wrapper.

Run `libs/cobalt/test/tls//tls` for normal/buffered handshakes, SNI, peer
rejection, STARTTLS, primitive and composed transfer cancellation, and failed or
cancelled shutdown. Preserve the no-plaintext-after-TLS-attempt state contract.
Wasm composition cancellation corrections are shipped explicitly. Native core
retains upstream behavior; only concrete corrected SSL read/write overloads
supply the approved native cancellation correction. The separate native
upstream-limitation probes must not be counted as successful cancellation tests.
Transfer cancellation is terminal in these checks: close the connection, do not
infer rollback of transmitted bytes or safe reuse from `secure()` remaining true.
OpenSSL is built from the pinned superproject source through
`/webcpp/third_party//openssl`; `/webcpp/asio//openssl` forwards that dependency
and defines `WEBCPP_ASIO_HAS_OPENSSL`. Native and WASIp3 consumers must use the
matching generated headers and archives. Browser consumers retain the settings
adapter and must not link this target. Run `tools/openssl/test/run.py` in the
superproject for real TLS and entropy checks; never infer entropy quality from
random-byte uniqueness. Do not add a second OpenSSL copy or a system fallback.
The commands in `doc/wasip3.adoc` run local tests only.

## Embedded TLS trust

`/webcpp/asio//tls` adds the digest-pinned Mozilla/curl CA data target and the
`tls_client_context.hpp` factory. The factory returns errors as values, enables
peer verification and TLS >= 1.2, and never imports system roots. A supplied PEM
set replaces public roots completely; reject empty, malformed and partially
invalid sets without returning a usable context. Keep hostname/SNI requirements
explicit in examples. Emscripten returns `unsupported_target`.

Run `tools/ca_certificates/run.py --fresh` locally for the native/WASIp3 matrix.
Its WASIp3 clients have no certificate filesystem grants. Preserve that witness.
Embedded updates require rebuilding and redeploying. Do not silently download
roots during builds or runtime, or present a PEM export as Firefox's full policy.

## Windows context-owned thread allocations

The historical correction in `upstream/reference/windows-iocp` forwarded the context
allocator into IOCP-owned threads. The current native-identity contract does not
select it: native Boost 1.92 retains this limitation. Keep the original defect
and correction provenance distinct from the current support claim. Do not claim
Windows allocation coverage from macOS tests or disguise the defect with a
warm-up or an allowance for extra global allocations.

## Joint Asio and Beast closure

Preserve deferred initiation and owning resolver input lifetimes. Resolver host
and service storage uses the initiating token's associated allocator; an allocator
bound only when a deferred operation is launched cannot retroactively own that
captured storage. Keep the positive controls for unbound handlers and runtime
allocation attribution; never replace them with a permitted allocation count.

The owner clarified on 2026-11-03 that Asio and Beast are intended foundations
for a later Trystero migration. Never ban their combination to avoid a dependency
conflict. All native consumers, including libdatachannel, use the same pinned
source-built OpenSSL provider. Trystero's current native backend requires threads.

The `tls-integration` own lane runs real native and WASIp3 TLS peers, state and
cancellation checks. The native `openssl-upstream` lane additionally requests
`webcpp-openssl-tests=on` to test the exact source-built archives before use.
OpenSSL's native Perl harness is not evidence for its WASI configuration.

Use the repository's `webcpp-documentation` skill for Diataxis organization,
AsciiDoc and generated MrDocs reference. The maintained `doc/changelog.adoc`
is the final appendix; never edit generated reference or matrix files by hand.

## Capability completion and browser workers (2026-11-03)

`config.hpp` owns read-only numeric `WEBCPP_ASIO_HAS_*` capabilities. Optional
`WEBCPP_ASIO_REQUIRE_*` switches assert compile-visible requirements without
banning portable headers. Selected `WEBCPP_ASIO_HAS_OPENSSL=1` remains distinct
from platform `HAS_OPENSSL_BACKEND`. Runtime network grants, TLS identities,
page isolation and blocking host imports are not compile-time facts.

Emscripten explicitly supports S and M with JSPI/Asyncify and both EH choices.
Only S disables Asio threads. The M poll wrapper preserves suspension on the
actual main thread; do not silently enable PROXY_TO_PTHREAD. Run a browser
context only on its creator thread, post between threads, and keep DOM values
on main. `browser::main_thread_context` callbacks are nonsuspending. Its context
must outlive executor calls. See the public worker guide and the real-browser
`browser-workers` lane. The versioned raw pthread-return probe documents the
pinned Asyncify SDK limitation; it is not a passing feature test.
