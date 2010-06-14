/*
 * Written by J.T. Conklin <jtc@NetBSD.org>.
 * Public domain.
 */

#include <driver.h>
#include <sys/types.h>
#include <sys/endian.h>

#undef htonl
#undef htons
#undef ntohl
#undef ntohs

#if BYTE_ORDER == LITTLE_ENDIAN
uint32_t
htonl(uint32_t x)
{

	u_char *s = (u_char *)&x;
	return (uint32_t)(s[0] << 24 | s[1] << 16 | s[2] << 8 | s[3]);
}

uint16_t
htons(uint16_t x)
{

	u_char *s = (u_char *) &x;
	return (uint16_t)(s[0] << 8 | s[1]);
}

uint32_t
ntohl(uint32_t x)
{

	u_char *s = (u_char *)&x;
	return (uint32_t)(s[0] << 24 | s[1] << 16 | s[2] << 8 | s[3]);
}

uint16_t
ntohs(uint16_t x)
{

	u_char *s = (u_char *) &x;
	return (uint16_t)(s[0] << 8 | s[1]);
}
#endif
