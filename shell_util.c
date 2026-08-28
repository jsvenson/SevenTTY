/* shell_util.c -- dependency-free pure-logic helpers extracted from shell.c.
 *
 * Tested host-side by tests/test_shell_util.c. The Retro68 build compiles this
 * exact file, so the tested code is the shipped code. Keep this file free of
 * Toolbox includes so it compiles with plain cc.
 */
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "shell_util.h"

/* parse a command line into argc/argv, handling quoting and backslash escapes.
   modifies line in-place to collapse escape sequences. */
int parse_args(char* line, char* argv[], int max_args)
{
	int argc = 0;
	char* p = line;

	while (*p && argc < max_args)
	{
		/* skip whitespace */
		while (*p == ' ' || *p == '\t') p++;
		if (*p == '\0') break;

		if (*p == '"')
		{
			/* quoted arg */
			p++;
			argv[argc++] = p;
			while (*p && *p != '"') p++;
			if (*p == '"') *p++ = '\0';
		}
		else if (*p == '\'')
		{
			p++;
			argv[argc++] = p;
			while (*p && *p != '\'') p++;
			if (*p == '\'') *p++ = '\0';
		}
		else
		{
			/* unquoted arg: handle backslash-escaped spaces */
			char* dst = p;
			argv[argc++] = dst;
			while (*p)
			{
				if (*p == '\\' && *(p+1) == ' ')
				{
					/* escaped space: collapse to literal space */
					*dst++ = ' ';
					p += 2;
				}
				else if (*p == ' ' || *p == '\t')
				{
					break; /* unescaped whitespace = end of arg */
				}
				else
				{
					*dst++ = *p++;
				}
			}
			if (*p) { *dst = '\0'; p++; }
			else { *dst = '\0'; }
		}
	}

	return argc;
}

/* simple glob match: supports * and ? only, case-insensitive */
int glob_match(const char* pattern, const char* str)
{
	while (*pattern)
	{
		if (*pattern == '*')
		{
			pattern++;
			if (!*pattern) return 1; /* trailing * matches everything */
			while (*str)
			{
				if (glob_match(pattern, str)) return 1;
				str++;
			}
			return 0;
		}
		else if (*pattern == '?')
		{
			if (!*str) return 0;
			pattern++;
			str++;
		}
		else
		{
			if (tolower((unsigned char)*pattern) != tolower((unsigned char)*str))
				return 0;
			pattern++;
			str++;
		}
	}
	return *str == '\0';
}

/* format bytes as human-readable string: "1.5 MB", "320 KB", etc. */
void fmt_human(char* buf, int bufsz, long bytes)
{
	if (bytes >= 1024L * 1024L * 1024L)
		snprintf(buf, bufsz, "%ld.%ld GB",
			bytes / (1024L * 1024L * 1024L),
			(bytes / (1024L * 1024L * 100L)) % 10);
	else if (bytes >= 1024L * 1024L)
		snprintf(buf, bufsz, "%ld.%ld MB",
			bytes / (1024L * 1024L),
			(bytes / (1024L * 100L)) % 10);
	else
		snprintf(buf, bufsz, "%ld KB", bytes / 1024L);
}

void ostype_to_str(uint32_t t, char* out)
{
	out[0] = (t >> 24) & 0xFF;
	out[1] = (t >> 16) & 0xFF;
	out[2] = (t >> 8)  & 0xFF;
	out[3] = t & 0xFF;
	out[4] = '\0';

	/* replace non-printable with '?' */
	{
		int i;
		for (i = 0; i < 4; i++)
			if (out[i] < 32 || out[i] > 126) out[i] = '?';
	}
}

uint32_t str_to_ostype(const char* s)
{
	char buf[4] = { ' ', ' ', ' ', ' ' };
	int i;
	for (i = 0; i < 4 && s[i]; i++) buf[i] = s[i];
	return ((uint32_t)buf[0] << 24) | ((uint32_t)buf[1] << 16) |
	       ((uint32_t)buf[2] << 8) | (uint32_t)buf[3];
}
