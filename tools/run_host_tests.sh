#!/bin/sh
# Host-side unit tests for shell_util.c and icmp_util.c. Standalone by design:
# uses only cc and does not touch the Retro68 CMake build (known stale-cache footguns).
cd "$(dirname "$0")/.." || exit 1
cc -std=c89 -Wall -Wextra -I. -o /tmp/seventty_host_tests \
    tests/test_shell_util.c shell_util.c
/tmp/seventty_host_tests
cc -std=c89 -Wall -Wextra -I. -o /tmp/seventty_icmp_tests \
    tests/test_icmp_util.c icmp_util.c
/tmp/seventty_icmp_tests
