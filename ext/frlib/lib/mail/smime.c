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
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef SunOS
extern int errno;
#endif


#include <fr/base/iopipe.h>
#include <fr/base/config.h>
#include <fr/base/slog.h>
#include <fr/base/errors.h>
#include <fr/base/strcase.h>
#include <fr/base/textop.h>
#include <fr/base/dn.h>
#include <fr/mail/smime.h>


static int read_config ();
static int config_read = 0;


struct dn_entry {
	const char	*dn;
	const char	*idx;
};
struct dn_list {
	struct dn_entry	*list;
	int					num;
};
#define DN_LIST_NULL	((struct dn_list){NULL, 0})
struct cert_entry {
	char	*idx;
	char	*cf, *kf, *dn;
	char	*certs, *signfor;
	char	*email, *from;
	int	docrypt, dosign, clearsign;
};
struct cert_list {
	struct cert_entry	*list;
	int					num;
};
#define CERT_ENTRY_NULL	((struct cert_entry){NULL,NULL,NULL,NULL,NULL,\
															NULL,NULL,NULL,0,0,0})
#define CERT_LIST_NULL ((struct cert_list){NULL, 0})


static int have_dn = 0;
static struct dn_list	dn_list = DN_LIST_NULL;
static struct cert_list	cert_list = CERT_LIST_NULL;
static int have_cert_list = 0;

static const char *ca_cert_file = NULL;
static const char *ca_cert_dir = NULL;
static const char *cipher = NULL;
static int	use_hsm = 0;
static const char *hsm_pin = NULL;
static const char	*key_pass = NULL;
static int	timeout = 0;
static const char	*egd = NULL;
static const char *ssl_prog = "openssl";
static const char *default_signfor = NULL;
static const char *encrypt_default = NULL;
static const char	*sslout_ondecrypt = NULL;

static int	read_dn();
static int	validate_dn (const char *, const char**);
static const char *get_cipher ();
static int	hfree_cert_list();
static int	read_certlist();
static struct cert_entry *get_certentry (const char*, int);

static int from_der (char**, int*, int, const char*, int);
static int to_der (char**, int*, const char*, int, int);

static
const char *
get_cipher ()
{
	CF_MAY_READ;
	if (!cipher) cipher = "rc2-128";
	sswitch (cipher) {
	sicase ("rc2-40")
		return "-rc2-40";
	sicase ("rc2-128")
		return "-rc2-128";
	sicase ("des")
		return "-des";
	sicase ("des3")
	sicase ("3des")
	sicase ("tripple-des")
		return "-des3";
	sicase ("rc2-64")
		return "-rc2-64";
	sicase ("aes128")
	sicase ("aes-128")
	sicase ("aes")
	sicase ("rijndael-128")
	sicase ("rijndael128")
	sicase ("rijndael")
		return "-aes128";
	sicase ("aes192")
	sicase ("aes-128")
	sicase ("rijndael192")
	sicase ("rijndael-192")
		return "-aes192";
	sicase ("rijndael256")
	sicase ("rijndael-256")
	sicase ("aes256")
	sicase ("aes-256")
		return "-aes256";
	sdefault
		return "-rc2-128";
	} esac;

	return "-rc2-128";
}



int
smim_encrypt (in, ilen, out, olen, encrypt_for, flags)
	const char	*in, *encrypt_for;
	char			**out;
	int			ilen, *olen, flags;
{
	const char			*cf;
	struct cert_entry	*ce;
	int					ret;

	if (!in || !out) return RERR_PARAM;
	CF_MAY_READ;
	if (!have_cert_list) {
		ret = read_certlist ();
		if (!RERR_ISOK(ret)) return ret;
	}
	if (!encrypt_for) encrypt_for = encrypt_default;
	ce = get_certentry (encrypt_for, 0);
	if ((flags & SMIM_F_CHECK_CRYPT) && ce && !ce->docrypt) {
		*olen = ilen;
		*out = malloc (ilen+1);
		if (!*out) return RERR_NOMEM;
		memcpy (*out, in, ilen);
		(*out)[ilen]=0;
		return RERR_OK;
	}
	cf = ce ? ce->cf : NULL;
	if (!cf) {
		ret = encrypt_for ? RERR_PARAM : RERR_CONFIG;
		FRLOGF (LOG_ERR, "cannot find certificate for >>%s<<: %s",
						encrypt_for?encrypt_for:"", rerr_getstr3 (ret));
		return ret;
	}
	ret = smim_encrypt2 (in, ilen, out, olen, cf, flags);
	return ret;
}

int
smim_encrypt2 (in, ilen, out, olen, cf, flags)
	const char	*in, *cf;
	char			**out;
	int			ilen, *olen, flags;
{
	const char	*megd, *rand;
	const char	*cipher2;
	int			iflags;

	if (!in || !out || !cf) return RERR_PARAM;
	CF_MAY_READ;
	iflags = 0 | ((flags & SMIM_F_SILENT) ? IOPIPE_F_NODEBUG : 0);
	if (egd) {
		megd = egd;
		rand = "-rand ";
	} else {
		megd = rand = "";
	}
	cipher2 = get_cipher();
	if (!cipher2) return RERR_INTERNAL;
	return iopipef (in, ilen, out, olen, timeout, iflags,
			"%s smime -encrypt %s %s%s %s", ssl_prog, cipher2, rand, megd, cf);
}

int
smim_encryptlc (in, ilen, out, olen, cert, certlen, format, flags)
	const char	*in;
	char			**out;
	int			ilen, *olen, certlen;
	const char	*cert;
	int			format, flags;
{
	char	tmpfile[256];
	int	fd, oldmask;
	int	ret;
	char	*b64out = NULL;

	if (!in || !out || !cert) return RERR_PARAM;
	CF_MAY_READ;
	strcpy (tmpfile, "/tmp/smim_cert.XXXXXX");
	oldmask = umask (0077);
	fd = mkstemp (tmpfile);
	umask (oldmask);
	switch (format) {
	case SMIM_CERT_PEM:
		write (fd, cert, certlen);
		break;
	case SMIM_CERT_DER:
		write (fd, "-----BEGIN CERTIFICATE-----\n", 28);
		ret = top_base64encode (cert, certlen, &b64out);
		if (!RERR_ISOK(ret)) {
			close (fd);
			unlink (tmpfile);
			FRLOGF (LOG_ERR, "error base64 encode certificate");
			return ret;
		}
		write (fd, b64out, strlen (b64out));
		free (b64out);
		b64out = NULL;
		write (fd, "-----END CERTIFICATE-----\n", 26);
		break;
	default:
		FRLOGF (LOG_ERR, "unknown certificate format %d\n", format);
		close (fd);
		unlink (tmpfile);
		return RERR_INVALID_FORMAT;
	}
	close (fd);
	ret = smim_encrypt2 (in, ilen, out, olen, tmpfile, flags);
	unlink (tmpfile);
	return ret;
}


int
smim_decrypt (in, inlen, out, outlen, flags)
	const char	*in;
	char			**out;
	int			inlen, *outlen, flags;
{
	int					ret;
	int					i;
	char					*kf, *cf;
	struct cert_entry	*ce;

	if (!in || !out) return RERR_PARAM;
	CF_MAY_READ;
	if (!have_cert_list) {
		ret = read_certlist ();
		if (!RERR_ISOK(ret)) return ret;
	}
	if (flags & SMIM_F_VERBOSE) {
		flags &= ~SMIM_F_SILENT;
	} else if (flags & SMIM_F_SILENT) {
		/* flags &= ~SMIM_F_VERBOSEONLAST; */
	} else if (sslout_ondecrypt) {
		if (cf_isyes (sslout_ondecrypt)) {
			flags &= ~SMIM_F_SILENT;
		} else {
			flags |= SMIM_F_SILENT;
		}
	} else if (flags & SMIM_F_VERBOSEONLAST) {
		flags |= SMIM_F_SILENT;
	}
	for (i=0; i<cert_list.num; i++) {
		ce = &(cert_list.list[i]);
		kf = ce->kf;
		if (!kf || !*kf) continue;
		cf = ce->cf;
		if (!cf || !*cf) continue;
		if ((i==(cert_list.num-1)) && (flags & SMIM_F_VERBOSEONLAST)) {
			flags &= ~SMIM_F_SILENT;
		}
		ret = smim_decrypt2 (in, inlen, out, outlen, kf, cf, flags);
		if (RERR_ISOK(ret)) break;
	}
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error decrypting file");
		return RERR_DECRYPTING;
	}
	return RERR_OK;
}


int
smim_decrypt2 (in, ilen, out, olen, keyfile, certfile, flags)
	const char	*in;
	char			**out;
	int			*olen, ilen, flags;
	const char	*keyfile, *certfile;
{
	int	ret;
	int	iflags;

	iflags = 0 | ((flags & SMIM_F_SILENT) ? IOPIPE_F_NODEBUG : 0);
	if (!in || !out || !keyfile || !certfile) return RERR_PARAM;
	CF_MAY_READ;
	if (use_hsm || key_pass) {
		ret = iopipef (in, ilen, out, olen, timeout, iflags,
							"%s smime -decrypt %s -passin fd:%y -inkey %s -recip %s",
							(use_hsm?hsm_pin:key_pass), ssl_prog, 
							(use_hsm?"-engine pkcs11 -keyform engine":""),
							keyfile, certfile);
	} else {
		ret = iopipef (in, ilen, out, olen, timeout, iflags,
							"%s smime -decrypt -inkey %s -recip %s",
							ssl_prog, keyfile, certfile);
	}

	if (RERR_ISOK(ret) && (flags & SMIM_F_DELCR)) {
		top_delcr (*out);
	}
	return ret;
}



int
smim_verify (in, ilen, out, olen, flags, signer)
	const char	*in;
	char			**out;
	char			**signer;
	int			ilen, *olen, flags;
{
	int	ret, iflags;
	int	fd, oldmask;
	char	tmpfile[256];
	char	*dn;
	char	*s;

	if (!in || !out) return RERR_PARAM;
	CF_MAY_READ;
	iflags = 0 | ((flags & SMIM_F_SILENT) ? IOPIPE_F_NODEBUG : 0);
	strcpy (tmpfile, "/tmp/smim.XXXXXX");
	oldmask = umask (0077);
	fd = mkstemp (tmpfile);
	umask (oldmask);
	if (fd == -1) {
		FRLOGF (LOG_ERR, "smim_verify(): cannot create tempfile >>%s<<: %s", 
							tmpfile, rerr_getstr3(RERR_SYSTEM));
		return RERR_SYSTEM;
	}
	close (fd);
	ret = iopipef (in, ilen, out, olen, timeout, iflags,
							"%s smime -verify %s %s %s %s -signer %s", 
							ssl_prog, (ca_cert_file?"-CAfile":""), 
							(ca_cert_file?ca_cert_file:""),
							(ca_cert_dir?"-CApath":""),
							(ca_cert_dir?ca_cert_dir:""), tmpfile);
	if (!RERR_ISOK(ret)) {
		unlink (tmpfile);
		return (ret==RERR_NOMEM)?RERR_NOMEM:RERR_INVALID_SIGNATURE;
	}
	if (flags & SMIM_F_DELCR) {
		top_delcr (*out);
	}
	ret = iopipef (NULL, 0, &dn, NULL, timeout, iflags, 
							"%s x509 -in %s -noout -subject", ssl_prog, tmpfile);
	unlink (tmpfile);
	if (!RERR_ISOK(ret)) {
		free (*out);
		*out = NULL;
		return ret;
	}
	s = index (dn, '\n');
	if (s) *s = 0;
	s = index (dn, '=');
	s = top_skipwhite (s?s+1:dn);
	if (flags & SMIM_F_PASS_DN) {
		if (signer) *signer = strdup (s);
		ret = RERR_OK;
	} else if (!(flags & SMIM_F_NO_VALIDATE_DN)) {
		ret = validate_dn (s, (const char **)signer);
	}
	free (dn);
	return ret;
}

int
smim_verify2 (in, ilen, out, olen, flags, signer)
	char			**out;
	const char	*in, *signer;
	int			ilen, *olen, flags;
{
	char	*signer2;
	int	ret;

	if (!signer || !in || !out) return RERR_PARAM;
	ret = smim_verify (in, ilen, out, olen, flags, &signer2);
	if (!RERR_ISOK(ret)) return ret;
	if (!signer2) return RERR_INTERNAL;
	ret = !strcasecmp (signer, signer2);
	free (signer2);
	if (!ret && *out) {
		free (*out);
		*out = NULL;
		return RERR_INVALID_SIGNATURE;
	}
	return RERR_OK;
}

	



int
smim_sign (in, ilen, out, olen, signfor, flags)
	const char	*in, *signfor;
	char			**out;
	int			ilen, *olen, flags;
{
	char					tmp[64];
	char					buf[1024];
	int					fd;
	int					num;
	int					ret;
	FILE					*f;
	int					i;
	mode_t				oldmask;
	const char			*envstr;
	const char			*cf, *kf;
	int					force_clear, iflags;
	struct cert_entry	*ce;
	const char			*certs;
	int					clearsign;

	if (!in || !out) return RERR_PARAM;
	CF_MAY_READ;

	if (!have_cert_list) {
		ret = read_certlist();
		if (!RERR_ISOK(ret)) return ret;
	}
	force_clear = (flags & SMIM_F_FORCE_CLEAR) ? 1 : 0;
	iflags = 0 | ((flags & SMIM_F_SILENT) ? IOPIPE_F_NODEBUG : 0);
	if (!signfor || !*signfor) {
		signfor = default_signfor;
	}
	ce = get_certentry (signfor, 1);
	if (!ce) {
		FRLOGF (LOG_ERR, "cannot find certificate for >>%s<<", signfor);
		return RERR_CONFIG;
	}
	if ((flags & SMIM_F_CHECK_SIGN) && !ce->dosign) {
		*olen = ilen;
		*out = malloc (ilen+1);
		if (!*out) return RERR_NOMEM;
		memcpy (*out, in, ilen);
		(*out)[ilen]=0;
		return RERR_OK;
	}
	cf = ce->cf;
	kf = ce->kf;
	if (!cf || !kf) {
		FRLOGF (LOG_ERR, "cannot find %sfile for >>%s<<",
						((!cf&&!kf)?"key- and cert":((!cf)?"cert":
						"key")), signfor?signfor:"<NULL>");
		return RERR_NOKEY;
	}
	certs = ce->certs?ce->certs:"";
	clearsign = ce->clearsign;
	
	/* write in-buffer to temp file */
	strcpy (tmp, "/tmp/smim.XXXXXX");
	oldmask = umask (0077);
	fd = mkstemp (tmp);
	umask (oldmask);
	if (fd == -1) {
		FRLOGF (LOG_ERR, "cannot create tempfile >>%s<<: %s", tmp,
							rerr_getstr3(RERR_SYSTEM));
		return RERR_SYSTEM;
	}
	if (ilen<=0) ilen = strlen (in);
	num = write (fd, in, ilen);
	if (num != ilen) {
		FRLOGF (LOG_ERR, "error writing to >>%s<<", tmp);
		return RERR_SYSTEM;
	}
	close (fd);

	if (!clearsign && !force_clear) {
		envstr = "-nodetach ";
	} else {
		envstr = "";
	}
	if (use_hsm || key_pass) {
		ret = iopipef (NULL, 0, out, NULL, timeout, iflags, "%s smime -sign "
								"-signer %s -inkey %s %s %s -passin fd:%y %s -in %s",
								(use_hsm?hsm_pin:key_pass), ssl_prog, cf, kf,
								(use_hsm?"-engine pkcs11 -keyform engine":""),
								envstr, certs, tmp);
	} else {
		ret = iopipef (NULL, 0, out, NULL, timeout, iflags, "%s smime -sign "
								"-signer %s %s -inkey %s %s -in %s", ssl_prog, cf,
								envstr, kf, certs, tmp);
	}

	/* overwrite temp file */
	bzero (buf, 1024);
	f = fopen (tmp, "r");
	if (f) {
		for (i=ilen/1024+(ilen%24>0); i; i--) 
			fwrite (buf, 1, 1024, f);
		fclose (f);
	}
	
	/* delete temp file */
	unlink (tmp);

	/* if enveloped change mime-header - add smime-type="signed-data" */
	if (!clearsign && !force_clear && RERR_ISOK(ret)) {
		char	* ibuf, *obuf, *s;
		ibuf = *out;
		*out = NULL;
		obuf = malloc (strlen (ibuf) + 32);
		if (!obuf) {
			free (ibuf);
			return RERR_NOMEM;
		}
		strcpy (obuf, "Content-Disposition: attachment; "
							"filename=\"smime.p7m\"\n"
							"Content-Type: application/x-pkcs7-mime; "
							"name=\"smime.p7m\"; "
							"smime-type=\"signed-data\"\n"
							"Content-Transfer-Encoding: base64\n\n");
		s = strstr (ibuf, "\n\n");
		if (!s) {
			free (ibuf); free (obuf);
			FRLOGF (LOG_ERR, "openssl output doesn't contain valid data");
			return RERR_INVALID_MAIL;
		}
		s+=2;
		strcat (obuf, s);
		free (ibuf);
		*out = obuf;
	}

	return ret;
}


static
int
read_config ()
{
	cf_begin_read ();
	ca_cert_file = cf_getval ("smim_root_ca_cert_file");
	if (!ca_cert_file)
		ca_cert_file = cf_getval ("smim_ca_cert_file");
	ca_cert_dir = cf_getval ("smim_ca_cert_dir");
	if (!ca_cert_dir) cf_getval ("smim_ca_cert_path");
	if (!ca_cert_file && !ca_cert_dir) {
		FRLOGF (LOG_WARN, "you must configure ca_cert_file or ca_cert_dir");
	}
	use_hsm = cf_isyes (cf_getval2 ("smim_use_hsm", "no"));
	hsm_pin = cf_getval2 ("smim_hsm_pin", "");
	key_pass = cf_getval ("smim_key_pass");
	cipher = cf_getval2 ("smim_cipher", "rc-128");
	ssl_prog = cf_getarr2 ("prog", "openssl", "openssl");
	timeout = cf_atoi (cf_getval2 ("smim_timeout", "0"));
	egd = cf_getval ("smim_egd_path");
	default_signfor = cf_getval ("smim_default_signfor");
	encrypt_default = cf_getval ("smim_default_encryptfor");
	sslout_ondecrypt = cf_getval ("smim_sslout_ondecrypt");
	config_read = 1;
	have_cert_list = 0;
	have_dn = 0;
	cf_end_read_cb (&read_config);

	return 1;
}




static
int
validate_dn (dn, signer)
	const char	*dn;
	const char	**signer;
{
	int	i;

	if (!dn) return RERR_PARAM;
	if (!*dn) return RERR_INVALID_SIGNATURE;
	if (!have_dn) read_dn();
	for (i=0; i<dn_list.num; i++) {
		if (!dn_cmp (dn_list.list[i].dn, dn)) {
			if (signer) {
				*signer = dn_list.list[i].idx;
			}
			return RERR_OK;
		}
	}
	return RERR_INVALID_SIGNATURE;
}


static
int
read_dn ()
{
	int	i, num;

	CF_MAY_READ;
	if (have_dn) {
		if (dn_list.list) free (dn_list.list);
		dn_list = DN_LIST_NULL;
		have_dn = 0;
	}
	cf_begin_read ();
	num=cf_getnumarr ("smim_dn");
	if (num>0) {
		dn_list.list = realloc (dn_list.list, num*sizeof (struct dn_entry));
		if (!dn_list.list) {
			dn_list.num = 0;
			return 0;
		}
		for (i=0; i<num; i++) {
			dn_list.list[i].dn=cf_getarrxwi ("ext_ca_dn", i, 
								&(dn_list.list[dn_list.num+i].idx));
		}
		dn_list.num = num;
	}
	have_dn = 1;
	cf_end_read();
	return 1;
}



static
struct cert_entry*
get_certentry (idx, indirect)
	const char	*idx;
	int			indirect;
{
	int					low, up, mid, res;
	struct cert_entry	*p;

	if (!idx) return NULL;
	low=0; up=cert_list.num-1;
	while (up>=low) {
		mid=(up+low)/2;
		res = strcasecmp (idx, cert_list.list[mid].idx);
		if (res == 0) {
			if (indirect) {
				p = get_certentry (cert_list.list[mid].signfor, 0);
				if (p) return p;
			}
			return &(cert_list.list[mid]);
		} else if (res<0) {
			up=mid-1;
		} else {
			low=mid+1;
		}
	}
	return NULL;
}


static
int
read_certlist ()
{
	int			i, j, num, found, sz;
	char			*s2, *field, *cl, *certs;
	const char	*idx, *cf, *s;

	cf_begin_read ();
	/* get all cert_file entries */
	num = cf_getnumarr ("smim_cert_file");
	hfree_cert_list ();
	cert_list.list = malloc (num*sizeof (struct cert_entry));
	if (!cert_list.list) {
		cf_end_read ();
		return RERR_NOMEM;
	}
	bzero (cert_list.list, num*sizeof(struct cert_entry));
	cert_list.num = num;
	for (i=0; i<num; i++) {
		s = cf_getarrxwi ("smim_cert_file", i, &idx);
		if (!s || !idx || !*idx) continue;
		cert_list.list[i].idx = strdup (idx);
	}
	/* get also all signfor entries */
	num = cf_getnumarr ("smim_signfor");
	for (i=0; i<num; i++) {
		s = cf_getarrxwi ("smim_sign_for", i, &idx);
		if (!s || !idx || !*idx) continue;
		for (j=0; j<cert_list.num; j++) {
			if (!cert_list.list[j].idx) continue;
			if (!strcasecmp (cert_list.list[j].idx, idx)) break;
		}
		if (j==cert_list.num) {
			cert_list.num++;
			cert_list.list = realloc (cert_list.list, 
								cert_list.num*sizeof (struct cert_entry));
			if (!cert_list.list) {
				cert_list.num = 0;
				cf_end_read ();
				return RERR_NOMEM;
			}
			cert_list.list[j] = CERT_ENTRY_NULL;
			cert_list.list[j].idx = strdup (idx);
		}
	}
	/* delete all empty indexes */
	for (i=0, j=-1; i<cert_list.num; i++) {
		if (!cert_list.list[i].idx || !*cert_list.list[i].idx) continue;
		j++;
		if (i==j) continue;
		cert_list.list[j].idx = cert_list.list[i].idx;
	}
	cert_list.num=j++;
	/* sort entries */
	found = 1;
	while (found) {
		found = 0;
		for (i=0; i<cert_list.num-1; i++) {
			if (strcasecmp(cert_list.list[i].idx,cert_list.list[i+1].idx) > 0) {
				s2 = cert_list.list[i].idx;
				cert_list.list[i].idx = cert_list.list[i+1].idx;
				cert_list.list[i+1].idx = s2;
				found=1;
			}
		}
	}
	/* read the rest from config */
	for (i=0; i<cert_list.num; i++) {
		idx = cert_list.list[i].idx;
		s = cf_getarr ("smim_cert_file", idx);
		if (s) cert_list.list[i].cf = strdup (s);
		s = cf_getarr ("smim_key_file", idx);
		if (s) cert_list.list[i].kf = strdup (s);
		s = cf_getarr ("smim_dn", idx);
		if (s) cert_list.list[i].dn = strdup (s);
		s = cf_getarr ("smim_signfor", idx);
		if (s) cert_list.list[i].signfor = strdup (s);
		s = cf_getarr ("smim_email", idx);
		if (s) cert_list.list[i].email = strdup (s);
		s = cf_getarr ("smim_from_email", idx);
		if (s) cert_list.list[i].from = strdup (s);
		cert_list.list[i].docrypt = cf_isyes (cf_getarr2 ("smim_docrypt", idx, "yes"));
		cert_list.list[i].dosign = cf_isyes (cf_getarr2 ("smim_dosign", idx, "yes"));
		cert_list.list[i].clearsign = cf_isyes (cf_getarr2 ("smim_clearsign", idx, "no"));
		s = cf_getarr ("smim_sign_certs", idx);
		if (!s) continue;
		s = top_skipwhiteplus (s, ",");
		if (!s || !*s) continue;
		certs = strdup (s);
		if (!certs) return RERR_NOMEM;
		for (j=1, s=certs; s && *s; s = index (s+1, ',')) j++;
		sz=1;
		cl = NULL;
		while ((field = top_getfield (&certs, ",", 0))) {
			if (!*field) continue;
			if (*field == '/') {
				cf = field;
			} else {
				cf = cf_getarr ("smim_cert_file", field);
				if (!cf || !*cf) continue;
			}
			sz += strlen (cf) + 12;
			cl = realloc (cl, sz);
			if (!cl) continue;
			strcat (cl, " -certfile ");
			strcat (cl, cf);
		}
		cert_list.list[i].certs = cl;
		free (certs);
	}
	/* ok - we are done */
	have_cert_list = 1;
	cf_end_read ();
	return RERR_OK;
}

static
int
hfree_cert_list ()
{
	int					i;
	struct cert_entry	*p;

	for (i=0; i<cert_list.num; i++) {
		p = &(cert_list.list[i]);
		if (p->idx) free (p->idx);
		if (p->cf) free (p->cf);
		if (p->kf) free (p->kf);
		if (p->certs) free (p->certs);
		if (p->signfor) free (p->signfor);
		if (p->dn) free (p->dn);
		if (p->email) free (p->email);
		if (p->from) free (p->from);
		*p = CERT_ENTRY_NULL;
	}
	if (cert_list.list) free (cert_list.list);
	cert_list = CERT_LIST_NULL;
	return RERR_OK;
}


const char*
smim_email2idx (email)
	const char	*email;
{
	int	ret, i;
	char	*em;

	if (!email) return NULL;
	CF_MAY_READ;
	if (!have_cert_list) {
		ret = read_certlist();
		if (!RERR_ISOK(ret)) return email;
	}
	for (i=0; i<cert_list.num; i++) {
		em = cert_list.list[i].email;
		if (!em || !*em) continue;
		if (!smim_cmpemail (em, email)) return cert_list.list[i].idx;
	}
	return email;
}

#define EMAILCHARS	"_-@%."
#define ISEMAILCHAR(c)	(isalnum(c)||index(EMAILCHARS,(c)))
int
smim_cmpemail (email1, email2)
	const char	*email1, *email2;
{
	const char	*s1, *s2, *s;
	int			l1, l2, len, res;

	if (!email1 || !email2) {
		if (!email1 && !email2) return 0;
		if (!email1) return -1;
		return 1;
	}
	s1 = rindex (email1, '@');
	s2 = rindex (email2, '@');
	if (!s1 || !s2) {
		if (!s1 && !s2) return 0;
		if (!s1) return -1;
		return 1;
	}
	for (s=s1+1, s1--; s1 >= email1 && ISEMAILCHAR(*s1); s1--) {}; s1++;
	for (; *s && ISEMAILCHAR(*s); s++);
	l1 = s-s1;
	for (s=s2+1, s2--; s2 >= email2 && ISEMAILCHAR(*s2); s2--) {}; s2++;
	for (; *s && ISEMAILCHAR(*s); s++);
	l2 = s-s2;
	len = l1<l2?l1:l2;
	res = strncasecmp (s1, s2, len);
	if (!res && l1!=l2) {
		res = l1<l2 ? -1 : 1;
	}
	return res;
}


int
smim_chfmt (in, inlen, infmt, out, outlen, outfmt)
	char	*in, **out;
	int	inlen, *outlen;
	int	infmt, outfmt;
{
	char	*obuf;
	int	len, len2, ret;
	int	i;

	if (!in || !out) return RERR_PARAM;
	if (!SMIM_VALID_FMT(infmt) || !SMIM_VALID_FMT(outfmt)) return RERR_NOT_SUPPORTED;
	if (outfmt==SMIM_FMT_AUTO) return RERR_NOT_SUPPORTED;
	if (infmt == SMIM_FMT_AUTO) {
		if (!strncasecmp (in, "MIME-Version:", 13) || 
					!strncasecmp (in, "Content-", 8)) {
			infmt = SMIM_FMT_SMIME;
		} else if (!strncasecmp (in, "-----BEGIN PKCS7-----", 21)) {
			infmt = SMIM_FMT_PEM;
		} else {
			for (i=0; i<inlen; i++) {
				if (!ISB64CHAR(in[i])) break;
			}
			if (i==inlen) {
				infmt = SMIM_FMT_B64;
			} else {
				infmt = SMIM_FMT_DER;
			}
		}
	}
	if (infmt == outfmt) {
		obuf = malloc (inlen);
		if (!obuf) return RERR_NOMEM;
		memcpy (obuf, in, inlen);
		*out = obuf;
		if (outlen) *outlen = inlen;
		return RERR_OK;
	}
	if (infmt != SMIM_FMT_DER) {
		ret = to_der (&obuf, &len, in, inlen, infmt);
		if (!RERR_ISOK(ret)) return ret;
		if (outfmt == SMIM_FMT_DER) {
			*out = obuf;
			if (outlen) *outlen = len;
			return RERR_OK;
		}
	} else {
		obuf = in;
		len = inlen;
	}
	ret = from_der (out, &len2, outfmt, obuf, len);
	if (infmt != SMIM_FMT_DER) free (obuf);
	if (!RERR_ISOK(ret)) return ret;
	if (outlen) *outlen = len2;
	return RERR_OK;
}



static
int
to_der (out, outlen, in, inlen, infmt)
	const char	*in;
	char			**out;
	int			*outlen, inlen, infmt;
{
	const char	*s, *s2;
	int			ret;

	if (!out || !in || !outlen || infmt == SMIM_FMT_DER) return RERR_PARAM;
	if (!SMIM_VALID_FMT(infmt)) return RERR_NOT_SUPPORTED;
	if (infmt == SMIM_FMT_SMIME) {
		s = strstr (in, "\n\n");
		if (!s) return RERR_INVALID_FORMAT;
		s+=2;
	} else if (infmt == SMIM_FMT_PEM) {
		s = index (in, '\n');
		if (!s) return RERR_INVALID_FORMAT;
		s++;
		s2 = strstr (s, "-----END PKCS7-----");
		if (!s2) return RERR_INVALID_FORMAT;
	} else if (infmt == SMIM_FMT_B64) {
		s = in;
	} else {
		return RERR_NOT_SUPPORTED;
	}
	if (s2) {
		ret = top_base64decode2 (s, (int)(s2-s), out, outlen);
	} else {
		ret = top_base64decode (s, out, outlen);
	}
	return ret;
}


static
int
from_der (out, outlen, outfmt, in, inlen)
	char			**out;
	const char	*in;
	int			outfmt, *outlen, inlen;
{
	char	*obuf, *ibuf;
	int	ret, hasnl,len;

	if (!out || !in || !outlen || outfmt == SMIM_FMT_DER) return RERR_PARAM;
	if (!SMIM_VALID_FMT(outfmt)) return RERR_NOT_SUPPORTED;
	ret = top_base64encode (in, inlen, &obuf);
	if (!RERR_ISOK(ret)) return ret;
	if (outfmt == SMIM_FMT_B64) {
		*out = obuf;
		*outlen = strlen (obuf);
		return RERR_OK;
	} else if (outfmt == SMIM_FMT_PEM) {
		ibuf = obuf;
		len = strlen (ibuf);
		obuf = malloc (len + 45);
		if (!obuf) {
			free (ibuf);
			return RERR_NOMEM;
		}
		hasnl = len == 0 || ibuf[strlen(ibuf)-1] == '\n';
		sprintf (obuf, "-----BEGIN PKCS7-----\n%s%s-----END PKCS7-----\n",
					ibuf, hasnl?"":"\n");
		free (ibuf);
		*out = obuf;
		*outlen = strlen (obuf);
		return RERR_OK;
	} else if (outfmt == SMIM_FMT_SMIME) {
		ibuf = obuf;
		len = strlen (ibuf);
		obuf = malloc (len + 170);
		if (!obuf) {
			free (ibuf);
			return RERR_NOMEM;
		}
		hasnl = len == 0 || ibuf[strlen(ibuf)-1] == '\n';
		sprintf (obuf, "MIME-Version: 1.0\n"
							"Content-Disposition: attachment; filename=\"smime.p7m\"\n"
							"Content-Type: application/x-pkcs7-mime; name=\"smime.p7m\"\n"
							"Content-Transfer-Encoding: base64\n"
							"\n%s%s\n", ibuf, hasnl?"":"\n");
		free (ibuf);
		*out = obuf;
		*outlen = strlen (obuf);
		return RERR_OK;
	} else {
		return RERR_NOT_SUPPORTED;
	}
	return RERR_OK;
}


const char*
smim_idx_getto (idx)
	const char	*idx;
{
	struct cert_entry	*ce;

	if (!idx || !*idx) return NULL;
	ce = get_certentry (idx, 0);
	if (!ce) return NULL;
	return ce->email;
}

const char*
smim_idx_getfrom (idx)
	const char	*idx;
{
	struct cert_entry	*ce;

	if (!idx || !*idx) return NULL;
	ce = get_certentry (idx, 0);
	if (!ce) return NULL;
	return ce->from;
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
