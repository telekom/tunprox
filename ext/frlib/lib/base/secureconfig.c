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
#include <unistd.h>
#include <stdint.h>
#include <sys/types.h>
#include <ctype.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <errno.h>
#ifdef SunOS
extern int errno;
#endif
#include <netinet/in.h>
#include <sys/mman.h>


#ifdef HAVE_OPENSSL
#include <openssl/aes.h>
#endif
#include "dlcrypto.h"


#include "textop.h"
#include "fileop.h"
#include "config.h"
#include "slog.h"
#include "errors.h"
#include "secureconfig.h"
#include "random.h"
#include "gzip.h"
#include "txtenc.h"
#include "md5sum.h"


static char MAGIC[] = { 0x56, 0xba, 0x0d, 0x99 };
static char VERSION[] = { 0x01, 0x01 };


static char	*secure_cf = NULL;
static int config_read = 0;
static int read_config();
static int get_key (AES_KEY*, const char*);
static int get_dkey (AES_KEY*, const char*);
static int do_delete_var (const char *, char *);
static int check_validity (const char *, size_t);
static void create_magic_block (char*);
static void create_clear_magic_block (char*);
static int is_bin_scf (const char*, int);
static int get_bin_scf (const char*, int, char**, int*, int);
static int is_clrscf (const char*, int);
#if 0
static int scf_mymlock (void*, size_t);
static int scf_mymunlock (void*, size_t);
#endif


static struct cf_cvlist	scv_list = CF_CVL_NULL;
static int	have_constscf = 0;

#define ROUND_UP(len,bs)	((len)+((len)%(bs)!=0)*((bs)-(len)%(bs)))
#define MIN(a,b)				((a)<(b)?(a):(b))



int
scf_getfile (filename, buf, len)
	const char	*filename;
	char			**buf;
	int			*len;
{
	FILE	*f;

	if (!buf || !len) return RERR_PARAM;
	CF_MAY_READ;
	if (!filename) filename = secure_cf;
	if (!filename) return RERR_NO_CRYPT_CF;
	if (!strcmp (filename, "-")) {
		f = stdin;
	} else {
		f = fopen (filename, "r");
		if (!f) {
			FRLOGF (LOG_ERR, "cannot open encrypted config file >>%s<<: %s",
						filename, rerr_getstr3(RERR_SYSTEM));
			return RERR_SYSTEM;
		}
	}
	*buf = fop_read_file2 (f, len);
	if (f != stdin) fclose (f);
	if (!*buf) {
		FRLOGF (LOG_ERR, "error reading encrypted config file >>%s<<",
								filename);
		return RERR_SYSTEM;
	}
	return RERR_OK;
}


int
scf_askpass (msg, passwd, len)
	const char	*msg;
	char			*passwd;
	int			len;
{
	char	*pass;

	if (!passwd || !len || len <= 0) {
		return RERR_PARAM;
	}
	pass = getpass (msg?msg:"Enter password: ");
	if (!pass) {
		FRLOGF (LOG_ERR2, "error reading password: %s",
					rerr_getstr3(RERR_SYSTEM));
		return RERR_SYSTEM;
	}
	strncpy (passwd, pass, len-1);
	passwd[len-1] = 0;
	bzero (pass, strlen (pass));
	return RERR_OK;
}


int
scf_changepass (infile, outfile)
	const char	*infile, *outfile;
{
	char	oldpass[1024];
	char	newpass[1024];
	char	*ibuf, *obuf;
	int	len, ret;

	ret = scf_getfile (infile, &obuf, &len);
	if (!RERR_ISOK (ret)) return ret;
	ret = scf_askverifypass ("old password: ", oldpass, 1024, obuf, len);
	if (!RERR_ISOK (ret)) {
		free (obuf);
		return ret;
	}
	ret = scf_doubleaskpass (newpass, 1024);
	if (!RERR_ISOK (ret)) {
		free (obuf);
		return ret;
	}
	ibuf = obuf;
	ret = scf_decrypt (ibuf, len, &obuf, oldpass, 0);
	free (ibuf);
	if (!RERR_ISOK (ret)) return ret;
	ibuf = obuf;
	ret = scf_encrypt (ibuf, &obuf, &len, newpass);
	bzero (ibuf, strlen (ibuf));
	free (ibuf);
	if (!RERR_ISOK (ret)) return ret;
	ret = scf_writeout (outfile, obuf, len);
	free (obuf);
	if (!RERR_ISOK (ret)) return ret;

	return RERR_OK;
}


int
scf_askverifypass (msg, passwd, pwdlen, cf, cflen)
	const char	*msg, *cf;
	char			*passwd;
	int			pwdlen, cflen;
{
	int	i, ret;

	ret = check_validity (cf, cflen);
	if (!RERR_ISOK (ret)) return ret;
	for (i=0; i<4; i++) {
		ret = scf_askpass (msg, passwd, pwdlen);
		if (!RERR_ISOK (ret)) return ret;
		ret = scf_verifypass (cf, cflen, passwd, 0);
		if (RERR_ISOK (ret)) break;
		if (ret != RERR_INVALID_PWD) return ret;
		switch (i) {
		case 0:
			fprintf (stderr, "It seems you have misstyped the password, "
							"please give it another try\n");
			sleep (1);
			break;
		case 1:
			fprintf (stderr, "You should try harder to type correctly (Hint: "
							"check your CAPS-LOCK key)\n");
			sleep (2);
			break;
		case 2:
			fprintf (stderr, "Are you sure you know the password?\n");
			sleep (4);
			break;
		case 3:
			fprintf (stderr, "You are not an authorized person, keep away from "
							"this system!\n");
			sleep (8);
			return RERR_INVALID_PWD;
			break;
		}
	}
	return RERR_OK;
}


int
scf_doubleaskpass (passwd, len)
	char	*passwd;
	int	len;
{
	char	newpass1[1024];
	char	newpass2[1024];
	int	i, ret;
	
	for (i=0; i<4; i++) {
		ret = scf_askpass ("new password: ", newpass1, 1024);
		if (!RERR_ISOK (ret)) return ret;
		ret = scf_askpass ("repeat password: ", newpass2, 1024);
		if (!RERR_ISOK (ret)) return ret;
		if (!strcmp (newpass1, newpass2)) break;
		fprintf (stderr, "The passwords are not equal, try again...\n");
	}
	if (i==4) return RERR_INVALID_PWD;
	bzero (passwd, len);
	if (len > 1024) len = 1024;
	strncpy (passwd, newpass1, len-1);
	passwd[len-1] = 0;
	return RERR_OK;
}


static
int
check_validity (cf, len)
	const char	*cf;
	size_t		len;
{
	unsigned char	first[AES_BLOCK_SIZE];
	char				*buf = NULL;
	const char		*pbuf;
	int				buflen, ret, isbin;

	if (!cf) return RERR_PARAM;

	isbin = is_bin_scf (cf, len);
	if (!isbin)	{
		ret = get_bin_scf (cf, len, &buf, &buflen, 1);
		if (!RERR_ISOK (ret)) return ret;
		pbuf = buf;
	} else {
		pbuf = cf;
		buflen = len;
	}
	if (buflen < AES_BLOCK_SIZE) {
		if (buf) free (buf);
		FRLOGF (LOG_ERR, "config file to short");
		return RERR_INVALID_SCF;
	}
	create_magic_block ((char*) first);
	if (memcmp (first, pbuf, 8)) {
		if (buf) free (buf);
		FRLOGF (LOG_ERR, "config file doesn't contain a valid super block");
		return RERR_INVALID_SCF;
	}
	if (buf) free (buf);

	return RERR_OK;
}


int
scf_verifypass (cf, len, passwd, flags)
	const char	*cf;
	const char	*passwd;
	size_t		len;
	int			flags;
{
	AES_KEY			key;
	unsigned char	out[AES_BLOCK_SIZE],
						ivec[AES_BLOCK_SIZE];
	char				*obuf = NULL;
	const char		*pbuf, *ibuf;
	int				ret, i, isbin, olen;
	int				strict;

	if (!cf || !passwd) return RERR_PARAM;
	strict = flags & SCF_FLAG_STRICT;
	isbin = is_bin_scf (cf, len);
	if (!isbin) {
		ret = get_bin_scf (cf, len, &obuf, &olen, flags);
		if (!RERR_ISOK (ret)) return ret;
		pbuf = obuf;
	} else {
		pbuf = cf;
		olen = len;
	}
	if (strict) {
		ret = check_validity (pbuf, olen);
		if (!RERR_ISOK (ret)) {
			if (obuf) free (obuf);
			return ret;
		}
	}
	ibuf = pbuf + AES_BLOCK_SIZE;
	
	get_dkey (&key, passwd);
	memcpy (ivec, ibuf, AES_BLOCK_SIZE);
	ibuf += AES_BLOCK_SIZE;
	EXTCALL(AES_cbc_encrypt) ((unsigned const char*)ibuf, out, AES_BLOCK_SIZE, &key, ivec, AES_DECRYPT);
	if (!isbin) free (obuf);
	for (i=0; i<AES_BLOCK_SIZE; i++) {
		if (out[i] != 0) {
			FRLOGF (LOG_NOTICE, "invalid password");
			return RERR_INVALID_PWD;
		}
	}
	return RERR_OK;
}


int
scf_writeout (filename, cf, len)
	const char	*filename, *cf;
	int			len;
{
	FILE	*f;

	CF_MAY_READ;
	if (!filename) filename = secure_cf;
	if (!filename) return RERR_INVALID_FILE;
	if (!strcmp (filename, "-")) {
		f = stdout;
	} else {
		f = fopen (filename, "w");
		if (!f) {
			FRLOGF (LOG_ERR, "error writing secure config file: %s",
									rerr_getstr3(RERR_SYSTEM));
			return RERR_INVALID_FILE;
		}
	}
	fwrite (cf, 1, len, f);
	if (f != stdout)
		fclose (f);
	return RERR_OK;
}

static
int
is_bin_scf (cf, len)
	const char	*cf;
	int			len;
{
	if (!cf || len < 20) return 1;
	if (!strncasecmp (cf, "-----BEGIN", 10)) return 0;
	return 1;
}


static
int
get_bin_scf (in, inlen, out, outlen, flags)
	const char	*in;
	char			**out;
	int			inlen, *outlen, flags;
{
	const char	*ptr, *s;
	char			*obuf;
	int			olen, ret;
	int			strict;

	if (!in || !out || !outlen) return RERR_PARAM;

	strict = flags & SCF_FLAG_STRICT;
	ptr = index (in, '\n');
	if (!ptr) {
		if (strict) {
			FRLOGF (LOG_ERR, "ascii version doesn't contain a valid header line");
			return RERR_INVALID_SCF;
		} else {
			FRLOGF (LOG_WARN, "ascii version doesn't contain a valid header line");
			ptr = in;
		}
	} else {
		ptr++;
	}
	s = index (ptr, '-');
	if (!s) {
		if (strict) {
			FRLOGF (LOG_ERR, "ascii version doesn't contain a valid footer line");
			return RERR_INVALID_SCF;
		} else {
			FRLOGF (LOG_WARN, "ascii version doesn't contain a valid footer line");
		}	
		inlen = strlen (ptr);
	} else {
		if (strict) {
			if (strncasecmp (s, "-----END SCF-----", 17) && 
					strncasecmp (s, "-----END CLRSCF-----", 20)) {
				FRLOGF (LOG_ERR, "ascii version doesn't contain a valid footer "
							"line");
				return RERR_INVALID_SCF;
			}
		}
		inlen = s - ptr;
	}
	ret = top_base64decode2 (ptr, inlen, &obuf, &olen);
	if (!RERR_ISOK (ret)) {
		FRLOGF (LOG_ERR, "error base64 decoding config file");
		return ret;
	}
	*out = obuf;
	*outlen = olen;

	return RERR_OK;
}


static
int
is_clrscf (cf, cflen)
	const char	*cf;
	int			cflen;
{
	if (!cf) return -1;
	if (is_bin_scf (cf, cflen)) {
		if (cflen < 16) return -1;
		return (cf[8] & 0x01) == 1;
	} else {
		if (cflen < 20) return -1;
		return !strncasecmp (cf, "-----BEGIN CLRSCF-----\n", 23);
	}
	return -1;
}



int
scf_decrypt (cf, len, out, passwd, flags)
	const char	*cf, *passwd;
	char			**out;
	size_t		len;
	int			flags;
{
	AES_KEY			key;
	int				olen, ret;
	unsigned char	ivec[AES_BLOCK_SIZE];
	char				*ibuf, *obuf;
	int				ilen, isbin, rlen;
	char				md5[16], smd5[16];
	int				strict;

	if (!cf || !passwd || !out) return RERR_PARAM;
	strict = flags & SCF_FLAG_STRICT;
	ibuf = (char*)cf;
	ilen = len;
	isbin = is_bin_scf (ibuf, ilen);
	if (!isbin) {
		ret = get_bin_scf (ibuf, ilen, &obuf, &olen, flags);
		if (!RERR_ISOK (ret)) return ret;
		ibuf = obuf;
		ilen = olen;
	}

	if (!is_clrscf (ibuf, ilen)) {
		ret = scf_verifypass (ibuf, ilen, passwd, flags);
		if (!RERR_ISOK (ret)) {
			if (!isbin) free (ibuf);
			return ret;
		}

		/* decrypt cf */
		memcpy (ivec, ibuf+2*AES_BLOCK_SIZE, AES_BLOCK_SIZE);
		get_dkey (&key, passwd);
		olen = ilen-3*AES_BLOCK_SIZE;
		obuf = malloc (olen+1);
		if (!obuf) {
			if (!isbin) free (ibuf);
			return RERR_NOMEM;
		}
		bzero (obuf, olen+1);
		EXTCALL(AES_cbc_encrypt) ((unsigned char*)ibuf+3*AES_BLOCK_SIZE, 
						(unsigned char*)obuf, olen, &key, ivec, 
						AES_DECRYPT);
		if (!isbin) free (ibuf);
		ibuf = obuf;
		ilen = olen;
	} else if (flags & SCF_FLAG_VSTRICT2) {
		if (cf != ibuf) {
			bzero (ibuf, ilen);
			free (ibuf);
		}
		return RERR_INVALID_SCF;
	}
	
	if (!is_bin_scf (ibuf, ilen)) {
		ret = get_bin_scf (ibuf, ilen, &obuf, &olen, flags);
		if (ibuf != cf) {
			bzero (ibuf, ilen);
			free (ibuf);
		}
		if (!RERR_ISOK (ret)) return ret;
		ibuf = obuf;
		ilen = olen;
	}
	/* check integrity */
	if (strict) {
		ret = check_validity (ibuf, ilen);
		if (!RERR_ISOK (ret)) {
			if (ibuf != cf) {
				bzero (ibuf, ilen);
				free (ibuf);
			}
			return ret;
		}
	}

	/* get md5 for later check */
	memcpy (smd5, ibuf+AES_BLOCK_SIZE, 16);

	/* get len */
	rlen = htonl (*(uint32_t*)(ibuf+AES_BLOCK_SIZE+16));

	/* gunzip cf */
	ibuf = obuf;
	ilen = olen;
	ret = gunzip (ibuf+AES_BLOCK_SIZE+20, rlen, &obuf, &olen);
	if (ibuf != cf) {
		bzero (ibuf, ilen);
		free (ibuf);
	}
	if (!RERR_ISOK (ret)) return ret;
	ilen = strlen (obuf);
	bzero (obuf+ilen, olen-ilen);
	olen = ilen;

	/* check md5 */
	if (strict) {
		md5sum (md5, obuf, olen, TENC_FMT_BIN);
		if (memcmp (md5, smd5, MIN(AES_BLOCK_SIZE,16))) {
			bzero (obuf, olen);
			free (obuf);
			return RERR_INVALID_SCF;
		}
	}

	/* finished return data */
	*out = obuf;
	return RERR_OK;
}
		
	




static
void
create_magic_block (first)
	char	*first;
{
	bzero (first, AES_BLOCK_SIZE);
	first[1] = MAGIC[0];
	first[2] = MAGIC[1];
	first[3] = MAGIC[2];
	first[4] = MAGIC[3];
	first[5] = VERSION[0];
	first[6] = VERSION[1];
	first[7] = (unsigned char) AES_BLOCK_SIZE;
	first[8] = 0;	/* flags */
	strcpy (first+12, "SCF");
}



static
void
create_clear_magic_block (first)
	char	*first;
{
	create_magic_block (first);
	first[8] |= 0x01;
}




int
scf_encrypt (cf, out, len, passwd)
	const char	*cf, *passwd;
	char			**out;
	int			*len;
{
	AES_KEY			key;
	unsigned char	ivec[AES_BLOCK_SIZE],
						first[AES_BLOCK_SIZE];
	char				*obuf, *ibuf;
	int				olen;
	int				ret, ilen;
	char				md5[16];

	if (!cf || !passwd || !out || !len) return RERR_PARAM;
	create_clear_magic_block ((char*)first);
	/* calculate md5 of cf */
	ret = md5sum (md5, cf, strlen (cf), TENC_FMT_BIN);
	if (!RERR_ISOK(ret)) return ret;
	/* gzip cf */
	ibuf = (char*)cf;
	ret = gzip (ibuf, strlen (ibuf), &obuf, &olen, -1);
	if (!RERR_ISOK (ret)) return ret;
	ibuf = obuf;
	ilen = olen;

	/* create clear scf (super-block; md5; len (4B); gzip(cf)) */
	olen = ilen + AES_BLOCK_SIZE + 16 + 4 + 1;
	obuf = malloc (olen);
	if (!obuf) {
		bzero (ibuf, ilen);
		free (ibuf);
		return RERR_NOMEM;
	}
	bzero (obuf, olen);
	memcpy (obuf, first, AES_BLOCK_SIZE);
	memcpy (obuf+AES_BLOCK_SIZE, md5, 16);
	*(uint32_t *)(void*)(obuf+AES_BLOCK_SIZE+16) = (uint32_t) htonl (ilen);
	memcpy (obuf+AES_BLOCK_SIZE+20, ibuf, ilen);
	
	/* base64 encode clear scf */
	ibuf = obuf;
	ilen = olen;
	ret = top_base64encode (ibuf, ilen, &obuf);
	bzero (ibuf, ilen);
	free (ibuf);

	/* create input for encrypting */
	ibuf = obuf;
	ilen = strlen (ibuf);
	olen = ilen + 45;
	olen = ROUND_UP (olen, AES_BLOCK_SIZE);
	olen += AES_BLOCK_SIZE;
	obuf = malloc (olen+1);
	if (!obuf) {
		bzero (ibuf, ilen);
		free (ibuf);
		return RERR_NOMEM;
	}
	bzero (obuf, olen+1);
	sprintf (obuf+AES_BLOCK_SIZE, "-----BEGIN CLRSCF-----\n%s"
					"-----END CLRSCF-----\n", ibuf);
	bzero (ibuf, ilen);
	free (ibuf);

	/* create output buffer and encrypt cf */
	create_magic_block ((char*)first);
	ibuf = obuf;
	ilen = olen;
	olen = ilen + 2*AES_BLOCK_SIZE;
	obuf = malloc (olen+1);
	if (!obuf) {
		bzero (ibuf, ilen);
		free (ibuf);
		return RERR_NOMEM;
	}
	bzero (obuf, olen+1);
	memcpy (obuf, first, AES_BLOCK_SIZE);
	frnd_getbytes ((char*)ivec, AES_BLOCK_SIZE);
	memcpy (obuf+AES_BLOCK_SIZE, ivec, AES_BLOCK_SIZE);
	get_key (&key, passwd);
	EXTCALL(AES_cbc_encrypt) ((unsigned char*)ibuf, (unsigned char*)obuf+2*AES_BLOCK_SIZE, 
					ilen, &key, ivec, AES_ENCRYPT);
	bzero (ibuf, ilen);
	free (ibuf);

	/* base64 encode output and append header and footer */
	ibuf = obuf;
	ilen = olen;
	ret = top_base64encode (ibuf, ilen, &obuf);
	free (ibuf);
	if (!RERR_ISOK (ret)) return ret;
	ibuf = obuf;
	ilen = strlen (ibuf);
	olen = ilen + 40;
	obuf = malloc (olen+1);
	if (!obuf) {
		free (ibuf);
		return RERR_NOMEM;
	}
	bzero (obuf, olen+1);
	sprintf (obuf, "-----BEGIN SCF-----\n%s-----END SCF-----\n", ibuf);
	free (ibuf);
	olen = strlen (obuf);

	/* finished - return the result */
	*out = obuf;
	if (len) (*len) = olen;

	return RERR_OK;
}

	



static
int
get_key (key, passwd)
	AES_KEY		*key;
	const char	*passwd;
{
	char		md5[16];

	if (!passwd || !key) return RERR_PARAM;
	md5sum (md5, passwd, strlen (passwd), TENC_FMT_BIN);
	EXTCALL(AES_set_encrypt_key) ((unsigned char*)md5, 128, &key);
	return RERR_OK;
}


static
int
get_dkey (key, passwd)
	AES_KEY		*key;
	const char	*passwd;
{
	char		md5[16];

	if (!passwd || !key) return RERR_PARAM;
	md5sum (md5, passwd, strlen (passwd), TENC_FMT_BIN);
	EXTCALL(AES_set_decrypt_key) ((unsigned char*)md5, 128, &key);
	return RERR_OK;
}





int
scf_readpass (fd, passwd, len)
	int	fd;
	char	*passwd;
	int	*len;		/* in and out parameter */
{
	if (!passwd || !len || fd < 0) return RERR_PARAM;
	*len = read (fd, passwd, *len);
	if (fd > 2) close (fd);
	if (*len < 0) {
		FRLOGF (LOG_ERR, "error reading passphrase: %s",
					rerr_getstr3(RERR_SYSTEM));
		return RERR_SYSTEM;
	}
	return RERR_OK;
}




int
scf_free ()
{
	cf_deregister_cv_list (&scv_list);
	cf_hfree_cvlist (&scv_list, 1);
	return 1;
}

static char		drowssap [1024];
static int		havepass = 0;

int
scf_read (passwd)
	const char	*passwd;
{
	char	*buf, *obuf;
	int	len, ret;

	if (!passwd && !havepass) return RERR_PARAM;
	if (!passwd) {
		passwd = drowssap;
	} else if (passwd != drowssap) {
		scf_forgetpass();
		strncpy (drowssap, passwd, sizeof(drowssap)-1);
		drowssap[sizeof(drowssap)-1]=0;
		havepass=1;
		passwd = drowssap;
	}
	scf_free ();
	ret = scf_getfile (NULL, &buf, &len);
	if (!RERR_ISOK (ret)) return ret;
	ret = scf_decrypt (buf, len, &obuf, passwd, 1);
	free (buf);
	if (!RERR_ISOK (ret)) return ret;
	ret = cf_parse (obuf, &scv_list);
	if (!RERR_ISOK (ret)) {
		scv_list = CF_CVL_NULL;
		free (obuf);
		return ret;
	}
	ret = cf_register_cv_list (&scv_list);
	if (!RERR_ISOK (ret)) {
		cf_hfree_cvlist (&scv_list, 1);
		scv_list = CF_CVL_NULL;
		return ret;
	}
	ret = cf_register_reread_callback (&scf_reread);
	if (!RERR_ISOK (ret)) {
		cf_deregister_cv_list (&scv_list);
		cf_hfree_cvlist (&scv_list, 1);
		scv_list = CF_CVL_NULL;
		return ret;
	}

	return RERR_OK;
}

int
scf_reread ()
{
	if (!havepass) return RERR_OK;
	return scf_read (NULL);
}


int
scf_fdread (fd)
	int		fd;
{
	char	*passwd = drowssap;
	int	ret, len=sizeof (drowssap);

	if (fd >= 0) {
		scf_forgetpass ();
		ret = scf_readpass (fd, passwd, &len);
		if (!RERR_ISOK (ret)) return ret;
		havepass = 1;
	}
	if (!scf_havefile()) return RERR_OK;
	if (fd < 0) {
		scf_forgetpass ();
		ret = scf_askverifypass2 ("Enter password: ", passwd, len);
		if (!RERR_ISOK (ret)) return ret;
	}
	ret = scf_read (passwd);
	if (!RERR_ISOK (ret)) return ret;
	return RERR_OK;
}


void
scf_forgetpass ()
{
	bzero (drowssap, sizeof (drowssap));
	havepass = 0;
}


const char *
scf_getpass ()
{
	if (!havepass) return NULL;
	return drowssap;
}



int
scf_askread ()
{
	return scf_fdread (-1);
}



int
scf_askverifypass2 (msg, passwd, len)
	const char	*msg;
	char			*passwd;
	int			len;
{
	char	*cf;
	int	ret, cflen;

	ret = scf_getfile (NULL, &cf, &cflen);
	if (!RERR_ISOK (ret)) return ret;
	ret = scf_askverifypass (msg, passwd, len, cf, cflen);
	free (cf);
	return ret;
}


int
scf_havefile ()
{
	CF_MAY_READ;
	return secure_cf != NULL;
}



int
scf_create (filename)
	const char	*filename;
{
	char	dummy[1], *obuf, passwd[1024];
	int	ret, olen;

	dummy[0] = 0;
	ret = scf_doubleaskpass (passwd, 1024);
	if (!RERR_ISOK (ret)) return ret;
	ret = scf_encrypt (dummy, &obuf, &olen, passwd);
	if (!RERR_ISOK (ret)) return ret;
	ret = scf_writeout (filename, obuf, olen);
	free (obuf);
	return ret;
}




int
scf_rmvar (infile, outfile, var)
	const char	*infile, *outfile, *var;
{
	char	*ibuf, *obuf;
	int	len, ret;
	char	passwd[1024];

	if (!var || !*var) return RERR_PARAM;
	ret = scf_getfile (infile, &obuf, &len);
	if (!RERR_ISOK (ret)) return ret;
	ret = scf_askverifypass ("Password: ", passwd, 1024, obuf, len);
	if (!RERR_ISOK (ret)) {
		free (obuf);
		return ret;
	}
	ibuf = obuf;
	ret = scf_decrypt (ibuf, len, &obuf, passwd, 0);
	free (ibuf);
	if (!RERR_ISOK (ret)) return ret;
	ret = do_delete_var (var, obuf);
	ibuf = obuf;
	ret = scf_encrypt (ibuf, &obuf, &len, passwd);
	bzero (ibuf, strlen (ibuf));
	free (ibuf);
	if (!RERR_ISOK (ret)) return ret;
	ret = scf_writeout (outfile, obuf, len);
	free (obuf);
	return ret;
}

static
int
do_delete_var (var, buf)
	const char	*var;
	char			*buf;
{
	char	*line, *s, *s2;
	int	found =1;

	if (!var || !*var || !buf) return RERR_PARAM;
	while (found) {
		line = buf;
		found = 0;
		while (1) {
			line = top_skipwhite (line);
			if (*line == '#') {
				line = index (line, '\n');
				if (!line) break;
				line++;
				continue;
			}
			s2 = index (line, '\n');
			s = index (line, '=');
			if (!s) break;
			if (s2 && s>s2) {
				line = s2+1;
				continue;
			}
			for (s--; s>line && isspace (*s); s--) {}; s++;
			if ((ssize_t)strlen (var) == s-line && !strncasecmp (line, var, s-line)) {
				found = 1;
				/* search end of entry */
				s2 = index (line, '=') + 1;
				s2 = top_skipwhite (s2);
				if (*s2 == '{') {
					do {
						s2=index (s2+1, '}');
					} while (s2 && *(s2-1) == '\\');
					if (s2) s2=index (s2+1, '\n');
					if (s2) s2++;
				} else {
					s2 = index (line, '\n');
					if (s2) s2++;
				}
				if (!s2) {
					*line = 0;
					break;
				}
				for (s=line; *s2; s++, s2++)
					*s = *s2;
				*s=0;
				break;
			} else if (!s2) {
				break;
			} else {
				line = s2+1;
			}
		}
	}
	return RERR_OK;
}

int
scf_addvar (infile, outfile, variable)
	const char	*infile, *outfile, *variable;
{
	char	*ibuf, *obuf;
	int	len, ret;
	char	passwd[1024];
	int	cflen, nlen, cf2len;
	char	*val, *s, *s2;
	int	need_nl;
	char	var[1024];

	if (!variable || !*variable) return RERR_PARAM;
	/* split variable */
	s2 = top_skipwhite (variable);
	s = index (s2, '=');
	if (!s) return RERR_INVALID_ENTRY;
	val = s+1;
	for (s--; s>=s2 && iswhite (*s); s--) {}; s++;
	if (s==s2) return RERR_INVALID_ENTRY;
	len = s-s2;
	if (len > 1023) len = 1023;
	strncpy (var, s2, len);
	var[len] = 0;
	
	ret = scf_getfile (infile, &obuf, &len);
	if (!RERR_ISOK (ret)) return ret;
	ret = scf_askverifypass ("Password: ", passwd, 1024, obuf, len);
	if (!RERR_ISOK (ret)) {
		free (obuf);
		return ret;
	}
	ibuf = obuf;
	ret = scf_decrypt (ibuf, len, &obuf, passwd, 0);
	free (ibuf);
	if (!RERR_ISOK (ret)) return ret;
	cflen = strlen (obuf);
	ret = do_delete_var (var, obuf);
	cf2len = strlen (obuf);
	nlen = strlen (var) + strlen (val) + 4;
	if (nlen > cflen - cf2len) {
		ibuf = obuf;
		obuf = malloc (cf2len + nlen + 2);
		if (!obuf) {
			bzero (ibuf, strlen (ibuf));
			free (ibuf);
			return RERR_NOMEM;
		}
		strcpy (obuf, ibuf);
		bzero (ibuf, strlen (ibuf));
		free (ibuf);
	}
	s = obuf+strlen (obuf);
	need_nl = s>obuf && *(s-1) != '\n';
	sprintf (s, "%s%s=%s\n", need_nl?"\n":"", var, val);
	ibuf = obuf;
	ret = scf_encrypt (ibuf, &obuf, &len, passwd);
	bzero (ibuf, strlen (ibuf));
	free (ibuf);
	if (!RERR_ISOK (ret)) return ret;
	ret = scf_writeout (outfile, obuf, len);
	free (obuf);
	return ret;
}


int
scf_list (infile, outfile)
	const char	*infile, *outfile;
{
	char	*ibuf, *obuf;
	int	len, ret;
	char	passwd[1024];
	FILE	*f;
	
	ret = scf_getfile (infile, &obuf, &len);
	if (!RERR_ISOK (ret)) return ret;
	ret = scf_askverifypass ("Password: ", passwd, 1024, obuf, len);
	if (!RERR_ISOK (ret)) {
		free (obuf);
		return ret;
	}
	ibuf = obuf;
	ret = scf_decrypt (ibuf, len, &obuf, passwd, 0);
	free (ibuf);
	if (!RERR_ISOK (ret)) return ret;
	if (!outfile || !strcmp (outfile, "-")) {
		f = stdout;
	} else {
		f = fopen (outfile, "w");
		if (!f) {
			FRLOGF (LOG_ERR, "cannot open file %s for writing: %s", outfile,
						rerr_getstr3(RERR_SYSTEM));
			bzero (obuf, strlen (obuf));
			free (obuf);
			return RERR_SYSTEM;
		}
	}
	fwrite (obuf, 1, strlen (obuf), f);
	bzero (obuf, strlen (obuf));
	free (obuf);
	if (f != stdout) fclose (f);
	return RERR_OK;
}


int
scf_encryptfile (infile, outfile)
	const char	*infile, *outfile;
{
	char	*ibuf, *obuf;
	int	ilen, olen, ret;
	char	passwd[1024];
	FILE	*f;

	if (!infile || !strcmp (infile, "-")) {
		f = stdin;
	} else {
		f = fopen (infile, "-");
		if (!f) {
			FRLOGF (LOG_ERR, "error opening file %s for reading: %s", infile,
						rerr_getstr3(RERR_SYSTEM));
			return RERR_SYSTEM;
		}
	}
	ibuf = fop_read_file2 (f, &ilen);
	if (f != stdin) fclose (f);
	if (!ibuf) {
		FRLOGF (LOG_ERR, "error reading file >>%s<<: %s",
						((f!=stdin) ? infile : "<stdin>"),
						rerr_getstr3(RERR_SYSTEM));
		return RERR_SYSTEM;
	}
	ret = scf_doubleaskpass (passwd, 1024);
	if (!RERR_ISOK (ret)) {
		bzero (ibuf, ilen);
		free (obuf);
		return ret;
	}
	ret = scf_encrypt (ibuf, &obuf, &olen, passwd);
	bzero (ibuf, ilen);
	free (ibuf);
	if (!RERR_ISOK (ret)) return ret;
	ret = scf_writeout (outfile, obuf, olen);
	free (obuf);
	if (!RERR_ISOK (ret)) return ret;

	return RERR_OK;
}




int
scf_setfname (fname)
	const char	*fname;
{
	cf_begin_read ();
	if (secure_cf) {
		free (secure_cf);
		secure_cf = NULL;
	}
	if (!fname) {
		have_constscf = 0;
		config_read = 0;
	} else {
		secure_cf = strdup (fname);
		if (secure_cf) have_constscf = 1;
	}
	cf_end_read ();
	if (fname && !secure_cf) return RERR_NOMEM;
	return RERR_OK;
}












static
int
read_config ()
{
	const char	*s;

	cf_begin_read ();
	if (!have_constscf) {
		if (secure_cf) {
			free (secure_cf);
			secure_cf = NULL;
		}
		s = cf_getval ("secure_config_file");
		if (!s) s = cf_getval ("secure_cf");
		if (!s) s = cf_getval ("crypt_cf");
		if (s) secure_cf = strdup (s);
	}
	config_read = 1;
	cf_end_read_cb (&read_config);
	return 1;
}

#if 0
static
int
scf_mymlock (addr, len)
	void		*addr;
	size_t	len;
{
	static size_t	psize = 0;
	size_t			start, diff;

	if (!psize) psize = getpagesize ();
	if (!addr) return RERR_PARAM;
	if (len<=0) return RERR_OK;
	start = (size_t) addr;
	diff = start % psize;
	if (diff > 0) {
		start -= diff;
	}
	addr = (void*) start;
	len += diff;
	diff = len % psize;
	if (diff > 0) {
		len += (psize - diff);
	}
	if (mlock (addr, len) < 0) return RERR_SYSTEM;
	return RERR_OK;
}

static
int
scf_mymunlock (addr, len)
	void		*addr;
	size_t	len;
{
	static size_t	psize = 0;
	size_t			start, diff;

	if (!psize) psize = getpagesize ();
	if (!addr) return RERR_PARAM;
	if (len<=0) return RERR_OK;
	start = (size_t) addr;
	diff = start % psize;
	if (diff > 0) {
		start -= diff;
	}
	addr = (void*) start;
	len += diff;
	diff = len % psize;
	if (diff > 0) {
		len += (psize - diff);
	}
	if (munlock (addr, len) < 0) return RERR_SYSTEM;
	return RERR_OK;
}


#endif
















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
