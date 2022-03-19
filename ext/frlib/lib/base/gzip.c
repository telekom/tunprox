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
#include <string.h>
#include <strings.h>
#include <time.h>
#include <sys/types.h>
#include <ctype.h>



#include "gzip.h"
#include "slog.h"
#include "config.h"
#include "errors.h"
#include "iopipe.h"


static const char	*prog_gzip = "gzip";
static const char	*prog_bzip = "bzip2";
static const char	*prog_compress = "compress";
static int config_read = 0;

static int read_config ();
static int dozip (const char*, const char*, int, char**, int*, int);
static int dounzip (const char*, const char*, int, char**, int*);



int
gzip (in, inlen, out, outlen, level)
	const char	*in;
	char			**out;
	int			inlen, *outlen, level;
{
	CF_MAY_READ;
	return dozip (prog_gzip, in, inlen, out, outlen, level);
}

int
gunzip (in, inlen, out, outlen)
	const char	*in;
	char			**out;
	int			inlen, *outlen;
{
	CF_MAY_READ;
	return dounzip (prog_gzip, in, inlen, out, outlen);
}


int
bzip2 (in, inlen, out, outlen, level)
	const char	*in;
	char			**out;
	int			inlen, *outlen, level;
{
	CF_MAY_READ;
	return dozip (prog_bzip, in, inlen, out, outlen, level);
}

int
bunzip2 (in, inlen, out, outlen)
	const char	*in;
	char			**out;
	int			inlen, *outlen;
{
	CF_MAY_READ;
	return dounzip (prog_bzip, in, inlen, out, outlen);
}

int
compress (in, inlen, out, outlen)
	const char	*in;
	char			**out;
	int			inlen, *outlen;
{
	CF_MAY_READ;
	return dozip (prog_compress, in, inlen, out, outlen, -1);
}

int
uncompress (in, inlen, out, outlen)
	const char	*in;
	char			**out;
	int			inlen, *outlen;
{
	CF_MAY_READ;
	return dounzip (prog_compress, in, inlen, out, outlen);
}



static
int
dozip (prog, in, inlen, out, outlen, level)
	const char	*prog, *in;
	char			**out;
	int			inlen, *outlen, level;
{
	char	slevel[64];

	if (!prog || !in || !out || inlen < 0 || level > 9) return RERR_PARAM;
	*slevel=0;
	if (level>=0) sprintf (slevel, " -%d", level);
	return iopipef (in, inlen, out, outlen, 120, 0, "%s -c%s", prog, slevel);
}

static
int
dounzip (prog, in, inlen, out, outlen)
	const char	*prog, *in;
	char			**out;
	int			inlen, *outlen;
{
	int	ret;

	if (!prog || !in || !out || inlen < 0) return RERR_PARAM;
	ret = iopipef (in, inlen, out, outlen, 120, 0, "%s -dc", prog);
	if (RERR_ISOK (ret) || ret == RERR_NOMEM) return ret;
	return RERR_INVALID_FORMAT;
}



int
isgzip (in, ilen)
	const char	*in;
	int			ilen;
{
	if (!in || ilen < 3) return 0;
	return (in[0] == 0x1f && (unsigned char) in[1] == 0x8b)?1:0;
}









static
int
read_config ()
{
	cf_begin_read ();
	prog_gzip = cf_getarr2 ("prog", "gzip", "gzip");
	prog_bzip = cf_getarr2 ("prog", "bzip2", "bzip2");
	prog_compress = cf_getarr2 ("prog", "compress", "compress");
	config_read=1;
	cf_end_read_cb (&read_config);
	return 1;
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
