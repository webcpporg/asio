# Shipped Asio implementation

Native public equivalents forward to installed Boost.Asio. Wasm selects the
pinned implementation under `include/webcpp/asio/detail/backend/`; its namespace,
configuration macros, guards and internal includes are isolated from Boost.Asio.
Consumers never apply a patch or manufacture a replacement `boost/asio` tree.

`pin.json` identifies the upstream Boost 1.92 headers by SHA-256. The correction
manifest verifies every before/after input to the port. The private headers
select WASI corrections only for WASIp2/P3; Emscripten retains the corresponding
upstream algorithm where those changes are WASI-specific. Common corrections
carry explicit target metadata. The browser scheduler always queues posted and
deferred completions on its shared queue and signals the wakeup: JavaScript may
re-enter the same thread while its reactor is suspended, before a private queue
could be drained. This also applies to concurrency hint 1, including the
allocator constructor, without changing the constructor API or native behavior.
The system executor's three `noexcept` context/occupancy accessors explicitly
terminate if initial construction of the global context throws. This preserves
the upstream fatal-initialization contract while making its exception boundary
visible to static analysis; it does not turn allocation failure into success.
`extensions/` contains
the maintained resolver/waitable adapters. SDK bridge source and the pinned
wasi-libc ABI declarations are shipped in `src/wasip2` and `src/wasip3`.

The maintainer command is separate from every consumer build:

```sh
python3 upstream/sync.py --boost-include /path/to/boost/include
python3 upstream/sync.py --boost-include /path/to/boost/include --check
```

`generated.json` records the resulting file hashes. Preserve upstream copyright
and licence notices. Change maintained inputs and regenerate, never hand-edit
the outputs. Native Windows now uses upstream IOCP, including any upstream
allocator limitation; the old build-time correction is not silently selected.
