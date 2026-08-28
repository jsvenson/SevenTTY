/* shell_util.h -- dependency-free pure-logic helpers shared by shell.c and the
 * host-side test harness. OSType values are passed as uint32_t so the host
 * (8-byte unsigned long) and the Mac (4-byte unsigned long) agree on packing.
 */
#ifndef SHELL_UTIL_H
#define SHELL_UTIL_H

#include <stdint.h>

#define MAX_ARGS 32

int      parse_args(char* line, char* argv[], int max_args);
int      glob_match(const char* pattern, const char* str);
void     fmt_human(char* buf, int bufsz, long bytes);
void     ostype_to_str(uint32_t t, char* out);
uint32_t str_to_ostype(const char* s);

#endif /* SHELL_UTIL_H */
