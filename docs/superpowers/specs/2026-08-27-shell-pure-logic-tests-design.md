# Design: Host-side unit tests for shell pure-logic helpers

Date: 2026-08-27
Status: Approved (in brainstorming)

## Summary

SevenTTY's local shell (`shell.c`, ~10,900 lines) is a single monolithic translation
unit with ~30 commands and many helpers, all `static` and coupled to classic-Mac
Toolbox APIs. This makes host-side automated testing impossible today, because the
code won't link outside Retro68.

This first iteration extracts the handful of **dependency-free (pure) helper
functions** out of `shell.c` into a standalone file, and adds a tiny host-side
assert-based test harness that runs in milliseconds with a plain `cc`. It is the
first step of the "Approach A" testing strategy (extract pure logic → host unit
tests), a deliberate precursor to a future "Approach B/C" harness that stubs the
Toolbox / drives the emulator (out of scope here).

**Core principle:** the tested code IS the shipped code. The functions are *moved*
out of `shell.c` (not copied), and `shell.c` calls the moved versions. The Retro68
build compiles the exact same `shell_util.c` the host tests compile, so tests can
never drift from what actually runs on the Mac.

## Scope

**In scope — extract + test these 5 functions** (all verified host-compilable, only
using string.h / ctype.h / stdio.h):

| Function | Current location | Behavior |
|----------|------------------|----------|
| `parse_args` | shell.c:434 | Split a command line into argv[]; handles double/single quotes, backslash-escaped spaces, MAX_ARGS cap. Mutates the line in place. |
| `glob_match` | shell.c:557 | Case-insensitive glob match supporting `*` and `?`; recursive; full-string anchored. |
| `fmt_human` | shell.c:2165 | Format byte counts as KB/MB/GB with one decimal place. |
| `ostype_to_str` | shell.c:517 | 4-char OSType → string; non-printable bytes → `?`. |
| `str_to_ostype` | shell.c:533 | String → 4-byte OSType; short input right-padded with spaces. |

**Out of scope (future work, recorded in TODO.md):**
- Approach B harness (stub `vt_write`, File Manager against a temp dir).
- Approach C (drive the real emulator, capture output).
- Extracting anything beyond these 5 functions in this iteration.
- The ICMP ping feature (separate TODO item).

## Files

| File | Purpose |
|------|---------|
| `shell_util.h` | Public prototypes for the 5 functions; defines `MAX_ARGS` (32). |
| `shell_util.c` | The 5 moved functions, non-`static`. Dependency-free: includes only `<stdio.h>`, `<string.h>`, `<ctype.h>` and `shell_util.h`. |
| `tests/test_shell_util.c` | Hand-rolled assert harness + all test cases. |
| `tools/run_host_tests.sh` | Compiles and runs the harness with `cc`. |

### Wiring

- **shell.c**: remove the 5 `static` definitions; `#include "shell_util.h"`; call the
  external versions. `MAX_ARGS` is still used by `shell_execute` (shell.c:10293), so
  it comes from `shell_util.h` via the include. `glob_match` is recursive and must be
  non-`static` (public) so the header can declare it; its self-calls resolve in-file.
- **CMakeLists.txt:9**: add `shell_util.c` to the `add_application` source list so the
  Retro68 build links the same file the tests compile.

### Correctness trap: OSType width

On the classic Mac, `OSType` is a 4-byte `unsigned long`. On the host (macOS, LP64)
`unsigned long` is 8 bytes. The extracted file must use a **fixed 4-byte type** for the
OSType parameters — `uint32_t` from `<stdint.h>` — so the host tests and the Mac build
agree on the type's width and the bit-shift packing (`(t >> 24)` etc.). shell.c passes
its `OSType` values (which fit in 4 bytes) to these parameters; the round-trip stays
correct as long as values are < 2^32.

## Test harness

`tests/test_shell_util.c` is a single self-contained file (C89, matching the project):

- A `CHECK(cond, ...)` macro that increments pass/fail counters and prints a message
  with the source line on failure.
- One test function per helper, exercising edge cases.
- `main()` runs all tests, prints `N passed, M failed`, returns nonzero if any failed
  (so the script's exit code reflects failure).

### Test cases

- **parse_args**: empty string → argc 0; simple whitespace split; double-quoted arg
  with spaces; single-quoted arg; backslash-escaped space collapsing to a literal
  space; quoted arg ending exactly at end of string; arg count capped at `MAX_ARGS`
  (32); each returned pointer points at the correct (mutated) string.
- **glob_match**: literal match; case-insensitivity; `*` at start/middle/end; multiple
  `*`; `?` matching one char; `?`/`*` failing where they shouldn't; full-string
  anchoring (partial matches fail).
- **fmt_human**: sub-1024 → `N KB`; MB boundary; GB boundary; exact multiples vs
  fractional tenths.
- **ostype_to_str / str_to_ostype**: round-trip of a 4-char code; short input padded
  with spaces; non-printable byte → `?`; all four bytes extracted in correct order.

## Build wiring

`tools/run_host_tests.sh` (sh, ~6 lines):

```sh
#!/bin/sh
cc -std=c89 -Wall -Wextra -o /tmp/seventty_host_tests \
    tests/test_shell_util.c shell_util.c
/tmp/seventty_host_tests
```

Exits with the test binary's exit code. Standalone by design: it does not touch the
Retro68 CMake build (which has known stale-cache footguns).

## Verification / definition of done

1. `./tools/run_host_tests.sh` runs green — all 5 helpers tested, nonzero exit on failure.
2. The Retro68 build still compiles and links with `shell_util.c` in the source list
   (rebuild `build-m68k`/`build-ppc` from a fresh dir per CLAUDE.md guidance).
3. shell.c behavior is unchanged — the moved functions are byte-for-byte identical,
   just relocated and de-`static`ed.
4. Optional manual sanity check on the emulator: `ls`, `cd`, `df -h` still behave
   (exercise `glob_match`, `parse_args`, `fmt_human`).
