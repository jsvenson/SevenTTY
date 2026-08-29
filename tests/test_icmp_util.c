/* tests/test_icmp_util.c -- host-side unit tests for icmp_util.c.
 * Plain C89, compiled with cc. Returns nonzero if any test fails.
 */
#include <stdio.h>
#include <string.h>
#include "icmp_util.h"

static int g_pass = 0;
static int g_fail = 0;

#define CHECK(cond, msg) do { \
	if (cond) { g_pass++; } \
	else { g_fail++; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, msg); } \
} while (0)

static void test_checksum(void)
{
	unsigned char zero[8];
	unsigned char odd[5];
	unsigned char req[12];

	memset(zero, 0, sizeof(zero));
	memset(odd, 0, sizeof(odd));
	memset(req, 0, sizeof(req));

	/* echo request: type 8, code 0, cksum 0, id 0x1234, seq 1, ts 0 */
	req[0] = 8;
	req[4] = 0x12; req[5] = 0x34;
	req[6] = 0x00; req[7] = 0x01;
	CHECK(icmp_checksum(req, 12) == 0xE5CA, "echo request vector -> 0xE5CA");

	CHECK(icmp_checksum(zero, 8) == 0xFFFF, "all-zero buffer -> 0xFFFF");

	odd[0] = 0x01; odd[1] = 0x02; odd[2] = 0x03; odd[3] = 0x04; odd[4] = 0x05;
	CHECK(icmp_checksum(odd, 5) == 0xF6F9, "odd-length -> 0xF6F9");
}

static void test_build(void)
{
	unsigned char buf[64];
	int len;

	len = icmp_build_echo_request(buf, 4, 0x1234, 1, 0);
	CHECK(len == 12, "build len == 12 (8 hdr + 4 payload)");
	CHECK(buf[0] == 8, "type == echo request");
	CHECK(buf[1] == 0, "code == 0");
	CHECK(buf[2] == 0xE5 && buf[3] == 0xCA, "checksum bytes E5 CA");
	CHECK(buf[4] == 0x12 && buf[5] == 0x34, "id bytes");
	CHECK(buf[6] == 0x00 && buf[7] == 0x01, "seq bytes");
	CHECK(buf[8] == 0 && buf[9] == 0 && buf[10] == 0 && buf[11] == 0, "timestamp 0");

	len = icmp_build_echo_request(buf, 4, 0x1234, 1, 0x01020304UL);
	CHECK(buf[8] == 0x01 && buf[9] == 0x02 && buf[10] == 0x03 && buf[11] == 0x04,
	      "timestamp big-endian");
}

static void test_parse(void)
{
	/* echo reply with a 20-byte IP header */
	unsigned char pkt[32];
	enum icmp_result r;
	struct icmp_reply_info info;

	memset(pkt, 0, sizeof(pkt));
	pkt[0] = 0x45;          /* v4, ihl 5 */
	pkt[8] = 0x40;          /* ttl = 64 */
	pkt[12] = 0xC0; pkt[13] = 0xA8; pkt[14] = 0x01; pkt[15] = 0x05; /* src 192.168.1.5 */
	/* ICMP echo reply at offset 20 */
	pkt[20] = 0;            /* type 0 */
	pkt[24] = 0x12; pkt[25] = 0x34;  /* id */
	pkt[26] = 0x00; pkt[27] = 0x01;  /* seq 1 */
	pkt[28] = 0x00; pkt[29] = 0x00; pkt[30] = 0x03; pkt[31] = 0xE8; /* ts 1000 */

	r = icmp_parse_reply(pkt, 32, 0x1234, 1, 1050, &info);
	CHECK(r == ICMP_ECHO_REPLY, "echo reply matched");
	CHECK(info.ttl == 64, "ttl read from IP header");
	CHECK(info.seq == 1, "seq echoed");
	CHECK(info.rtt_ms == 50, "rtt = now - sent ts");
	CHECK(strcmp(info.source_ip, "192.168.1.5") == 0, "source ip formatted");

	/* header-stripped form (defensive: no IP header present) */
	{
		unsigned char bare[12];
		memcpy(bare, pkt + 20, 12);
		r = icmp_parse_reply(bare, 12, 0x1234, 1, 1050, &info);
		CHECK(r == ICMP_ECHO_REPLY, "bare ICMP reply parsed");
		CHECK(info.rtt_ms == 50, "bare reply rtt");
		CHECK(info.ttl == -1, "bare reply has no ttl");
	}

	/* wrong id -> not mine */
	r = icmp_parse_reply(pkt, 32, 0x9999, 1, 1050, &info);
	CHECK(r == ICMP_NOT_MINE, "wrong id ignored");

	/* wrong seq -> not mine */
	r = icmp_parse_reply(pkt, 32, 0x1234, 99, 1050, &info);
	CHECK(r == ICMP_NOT_MINE, "wrong seq ignored");
}

static void test_parse_errors(void)
{
	/* destination unreachable (type 3, code 1) embedding our echo request */
	unsigned char pkt[36];
	enum icmp_result r;
	struct icmp_reply_info info;

	memset(pkt, 0, sizeof(pkt));
	pkt[0] = 3; pkt[1] = 1;          /* unreachable, host */
	pkt[8] = 0x45;                   /* embedded original IP header */
	pkt[28] = 8;                     /* embedded original ICMP type */
	pkt[32] = 0x12; pkt[33] = 0x34;  /* embedded original id */
	pkt[34] = 0x00; pkt[35] = 0x01;  /* embedded original seq */

	r = icmp_parse_reply(pkt, 36, 0x1234, 1, 0, &info);
	CHECK(r == ICMP_UNREACHABLE, "type 3 -> unreachable");
	CHECK(info.seq == 1, "error seq from embedded original header");

	/* error packet with a distinct embedded seq (0x0005): info->seq must
	   report the embedded original seq, not the error packet's own bytes
	   (RFC 792 "unused", always 0) */
	pkt[34] = 0x00; pkt[35] = 0x05;
	r = icmp_parse_reply(pkt, 36, 0x1234, 5, 0, &info);
	CHECK(r == ICMP_UNREACHABLE, "type 3 (distinct embedded seq) -> unreachable");
	CHECK(info.seq == 5, "error seq == embedded original seq");
	pkt[34] = 0x00; pkt[35] = 0x01;

	/* embedded id mismatch -> not mine */
	r = icmp_parse_reply(pkt, 36, 0x9999, 1, 0, &info);
	CHECK(r == ICMP_NOT_MINE, "error with foreign embedded id ignored");

	/* time exceeded (type 11) */
	pkt[0] = 11; pkt[1] = 0;
	r = icmp_parse_reply(pkt, 36, 0x1234, 1, 0, &info);
	CHECK(r == ICMP_TTL_EXCEEDED, "type 11 -> ttl exceeded");

	/* too short -> bad */
	r = icmp_parse_reply(pkt, 3, 0x1234, 1, 0, &info);
	CHECK(r == ICMP_BAD, "short buffer -> bad");

	/* garbage first byte (not v4) with len 4 -> bad */
	{
		unsigned char junk[4];
		memset(junk, 0x99, sizeof(junk));
		r = icmp_parse_reply(junk, 4, 0x1234, 1, 0, &info);
		CHECK(r == ICMP_BAD, "garbage -> bad");
	}
}

int main(void)
{
	test_checksum();
	test_build();
	test_parse();
	test_parse_errors();
	printf("%d passed, %d failed\n", g_pass, g_fail);
	return g_fail == 0 ? 0 : 1;
}
