/* icmp_util.h -- dependency-free pure-logic helpers for the ICMP ping command.
 * Shared by shell.c and the host-side test harness. No Mac Toolbox / OT calls.
 */
#ifndef ICMP_UTIL_H
#define ICMP_UTIL_H

enum icmp_result {
	ICMP_ECHO_REPLY,    /* echo reply matching expect_id + expect_seq */
	ICMP_UNREACHABLE,   /* destination unreachable (type 3) */
	ICMP_TTL_EXCEEDED,  /* time exceeded (type 11) */
	ICMP_NOT_MINE,      /* reply/error for a different ping run */
	ICMP_BAD            /* malformed / too short */
};

struct icmp_reply_info {
	char source_ip[16];   /* dotted-quad source, "" if no IP header */
	int ttl;              /* IP TTL, or -1 if no IP header present */
	unsigned short seq;   /* echo reply sequence */
	unsigned long rtt_ms; /* round-trip time (now_ms - echoed send ts) */
};

unsigned short icmp_checksum(const void* buf, int len);
int icmp_build_echo_request(unsigned char* buf, int payload_size,
                            unsigned short id, unsigned short seq,
                            unsigned long timestamp_ms);
enum icmp_result icmp_parse_reply(const unsigned char* buf, int len,
                                  unsigned short expect_id,
                                  unsigned short expect_seq,
                                  unsigned long now_ms,
                                  struct icmp_reply_info* info);

#endif /* ICMP_UTIL_H */
