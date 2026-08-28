/* tests/test_shell_util.c -- host-side unit tests for shell_util.c.
 * Plain C89, compiled with cc. Returns nonzero if any test fails.
 */
#include <stdio.h>
#include <string.h>
#include "shell_util.h"

static int g_pass = 0;
static int g_fail = 0;

#define CHECK(cond, msg) do { \
	if (cond) { g_pass++; } \
	else { g_fail++; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, msg); } \
} while (0)

static void test_parse_args(void)
{
	char line[512];
	char* argv[MAX_ARGS];
	int argc;

	line[0] = '\0';
	argc = parse_args(line, argv, MAX_ARGS);
	CHECK(argc == 0, "empty string -> argc 0");

	strcpy(line, "ls -la /tmp");
	argc = parse_args(line, argv, MAX_ARGS);
	CHECK(argc == 3, "simple whitespace split argc==3");
	CHECK(strcmp(argv[0], "ls") == 0, "argv[0] ls");
	CHECK(strcmp(argv[1], "-la") == 0, "argv[1] -la");
	CHECK(strcmp(argv[2], "/tmp") == 0, "argv[2] /tmp");

	strcpy(line, "echo \"hello world\"");
	argc = parse_args(line, argv, MAX_ARGS);
	CHECK(argc == 2, "double-quoted arg count");
	CHECK(strcmp(argv[1], "hello world") == 0, "double-quoted keeps space");

	strcpy(line, "echo 'single quoted'");
	argc = parse_args(line, argv, MAX_ARGS);
	CHECK(argc == 2, "single-quoted arg count");
	CHECK(strcmp(argv[1], "single quoted") == 0, "single-quoted keeps space");

	strcpy(line, "cat foo\\ bar.txt");
	argc = parse_args(line, argv, MAX_ARGS);
	CHECK(argc == 2, "escaped-space arg count");
	CHECK(strcmp(argv[1], "foo bar.txt") == 0, "escaped space collapses to literal");

	strcpy(line, "echo \"trailing quote\"");
	argc = parse_args(line, argv, MAX_ARGS);
	CHECK(argc == 2, "quoted arg ending at end of string");
	CHECK(strcmp(argv[1], "trailing quote") == 0, "trailing quoted content");

	{
		int i;
		char* p = line;
		*p++ = 'a';
		for (i = 1; i < 40; i++) { *p++ = ' '; *p++ = 'a'; }
		*p = '\0';
		argc = parse_args(line, argv, MAX_ARGS);
		CHECK(argc == MAX_ARGS, "arg count capped at MAX_ARGS");
	}
}

static void test_glob_match(void)
{
	CHECK(glob_match("README", "README") == 1, "literal match");
	CHECK(glob_match("readme", "README") == 1, "case-insensitive");
	CHECK(glob_match("*.c", "shell.c") == 1, "* at start");
	CHECK(glob_match("shell*", "shell.c") == 1, "* at end");
	CHECK(glob_match("s*e*.c", "shell_util.c") == 1, "multiple *");
	CHECK(glob_match("sh?ll.c", "shell.c") == 1, "? matches one char");
	CHECK(glob_match("sh?ll.c", "shll.c") == 0, "? requires one char");
	CHECK(glob_match("*.c", "shell.h") == 0, "*.c rejects .h");
	CHECK(glob_match("shell.c", "shell.c.txt") == 0, "full-string anchored at end");
	CHECK(glob_match("shell.c", "xshell.c") == 0, "full-string anchored at start");
}

static void test_fmt_human(void)
{
	char buf[32];

	fmt_human(buf, sizeof(buf), 0);
	CHECK(strcmp(buf, "0 KB") == 0, "0 bytes -> 0 KB");
	fmt_human(buf, sizeof(buf), 1024);
	CHECK(strcmp(buf, "1 KB") == 0, "1024 -> 1 KB");
	fmt_human(buf, sizeof(buf), 1024L * 1024L);
	CHECK(strcmp(buf, "1.0 MB") == 0, "1 MiB -> 1.0 MB");
	fmt_human(buf, sizeof(buf), 1536L * 1024L);
	CHECK(strcmp(buf, "1.5 MB") == 0, "1.5 MiB -> 1.5 MB");
	fmt_human(buf, sizeof(buf), 1024L * 1024L * 1024L);
	CHECK(strcmp(buf, "1.0 GB") == 0, "1 GiB -> 1.0 GB");
	fmt_human(buf, sizeof(buf), 1536L * 1024L * 1024L);
	CHECK(strcmp(buf, "1.5 GB") == 0, "1.5 GiB -> 1.5 GB");
}

static void test_ostype(void)
{
	char out[8];
	uint32_t t;

	t = str_to_ostype("APPL");
	CHECK(t == 0x4150504C, "APPL packs correctly");
	CHECK(str_to_ostype("TEXT") == 0x54455854, "TEXT packs correctly");

	ostype_to_str(str_to_ostype("ABCD"), out);
	CHECK(strcmp(out, "ABCD") == 0, "round-trip ABCD");

	t = str_to_ostype("AB");
	ostype_to_str(t, out);
	CHECK(strcmp(out, "AB  ") == 0, "short input padded with spaces");

	t = 0x41000042; /* 'A' 0x00 0x00 'B' */
	ostype_to_str(t, out);
	CHECK(strcmp(out, "A??B") == 0, "non-printable bytes become ?");

	t = 0x01020304;
	ostype_to_str(t, out);
	CHECK(strcmp(out, "????") == 0, "all four control bytes -> ?");
}

int main(void)
{
	test_parse_args();
	test_glob_match();
	test_fmt_human();
	test_ostype();
	printf("%d passed, %d failed\n", g_pass, g_fail);
	return g_fail == 0 ? 0 : 1;
}
