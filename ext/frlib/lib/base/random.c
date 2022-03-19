/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 2.0
 *
 *  This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 *  file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for more details governing rights and limitations under the License.
 *
 * The Original Code is part of the frlib.
 *
 * The Initial Developer of the Original Code is
 * Frank Reker <frank@reker.net>.
 * Portions created by the Initial Developer are Copyright (C) 2003-2014
 * by the Initial Developer. All Rights Reserved.
 *
 * ***** END LICENSE BLOCK ***** */




#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <ctype.h>
#include <errno.h>
#ifdef SunOS
extern int errno;
#endif


#ifdef HAVE_OPENSSL
#include <openssl/aes.h>
#endif
#include "dlcrypto.h"

#include "random.h"
#include "config.h"
#include "slog.h"
#include "errors.h"




static AES_KEY			aes_key;
static unsigned char	seed[16];
static int				is_init = 0;
static int 				config_read = 0;
static int				c_userandfile = 0;
static const char		*c_randfile;

static int get_seedvalue (unsigned char *, int);
static int write_seed (int);
static int read_config();

int
frnd_init ()
{
	unsigned char	ukey[16];
	unsigned char	tmp[32];
	int				ret;

	if (!config_read) read_config();
	ret = get_seedvalue (tmp, 32);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error reading seed value: %s", rerr_getstr3(ret));
		return ret;
	}
	memcpy (ukey, tmp, 16);
	memcpy (seed, tmp+16, 16);

	ret = EXTCALL(AES_set_encrypt_key) (ukey, 128, &aes_key);
	if (ret == -2) return 0;
	is_init = 1;
	if (c_userandfile) write_seed (32);
	return 1;
}


uint32_t
frnd_get32 ()
{
	uint32_t			r;
	uint32_t			*arr;
	int				ret;
	unsigned char	tmp[16];

	if (!is_init) frnd_init();
	ret = EXTCALL(AES_encrypt) (seed, tmp, &aes_key);
	if (ret != -2) {
		memcpy (seed, tmp, 16);
	}
	arr = (uint32_t*)(void*)seed;
	r = arr[0];
	if (arr[1]) r %= arr[1];
#if 0
	if (arr[2]) r %= arr[2];
	if (arr[2]) r %= arr[3];
#endif
	return r;
}

void
frnd_getbytes (buf, len)
	char		*buf;
	size_t	len;
{
	uint32_t	r,i;
	char		*s;

	if (!buf) return;
	for (i=0; i<len/4; i++) {
		((uint32_t*)(void*)buf)[i] = frnd_get32();
	}
	buf+=4*(len/4);
	r = frnd_get32();
	s = (char*)(void*)&r;
	for (i=0; i<len%4; i++) {
		buf[i] = s[i];
	}
	return;
}


#define MAX (~((uint32_t)0))
#define GET_LIMIT(num) (MAX-MAX%(num))

uint32_t
frnd_get32limit (limit)
	uint32_t	limit;
{
	uint32_t	r;
	uint32_t	max;

	max = GET_LIMIT (limit);
	while ((r=frnd_get32()) >= max);
	r%=limit;
	return r;
}


uint32_t
frnd_get32range (min, max)
	uint32_t	min, max;
{
	uint32_t	r;

	r = frnd_get32limit (max-min+1);
	r += min;
	return r;
}


char
frnd_getletter ()
{
	uint32_t	r;

	r = frnd_get32limit (52);
	if (r < 26) return 'A' + r;
	return 'a' + (r-26);
}

char
frnd_getalnum ()
{
	uint32_t	r;

	r = frnd_get32limit (62);
	if (r < 26) return 'A' + r;
	if (r < 52) return 'a' + (r-26);
	return '0' + (r-52);
}

int
frnd_ishuffle (arr, num)
	int		*arr;
	uint32_t	num;
{
	uint32_t	i, r;
	int		s;

	if (!arr || !num) return RERR_PARAM;
	if (num == 1) return RERR_OK;
	for (i=0; i<num; i++) {
		r = frnd_get32limit (num);
		s = arr[i];
		arr[i] = arr[r];
		arr[r] = s;
	}
	return RERR_OK;
}



static
int
get_seedvalue (data, len)
	unsigned char	*data;
	int				len;
{
	FILE			*f;
	const char	*myrandfile;
	int			rlen;

	if (!data || len < 0) return 0;
	if (len == 0) return 1;
	if (!config_read) read_config();
	if (c_userandfile) {
		myrandfile = c_randfile;
	} else {
		myrandfile = "/dev/urandom";
	}
	f = fopen (myrandfile, "r");
	if (!f) {
		FRLOGF (LOG_ERR, "cannot open randfile: %s\n", 
								rerr_getstr3(RERR_SYSTEM));
		return RERR_SYSTEM;
	}
	rlen = fread (data, 1, len, f);
	fclose (f);
	if (rlen != len) {
		FRLOGF (LOG_WARN, "got only %d from %d bytes of random data", rlen, len);
	}

	return RERR_OK;
}


static
int
read_config ()
{
	const char	*s;

	cf_begin_read ();
	c_randfile = cf_getval ("randfile");
	c_userandfile = c_randfile ? 1 : 0;
	s = cf_getval ("use_randfile");
	if (s && !cf_isyes (s)) c_userandfile = 0;
	config_read = 1;
	cf_end_read_cb (&read_config);
	return 1;
}



static
int
write_seed (len)
	int		len;
{
	int			i;
	FILE			*f;
	uint32_t	r;

	if (!config_read) read_config();
	f = fopen (c_randfile, "w");
	if (!f) {
		FRLOGF (LOG_ERR, "error opening random file for writing: %s",
					rerr_getstr3(RERR_SYSTEM));
		return RERR_SYSTEM;
	}
	frnd_get32();
	for (i=0; i<(len+3)/4; i++) {
		r = frnd_get32();
		fwrite (&r, 4, 1, f);
	}
	fclose (f);
	return RERR_OK;
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
