#!/usr/bin/env python3
# Copyright (c) 2026 WebCpp.org
#
# Distributed under the Boost Software License, Version 1.0. (See
# accompanying file LICENSE_1_0.txt or copy at
# https://www.boost.org/LICENSE_1_0.txt)
"""Verify the private wasi-libc ABI used by the P3 waitable descriptor bridge."""
import argparse
import hashlib
import json
from pathlib import Path


def verify(sdk: Path, bundle: Path):
    manifest = json.loads((bundle / 'wasi-libc/manifest.json').read_text())
    version = (sdk / 'VERSION').read_text().splitlines()
    expected = 'wasi-libc: ' + manifest['commit'][:12]
    if not version or version[0] != manifest['sdk'] or expected not in version:
        raise ValueError('WASIp3 waitables require wasi-sdk ' + manifest['sdk'] +
                         ' with ' + expected + '; revalidate the bridge before changing SDK')
    verify_bundle(bundle)


def verify_bundle(bundle: Path):
    manifest = json.loads((bundle / 'wasi-libc/manifest.json').read_text())
    expected_files = set(manifest['files']) | {'manifest.json'}
    for path in (bundle / 'wasi-libc').rglob('*'):
        if path.is_symlink():
            raise ValueError('private wasi-libc snapshot contains a symlink: ' + str(path))
        relative = path.relative_to(bundle / 'wasi-libc').as_posix()
        if path.is_file() and relative not in expected_files:
            raise ValueError('private wasi-libc snapshot contains an unlisted file: ' + str(path))
    for name, expected_hash in manifest['files'].items():
        path = Path(name)
        if path.is_absolute() or '..' in path.parts:
            raise ValueError('invalid private ABI manifest path')
        actual = hashlib.sha256((bundle / 'wasi-libc' / path).read_bytes()).hexdigest()
        if actual != expected_hash:
            raise ValueError('private wasi-libc header changed: ' + name)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sdk', type=Path)
    parser.add_argument('--bundle', type=Path, default=Path(__file__).resolve().parent)
    parser.add_argument('--stamp', type=Path)
    args = parser.parse_args()
    try:
        if args.sdk:
            verify(args.sdk, args.bundle)
        else:
            verify_bundle(args.bundle)
    except (OSError, ValueError) as error:
        parser.exit(1, str(error) + '\n')
    if args.stamp:
        args.stamp.parent.mkdir(parents=True, exist_ok=True)
        args.stamp.write_text('WASIp3 waitable ABI verified\n')


if __name__ == '__main__':
    main()
