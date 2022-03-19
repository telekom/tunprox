#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <stdint.h>
#include <ctype.h>


#include "errors.h"
#include "slog.h"
#include "textop.h"
#include "fileop.h"


#include "ipool.h"


#define FORMAT_INT	0
#define FORMAT_IP		1


static int s_parse_num (uint64_t*, char*);
static int s_parse_mask (uint64_t*, char*, int, int);
static void s_print_int (uint64_t);
static void s_print_hex (uint64_t);
static void s_print_ip (uint32_t);


int
ipool_add (pool, desc, flags)
	struct ipool	*pool;
	char				*desc;
	int				flags;
{
	char		*sr, *sb, *se, *sm;
	uint64_t	b, b2, e, m, m2, m3;
	int		f, ret, i, nb;

	if (!pool || !desc) return RERR_PARAM;
	while ((sr=top_getfield (&desc, ",; \t\n", 0))) {
		sb = top_getfield (&sr, "-", 0);
		se = top_getfield (&sr, NULL, 0);
		sr = sb;
		sb = top_getfield (&sr, "/", 0);
		sm = top_getfield (&sr, NULL, 0);
		if (!sb) continue;
		f = s_parse_num (&b, sb);
		if (!RERR_ISOK(f)) return f;
		if (se) {
			f = s_parse_num (&e, se);
			if (!RERR_ISOK(f)) return f;
			ret = ipool_add_range (pool, b, e);
			if (!RERR_ISOK(ret)) return ret;
		} else if (sm) {
			f = s_parse_mask (&m, sm, f, flags);
			if (!RERR_ISOK(f)) return f;
			/* find num of lower bits */
			for (m2=0, i=0; m&1; i++, m>>=1, m2=(m2<<1)|1);
			m <<= i;
			nb = numbits64 (m);
			if (nb > 16) {
				/* two many bits */
				return RERR_TOO_LONG;
			}
			for (i=0; i<(1<<nb); i++) {
				m3 = distributebits64 (i, m);
				b2 = (b & ~m) | m3;
				e = b2 | m2;
				ret = ipool_add_range (pool, b2, e);
				if (!RERR_ISOK(ret)) return ret;
			}
		} else {
			ret = ipool_add_range (pool, b, b);
			if (!RERR_ISOK(ret)) return ret;
		}
	}
	return RERR_OK;
}


int
ipool_addfile (pool, file, flags)
	struct ipool	*pool;
	const char		*file;
	int				flags;
{
	char	*buf;
	int	ret;

	if (!file) return RERR_PARAM;
	if (!strcmp (file, "-")) {
		ret = fop_fdread (0, &buf);
	} else {
		ret = fop_fnread (file, &buf);
	}
	if (!RERR_ISOK(ret)) return ret;
	if (!buf) return RERR_INTERNAL;
	ret = ipool_add (pool, buf, flags);
	free (buf);
	return ret;
}

int
ipool_add_range (pool, b, e)
	struct ipool	*pool;
	uint64_t			b, e;
{
	struct ipool_range	*p, *q;
	int					i;

	if (!pool) return RERR_PARAM;
	if (e < b) return RERR_OUTOFRANGE;
	for (i=0, p=pool->list; i<pool->len; p++, i++) {
		if (p->b > 0 && e < p->b-1) continue;
		if ((p->e+1 > 0) && b > p->e+1) continue;
		if (b < p->b) p->b = b;	/* extend range to the beginning */
		if (e <= p->e) return RERR_OK;	/* already covered */
		p->e = e; /* extend range to the end */

		/* now check following ranges - maybe united */
		for (q=p+1,i++; i<pool->len && e>q->e; q++, i++);
		if (i<pool->len && e>=q->b-1) {
			p->e = q->e;
		} else {
			q--; i--;
		}
		if (q > p) {
			/* remove all inbetween */
			for (p++, q++, i++; i<pool->len; p++, q++, i++) *p = *q;
			pool->len = p-pool->list;
		}
		/* done */
		return RERR_OK;
	}
	/* we need to add a new range */
	p = realloc (pool->list, (pool->len+1)*sizeof (struct ipool_range));
	if (!p) return RERR_NOMEM;
	pool->list = p;
	/* find position */
	for (i=0, p=pool->list; i<pool->len && b > p->b; p++, i++);
	if (i < pool->len) {
		memmove (p+1, p, (pool->len-i)*sizeof (struct ipool_range));
	}
	*p = (struct ipool_range) { .b = b, .e = e };
	pool->len++;
	return RERR_OK;
}


/* subtract function */
int
ipool_subtract_range (pool, b, e)
	struct ipool	*pool;
	uint64_t			b, e;
{
	struct ipool_range	*p, *q;
	int					i;
	uint64_t				e2;

	if (!pool) return RERR_PARAM;
	if (e < b) return RERR_OUTOFRANGE;
	for (i=0, p=pool->list; i<pool->len; p++, i++) {
		if (e < p->b) continue;
		if (b > p->e) continue;
		/* first case fully inside */
		if (b > p->b && e < p->e) {
			/* we need to split the range */
			e2 = p->e;
			p->e = b-1;
			b = e+1;
			e = e2;
			return ipool_add_range (pool, b, e);
		}
		/* second case beginning before and end inside */
		if (e < p->e) {
			/* just modify the beginning */
			p->b = e+1;
			return RERR_OK;
		}
		/* third case beginning inside end after */
		if (b > p->b) {
			/* modify end */
			p->e = b-1;
			/* we still might modify next ranges */
			p++; i++;
			if (i >= pool->len) {
				/* no more ranges - ready */
				return RERR_OK;
			}
		}
		/* forth case eat up whole range */
		for (q=p; i<pool->len; i++, q++) {
			/* start with p, because might be incremented in third case */
			if (e < q->e) break;
		}
		if (i>=pool->len) {
			/* just resets pool length */
			pool->len = p - pool->list;
			/* done */
			return RERR_OK;
		}
		memmove (p, q, (pool->len - (q - pool->list)) * sizeof (struct ipool_range));
		pool->len -= (q-p);
		/* now p still might overlap with range */
		if (e >= p->b) p->b = e+1;
		/* done */
		return RERR_OK;
	}
	/* if we come here - range is not in pool - so nothing to be done */
	return RERR_OK;
}

int
ipool_subtract_pool (pool, subpool)
	struct ipool	*pool, *subpool;
{
	int	i, ret;

	if (!pool) return RERR_PARAM;
	if (!subpool) return RERR_OK;
	for (i=0; i<subpool->len; i++) {
		ret = ipool_subtract_range (pool, subpool->list[i].b, subpool->list[i].e);
		if (!RERR_ISOK(ret)) return ret;
	}
	return RERR_OK;
}


int
ipool_choose (out, pool, num)
	struct ipool	*pool;
	uint64_t			*out;
	int				num;
{
	struct ipool_range	*p;
	int					i;
	uint64_t				b, e;

	if (!pool || !out) return RERR_PARAM;
	*out = 0;
	if (num == 0) return RERR_OUTOFRANGE;
	for (i=0, p=pool->list; i<pool->len; i++, p++) {
		if (p->e - p->b + 1 >= (unsigned)num) break;
	}
	if (i>=pool->len) return RERR_NOT_FOUND;
	b = p->b;
	e = b + num - 1;
	*out = b;
	return ipool_subtract_range (pool, b, e);
}

int
ipool_print (pool, flags)
	struct ipool	*pool;
	int				flags;
{
	int	i, has=0;

	if (!pool) return RERR_PARAM;
	for (i=0; i<pool->len; i++) {
		if (pool->list[i].e == pool->list[i].b) {
			if (has) printf (", ");
			ipool_print_num (pool->list[i].b, flags);
		} else {
			if (has) printf (", ");
			ipool_print_num (pool->list[i].b, flags);
			printf ("-");
			ipool_print_num (pool->list[i].e, flags);
		}
		has=1;
	}
	if (has) printf ("\n");
	return RERR_OK;
}

void
ipool_print_num (num, flags)
	uint64_t	num;
	int		flags;
{
	if (flags & IPOOL_F_PRT_IP) {
		s_print_ip ((uint32_t)num);
	} else if (flags & IPOOL_F_PRT_HEX) {
		s_print_hex (num);
	} else {
		s_print_int (num);
	}
}






/* *******************
 * static functions
 * *******************/

static
void
s_print_int (num)
	uint64_t	num;
{
	printf ("%Lu", (unsigned long long)num);
}

static
void
s_print_hex (num)
	uint64_t	num;
{
	printf ("0x%Lx", (unsigned long long)num);
}

static
void
s_print_ip (ip)
	uint32_t	ip;
{
	printf ("%u.%u.%u.%u", ip>>24, (ip>>16)&0xff, (ip>>8)&0xff, ip&0xff);
}

static
int
s_parse_num (out, str)
	uint64_t	*out;
	char		*str;
{
	int		f, dot=0, a, b, c, d;
	char		*s;
	uint64_t	num;
	int		base=10;

	if (!str || !out) return RERR_PARAM;
	if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X')) {
		base = 16;
		f = FORMAT_INT;
		str += 2;
		for (s=str; *s; s++) {
			if (!ishex (*s)) return RERR_INVALID_VAL;
		}
	} else {
		for (s=str; *s; s++) {
			if (isdigit (*s)) continue;
			if (*s == '.') dot++;
			else break;
		}
		if (*s) return RERR_INVALID_VAL;
	}
	if (dot == 3) {
		f = FORMAT_IP;
	} else if (dot > 0) {
		return RERR_INVALID_VAL;
	} else {
		f = FORMAT_INT;
		if (base != 16 && *str == '0' && str[1]) {
			base = 8;
			str++;
		}
	}
	if (f == FORMAT_IP) {
		if (sscanf (str, "%d.%d.%d.%d", &a, &b, &c, &d) != 4) return RERR_INVALID_VAL;
		num = (((uint64_t)a) << 24) | (b << 16) | (c << 8) | d;
	} else {
		num = strtoull (str, NULL, base);
	}
	*out = num;
	return f;
}


static
int
s_parse_mask (out, d, format, flags)
	uint64_t	*out;
	char		*d;
	int		format, flags;
{
	int		f;
	uint64_t	num;
	uint32_t	num32;

	f = s_parse_num (&num, d);
	if (!RERR_ISOK(f)) return f;
	if (format == FORMAT_IP) {
		if (f == FORMAT_INT) {
			num32 = 0xffffffffUL << (32-num);
		} else {
			num32 = num;
		}
		if (!(flags & IPOOL_F_MASK_REGULAR)) {
			num32 = ~num32;
		}
		num = num32;
	} else {
		if (flags & IPOOL_F_MASK_INVERT) {
			num = ~num;
		}
	}
	*out = num;
	return f;
}



















/*
 * Overrides for XEmacs and vim so that we get a uniform tabbing style.
 * XEmacs/vim will notice this stuff at the end of the file and automatically
 * adjust the settings for this buffer only.  This must remain at the end
 * of the file.
 * ---------------------------------------------------------------------------
 * Local variables:
 * c-indent-level: 3
 * c-basic-offset: 3
 * tab-width: 3
 * End:
 * vim:tw=0:ts=3:wm=0:
 */
