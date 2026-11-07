#!/usr/bin/env python3
# Copyright (c) 2026 WebCpp.org
#
# Distributed under the Boost Software License, Version 1.0. (See
# accompanying file LICENSE_1_0.txt or copy at
# https://www.boost.org/LICENSE_1_0.txt)
# See accompanying file LICENSE_1_0.txt or https://www.boost.org/LICENSE_1_0.txt.

"""Maintain the shipped Asio backend. Consumers never run this tool."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parent.parent
BUNDLE = ROOT / "upstream"
PREFIX = "webcpp/asio/detail/backend/"
LOCAL_SOCKET_HEADERS = (
    "local/basic_endpoint.hpp",
    "local/connect_pair.hpp",
    "local/datagram_protocol.hpp",
    "local/seq_packet_protocol.hpp",
    "local/stream_protocol.hpp",
)
NATIVE_ONLY = {
    "basic_signal_set.hpp": "process signals",
    "signal_set.hpp": "process signals",
    "signal_set_base.hpp": "process signals",
    "basic_raw_socket.hpp": "raw sockets",
    "generic/raw_protocol.hpp": "raw sockets",
    "ip/icmp.hpp": "ICMP sockets",
    "basic_seq_packet_socket.hpp": "sequenced-packet sockets",
    "generic/seq_packet_protocol.hpp": "sequenced-packet sockets",
}


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def relocate(text: str) -> str:
    """Isolate includes, namespaces and configuration from installed Boost.Asio."""
    text = text.replace("boost/asio/", PREFIX)
    text = re.sub(
        r"namespace boost\s*\{\s*namespace asio\s*\{",
        "namespace webcpp { namespace asio_backend {",
        text,
    )
    text = re.sub(r"(?<!\w)(?:::)?boost::asio\b", "::webcpp::asio_backend", text)
    text = re.sub(r"(?<![\w:])asio::", "::webcpp::asio_backend::", text)
    # The handler-hook helper is a global namespace, not nested in boost::asio.
    text = text.replace("asio_ ##", "webcpp_asio_backend_ ##")
    return text.replace("BOOST_ASIO_", "WEBCPP_ASIO_BACKEND_")


def preserve_system_executor_noexcept(text: str) -> str:
    """Make upstream's fatal initialization contract explicit to exception analysis."""
    for expression, expected in (
        ("detail::global<system_context>()", 2),
        ("detail::global<system_context>().num_threads_", 1),
    ):
        original = "{\n  return " + expression + ";\n}"
        if text.count(original) != expected:
            raise ValueError("upstream system_executor initialization contract changed")
        replacement = (
            "{\n"
            "  // Preserve the noexcept query contract if global initialization fails.\n"
            "#if !defined(BOOST_ASIO_NO_EXCEPTIONS)\n"
            "  try\n  {\n"
            "#endif\n"
            "    return " + expression + ";\n"
            "#if !defined(BOOST_ASIO_NO_EXCEPTIONS)\n"
            "  }\n  catch (...)\n  {\n    std::terminate();\n  }\n"
            "#endif\n}"
        )
        text = text.replace(original, replacement)
    return text.replace("#include <boost/asio/detail/executor_op.hpp>",
                        "#include <exception>\n#include <boost/asio/detail/executor_op.hpp>")


def collect(boost: Path) -> dict[str, str]:
    pin = json.loads((BUNDLE / "pin.json").read_text())
    result: dict[str, str] = {}
    for name, expected in pin["headers"].items():
        data = (boost / "boost/asio" / name).read_bytes()
        if digest(data) != expected:
            raise ValueError("upstream header differs from the pinned Boost 1.92: " + name)
        result[name] = data.decode()
    patches = json.loads((BUNDLE / "corrections.json").read_text())
    with tempfile.TemporaryDirectory(prefix="webcpp-asio-import-") as temporary:
        stage = Path(temporary)
        for name, hashes in patches["files"].items():
            relative = name.removeprefix("boost/asio/")
            data = result[relative].encode()
            if digest(data) != hashes["before"]:
                raise ValueError("correction input differs: " + name)
            destination = stage / name
            destination.parent.mkdir(parents=True, exist_ok=True)
            destination.write_bytes(data)
        subprocess.run(
            ["git", "apply", "--no-index", str(BUNDLE / "corrections.patch")],
            cwd=stage,
            check=True,
            capture_output=True,
            text=True,
        )
        for name, hashes in patches["files"].items():
            data = (stage / name).read_bytes()
            if digest(data) != hashes["after"]:
                raise ValueError("correction output differs: " + name)
            result[name.removeprefix("boost/asio/")] = data.decode()
    for path in sorted((BUNDLE / "extensions").rglob("*.hpp")):
        result[path.relative_to(BUNDLE / "extensions").as_posix()] = path.read_text()
    return result


def outputs(boost: Path) -> dict[str, str]:
    result: dict[str, str] = {}
    pin = json.loads((BUNDLE / "pin.json").read_text())
    owned = set(pin["owned_facades"])
    umbrella = (boost / "boost/asio.hpp").read_bytes()
    if digest(umbrella) != pin["umbrella_sha256"]:
        raise ValueError("upstream umbrella differs from the pinned Boost 1.92")
    umbrella_text = umbrella.decode()
    for name in (*NATIVE_ONLY, *LOCAL_SOCKET_HEADERS):
        umbrella_text = umbrella_text.replace(f"#include <boost/asio/{name}>\n", "")
    result["include/webcpp/asio/detail/backend.hpp"] = (
        "// Generated by upstream/sync.py; edit its pinned inputs, not this file.\n"
        + relocate(umbrella_text)
    )
    corrected = json.loads((BUNDLE / "corrections.json").read_text())["files"]
    for name, text in collect(boost).items():
        if name == "impl/system_executor.hpp":
            text = preserve_system_executor_noexcept(text)
        generated = "// Generated by upstream/sync.py; edit its pinned inputs, not this file.\n"
        correction = corrected.get("boost/asio/" + name)
        if correction and "emscripten" not in correction.get("targets", []):
            # These corrections were validated for WASI, not the browser's libc.
            # Keep both explicit versions under one isolated namespace; platform
            # selection is in the shipped header and never in include precedence.
            original = (boost / "boost/asio" / name).read_text()
            generated += (
                "#if defined(__wasip2__) || defined(__wasip3__)\n"
                + relocate(text)
                + "\n#else\n"
                + relocate(original)
                + "\n#endif\n"
            )
        else:
            generated += relocate(text)
        if name == "detail/config.hpp":
            # The original nested namespace inherits Boost's utilities by lookup.
            # Preserve that lookup without introducing declarations in boost::asio.
            generated += (
                "\nnamespace boost {}\n"
                "namespace webcpp::asio_backend { using namespace ::boost; }\n"
            )
        result["include/" + PREFIX + name] = generated
        if not name.endswith(".hpp") or any(
            p in ("detail", "impl", "ts") for p in Path(name).parts
        ):
            continue
        if name in owned:
            continue
        if name in ("yield.hpp", "unyield.hpp"):
            # Intentionally repeatable, and usable inside function bodies just
            # like upstream: neither an include guard nor namespace declarations.
            result["include/webcpp/asio/" + name] = (
                "// Generated by upstream/sync.py; do not edit.\n"
                f"#include WEBCPP_ASIO_BACKEND_HEADER({name})\n"
            )
            continue
        guard = "WEBCPP_ASIO_FORWARD_" + re.sub(r"[^A-Z0-9]", "_", name.upper())
        namespaces = "::".join(Path(name).parts[:-1])
        public = "webcpp::asio" + ("::" + namespaces if namespaces else "")
        backend = "::webcpp::asio_backend" + ("::" + namespaces if namespaces else "")
        if name.startswith("ssl/"):
            # ssl.hpp owns the namespace alias on socket targets and the browser
            # settings facade. A forwarding header must not reopen that alias.
            selection = "#include <webcpp/asio/ssl.hpp>\n"
            if name not in ("ssl/context.hpp", "ssl/context_base.hpp", "ssl/verify_mode.hpp"):
                selection += (
                    '#if !WEBCPP_ASIO_HAS_OPENSSL_BACKEND\n#error "webcpp::asio: this SSL operation'
                    ' requires native/WASI OpenSSL; browser TLS belongs to HTTPS/WSS"\n#endif\n'
                )
            declaration = ""
        else:
            selection = f"#include WEBCPP_ASIO_BACKEND_HEADER({name})\n"
            declaration = f"namespace {public} {{ using namespace {backend}; }}\n"
        if name in LOCAL_SOCKET_HEADERS:
            selection = (
                "#if !WEBCPP_ASIO_HAS_LOCAL_SOCKETS\n"
                '#error "webcpp::asio: local sockets require native Boost.Asio support; '
                'WASI/browser targets must use TCP or a supplied byte stream"\n'
                "#else\n" + selection
            )
            declaration += "#endif\n"
        elif name in NATIVE_ONLY:
            selection = (
                "#ifndef WEBCPP_TARGET_NATIVE\n"
                f'#error "webcpp::asio: {NATIVE_ONLY[name]} require a native target"\n'
                "#else\n" + selection
            )
            declaration += "#endif\n"
        result["include/webcpp/asio/" + name] = (
            "// Copyright (c) 2026 WebCpp.org\n"
            "// Distributed under the Boost Software License, Version 1.0.\n"
            "// See accompanying file LICENSE_1_0.txt or https://www.boost.org/LICENSE_1_0.txt.\n"
            "// Generated by upstream/sync.py; do not edit.\n\n"
            f"#ifndef {guard}\n#define {guard}\n"
            "#include <webcpp/asio/config.hpp>\n"
            + selection
            + "\n"
            + declaration
            + f"#endif  // {guard}\n"
        )
    profiles = {}
    jam = (
        "# Copyright (c) 2026 WebCpp.org\n#\n# Distributed under the Boost Software License,"
        " Version 1.0. (See\n# accompanying file LICENSE_1_0.txt or copy at\n#"
        " https://www.boost.org/LICENSE_1_0.txt)\n"
    )
    jam += "# Generated by upstream/sync.py from the same inventory as sources.json.\n"
    for target, source in (
        ("native", ""),
        ("wasip2", "wasip2/pollable.cpp"),
        ("wasip3", "wasip3/waitable.cpp"),
        ("emscripten", "browser_src.cpp"),
    ):
        profiles[target] = {
            "runtime": ["src/" + source] if source else [],
            "private_include": [f"src/{target}/wasi-libc"] if target.startswith("wasi") else [],
        }
        jam += f"ASIO_{target.upper()}_RUNTIME = {source} ;\n"
    result["sources.json"] = json.dumps({"schema": 1, "profiles": profiles}, indent=2) + "\n"
    result["src/sources.jam"] = jam
    inventory = {name: digest(data.encode()) for name, data in sorted(result.items())}
    result["upstream/generated.json"] = json.dumps(inventory, indent=2) + "\n"
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--boost-include", type=Path, required=True)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    try:
        generated = outputs(args.boost_include)
        for name, data in generated.items():
            path = ROOT / name
            if args.check:
                if not path.is_file() or path.read_text() != data:
                    raise ValueError("generated file is stale: " + name)
            else:
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(data)
    except (ValueError, OSError, subprocess.CalledProcessError) as error:
        parser.exit(1, str(error) + "\n")
    print(f"Asio shipped backend: {len(generated)} files {'verified' if args.check else 'written'}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
