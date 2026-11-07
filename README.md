# asio

WebCPP Asio brings Boost.Asio's programming model to native C++, browsers through
Emscripten, and WASIp2/P3 through runtimes such as Wasmtime. Native equivalents
are actual Boost types. Wasm selects a versioned implementation shipped with the
library, using the same explicit source contract with B2 or another build system.
Use `webcpp/asio/` headers and `webcpp::asio` names throughout portable code.

The browser context suspends through JSPI or Asyncify, wakes for browser events,
and yields between bounded turns. The configurable default is 100 handlers per
turn; it does not interrupt a long handler or promise a frame deadline.
WASI provides guest TCP/UDP and asynchronous forward DNS, subject to host grants.
See the [capability matrix](doc/capabilities.adoc),
[WASIp2 guide](doc/wasip2.adoc), [WASIp3 guide](doc/wasip3.adoc) and
[external build recipe](doc/distribution.adoc) for the supported variants.

Optional `/webcpp/asio//tls` supplies source-built OpenSSL and embedded Mozilla
CA roots on native and WASI targets. The TLS client factory accepts replacement
trust roots explicitly. Browser HTTPS/WSS uses the browser's TLS policy instead.
Cobalt's coroutine facade, compiled sources and runner belong to the separate
WebCPP Cobalt library; the old Asio Cobalt target names are forwarding aliases.

## First program

Schedule a message and a timer, then run their context. The complete
[tutorial](doc/html/index.html#asio-quick-start) walks through this program
natively and with both Emscripten suspension variants.

<!-- include::example/asio_quick_start.cpp[tag=program] -->
```cpp
#include <webcpp/asio/io_context.hpp>
#include <webcpp/asio/steady_timer.hpp>

#include <webcpp/test/exit_status.hpp>

#include <webcpp/asio/post.hpp>

#include <chrono>
#include <cstdio>

namespace asio = webcpp::asio;
using namespace std::chrono_literals;

int main() {
    asio::io_context io;
    asio::steady_timer timer(io);

    webcpp::asio::post(io, [&timer] {
        std::puts("posted");
        timer.expires_after(50ms);
        timer.async_wait([](boost::system::error_code ec) {
            if (ec) {
                std::printf("timer error: %d\n", ec.value());
            } else {
                std::puts("timer fired");
            }
        });
    });

    const auto handlers = io.run();  // lint-run: main owns the program's event loop
    std::printf("run returned %zu handlers\n", handlers);
    return webcpp::test::exit_status(0);
}
```

<!-- include::example/asio_quick_start.expected[] -->
```text
posted
timer fired
run returned 2 handlers
```

## Building and testing

asio is developed inside the webcpp superproject, as a Boost library is
developed inside Boost:

    git clone --recursive https://github.com/webcpporg/webcpp
    cd webcpp

From there, the four lanes of the continuous integration build and run
asio's tests and examples:

    b2 libs/asio/test libs/asio/example
    b2 toolset=emscripten libs/asio/test libs/asio/example
    b2 toolset=emscripten libs/asio/test//asyncify
    b2 toolset=emscripten webcpp-suspend=asyncify libs/asio/example
    b2 toolset=emscripten libs/asio/test/browser//browser

The first builds them natively, runs every test and every example, and
compares each example's output with the `.expected` file beside it. The
second does the same on emscripten, run by node, with the toolset that
`user-config.jam` registers against emsdk, in the default suspension
variant, JSPI. The third, the lane `asyncify`, runs every browser test again
under Asyncify, the other variant.
The extra `webcpp-suspend=asyncify` command also executes all guide examples
under Asyncify. The lane `browser` runs the loader that picks a variant under node,
loading both built artifacts and checking their output. It also runs idle,
animation and loader checks in headless Chrome, on JSPI and Asyncify.
`CHROME` names the executable; on macOS the driver can use the newest
Playwright headless shell already installed in its cache. b2 compares timestamps, so a
result that counts is built from scratch, with `b2 -a`, and the CI gives
each lane a `--build-dir` of its own. A b2 project uses asio through
`/webcpp/asio//asio`.

A page imports `load` from `webcpp-asio-loader.mjs`, shipped in
`include/webcpp/asio/`, and calls `load('page_program.variants.json')`.
The loader detects JSPI support, selects JSPI or Asyncify from the build's
manifest and returns `{variant, module}` when its factory resolves. A JSPI
factory can await `main`, so an indefinitely running program must expose
its readiness through its module callbacks instead of awaiting `load`. The manifest
and its artifacts keep the relative layout produced by `webcpp.variants`.

The documented binary-size table uses optimized release builds only. Build
both page variants and measure their uncompressed `.wasm` files with:

    b2 -a toolset=emscripten variant=release cxxflags=-O3 linkflags=-O3 libs/asio/example//asyncify

The link flag is required because the current b2 Emscripten toolset does not
forward the compilation optimization level to the link. The guide records
the toolchain and six profiles: `-O2`, `-O3`, `-Os`, `-Oz`, `-O3 -flto`
and `-Oz -flto`. It reports Wasm and JavaScript bytes separately for both
suspension variants, with exceptions on and off. It also records complete
build durations and separate module-initialization and example-execution
times, including sample counts and variability. Timer waits are included
in execution time; these examples do not measure throw/catch latency or
CPU-only throughput. Use the same flags for compilation and link, with a
separate build directory per profile and exception setting. Runtime memory and transfer
compression are outside the tables.

## Documentation

asio's page, with its API reference, is published at
<https://webcpporg.github.io/webcpp/libs/asio/>; `b2 libs/asio/doc` builds
it into `doc/html/index.html`. The guide uses Diataxis to distinguish
learning tutorials, practical instructions, explanations and reference.
Its AsciiDoc includes the API reference generated by MrDocs from the C++
Doc Comments. All shown programs and outputs are checked against their
compiled examples and `.expected` files.

## License

Distributed under the [Boost Software License, Version 1.0](LICENSE_1_0.txt).
