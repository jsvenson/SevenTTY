/* icmp_util.c -- dependency-free ICMP wire logic for the ping command.
 * RFC 792 wire format. Byte-order-agnostic: all fields are assembled /
 * parsed as explicit big-endian bytes, so the host (little-endian) and the
 * 68K Mac (big-endian) agree.
 */
#include "icmp_util.h"

/* One's-complement checksum over the message bytes (RFC 792 / RFC 1071).
 * Accumulates 16-bit words as big-endian byte pairs so the result is the
 * same on any host endianness. Returns the network-order checksum value. */
unsigned short icmp_checksum(const void* buf, int len)
{
	const unsigned char* p = (const unsigned char*)buf;
	unsigned long sum = 0;
	int i;

	for (i = 0; i + 1 < len; i += 2)
		sum += ((unsigned long)p[i] << 8) | (unsigned long)p[i + 1];
	if (len & 1)
		sum += (unsigned long)p[len - 1] << 8;
	while (sum >> 16)
		sum = (sum & 0xFFFF) + (sum >> 16);
	return (unsigned short)(~sum & 0xFFFF);
}

/* Build an ICMP echo request into buf: type 8, code 0, checksum, id, seq,
 * then a payload whose first 4 bytes are the send timestamp (big-endian ms).
 * payload_size must be >= 4 (timestamp needs 4 bytes). Returns total length. */
int icmp_build_echo_request(unsigned char* buf, int payload_size,
                            unsigned short id, unsigned short seq,
                            unsigned long timestamp_ms)
{
	unsigned short ck;
	int total = 8 + payload_size;
	int i;

	buf[0] = 8;                /* type: echo request */
	buf[1] = 0;                /* code */
	buf[2] = 0; buf[3] = 0;    /* checksum placeholder */
	buf[4] = (unsigned char)(id >> 8);
	buf[5] = (unsigned char)(id & 0xFF);
	buf[6] = (unsigned char)(seq >> 8);
	buf[7] = (unsigned char)(seq & 0xFF);

	buf[8]  = (unsigned char)(timestamp_ms >> 24);
	buf[9]  = (unsigned char)(timestamp_ms >> 16);
	buf[10] = (unsigned char)(timestamp_ms >> 8);
	buf[11] = (unsigned char)(timestamp_ms & 0xFF);
	for (i = 12; i < total; i++)
		buf[i] = 0;

	ck = icmp_checksum(buf, total);
	buf[2] = (unsigned char)(ck >> 8);
	buf[3] = (unsigned char)(ck & 0xFF);
	return total;
}

static void ip4_to_str(const unsigned char* ip, char* out)
{
	int o = 0;
	int i;
	for (i = 0; i < 4; i++)
	{
		unsigned int v = ip[i];
		if (v >= 100) out[o++] = (char)('0' + v / 100);
		if (v >= 10)  out[o++] = (char)('0' + (v / 10) % 10);
		out[o++] = (char)('0' + v % 10);
		if (i < 3) out[o++] = '.';
	}
	out[o] = '\0';
}

/* Parse a received datagram. Defensively skips an IP header if present (the
 * docs disagree on whether RawIP delivers it); matches echo replies and
 * ICMP errors against expect_id/expect_seq so other ping runs are ignored. */
enum icmp_result icmp_parse_reply(const unsigned char* buf, int len,
                                  unsigned short expect_id,
                                  unsigned short expect_seq,
                                  unsigned long now_ms,
                                  struct icmp_reply_info* info)
{
	const unsigned char* icmp;
	int icmp_len;
	int ihl = 0;
	unsigned char type;
	unsigned short id, seq;

	info->source_ip[0] = '\0';
	info->ttl = -1;
	info->seq = 0;
	info->rtt_ms = 0;

	/* Defensive IP-header skip: if the first byte is IPv4 with a sane IHL,
	   skip IHL*4 bytes; otherwise assume the buffer starts at the ICMP msg. */
	if (len >= 1 && (buf[0] >> 4) == 4)
	{
		ihl = (int)(buf[0] & 0x0F);
		if (ihl < 5) ihl = 5;
		if (ihl * 4 < len)
		{
			icmp = buf + ihl * 4;
			icmp_len = len - ihl * 4;
			info->ttl = buf[8];
			ip4_to_str(buf + 12, info->source_ip);
		}
		else
		{
			icmp = buf;
			icmp_len = len;
		}
	}
	else
	{
		icmp = buf;
		icmp_len = len;
	}

	if (icmp_len < 8)
		return ICMP_BAD;

	type = icmp[0];
	id = (unsigned short)((icmp[4] << 8) | icmp[5]);
	seq = (unsigned short)((icmp[6] << 8) | icmp[7]);

	if (type == 0) /* echo reply */
	{
		if (id != expect_id || seq != expect_seq)
			return ICMP_NOT_MINE;
		info->seq = seq;
		if (icmp_len >= 12)
		{
			unsigned long sent =
				((unsigned long)icmp[8] << 24) |
				((unsigned long)icmp[9] << 16) |
				((unsigned long)icmp[10] << 8) |
				(unsigned long)icmp[11];
			info->rtt_ms = now_ms - sent;
		}
		return ICMP_ECHO_REPLY;
	}

	if (type == 3 || type == 11) /* destination unreachable / time exceeded */
	{
		/* ICMP error body embeds the original IP header + original ICMP
		   header. The original ICMP header starts at offset 8 + 20 = 28;
		   its id at 32, seq at 34. If present and it is not ours, ignore. */
		if (icmp_len >= 36)
		{
			unsigned short oid =
				(unsigned short)((icmp[32] << 8) | icmp[33]);
			unsigned short oseq =
				(unsigned short)((icmp[34] << 8) | icmp[35]);
			if (oid != expect_id || oseq != expect_seq)
				return ICMP_NOT_MINE;
			info->seq = oseq;
		}
		else
			info->seq = seq;
		return (type == 3) ? ICMP_UNREACHABLE : ICMP_TTL_EXCEEDED;
	}

	/* any other ICMP type is not ours */
	return ICMP_NOT_MINE;
}
