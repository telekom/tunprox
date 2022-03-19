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
 * Portions created by the Initial Developer are Copyright (C) 2003-2020
 * by the Initial Developer. All Rights Reserved.
 *
 * ***** END LICENSE BLOCK ***** */


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>

#ifndef MAXPATH
# define MAXPATH 255
#endif

#include <fr/base.h>
#include <fr/connect.h>
#include "eddibox.h"


struct eddibox {
	char	*nbuf;
	char	*sys;
	char	*name;
	char	bpath[32];
	int	typ,
			sig,
			flags;
	char	*sock;
	int	sd,
			mqid;
	pid_t	pid;
	char	*filterpath;
	char	*infopath;
	struct {
		uint32_t	f_nbuf:1,
					f_name:1,
					f_sock:1,
					f_filterpath:1,
					f_infopath:1,
					rmbpath:1,
					rminfo:1,
					rmsock:1,
					rmfilter:1,
					isopen:1,
					islink:1;
	};
	struct scon	scon;
};


#if 0
type = unix | mqueue | unixd
path = /my/unix/socket/path
mqueue = id
signal = sig
pid = pid
filter = /path/to/filter
response = yes | no
#endif

#define EB_T_UNIX				0		/* unix sockets as streams */
#define EB_T_MQUEUE			1		/* message queues */
#define EB_T_UNIXD			2		/* unix sockets with dgrams */
#define EB_T_GETTYPE(flag)	((flag) & 0x0f)
#define EB_F_RESPONSE		0x10
#define EB_F_SIGNAL			0x20
//#define EB_F_SETSIG(sig)	((((sig) & 0x3f) << 16) | EB_F_SIGNAL)
#define EB_F_GETSIG(flag)	(((flag) & EB_F_SIGNAL) ? (((flag) >> 16) & 0x3f) : -1)
#define EB_F_GETFLAGS(flag)	((flag) & 0x0fff0)


static const char	*g_eddibase = "/var/run/eddi";
static const char *g_eddidefsys = "eddi";
static int config_read = 0;
static int read_config ();

static int dobox_free (struct eddibox*);
static int dobox_new (struct eddibox*, const char*, int);
static int dobox_new2 (struct eddibox*, const char*, int);
static int sbox_open (struct eddibox*);
static int sbox_close (struct eddibox*);
static int box_writeout (struct eddibox*);
static int sbox_link (struct eddibox*);
static int sbox_unlink (struct eddibox*);
static int geteddibase (char*, int, const char*, const char*);
static int maycreatebase (char*);
static int boxlist_add (struct eddibox *);
static int boxlist_rm (int);
static int boxlist_get (struct eddibox**, int);



int
eddibox_free (box)
	int	box;
{
	struct eddibox	*pbox;
	int				ret;

	ret = boxlist_get (&pbox, box);
	if (!RERR_ISOK(ret)) return ret;
	ret = dobox_free (pbox);
	if (!RERR_ISOK(ret)) return ret;
	return boxlist_rm (box);
}


int
eddibox_new (name, flags)
	const char	*name;
	int			flags;
{
	struct eddibox	*box;
	int				ret;

	box = malloc (sizeof (struct eddibox));
	if (!box) return RERR_NOMEM;
	ret = dobox_new (box, (char*) name, flags);
	if (!RERR_ISOK(ret)) {
		free (box);
		return ret;
	}	
	ret = boxlist_add (box);
	if (!RERR_ISOK(ret)) {
		dobox_free (box);
		free (box);
		return ret;
	}
	return ret;
}



static
int
dobox_free (box)
	struct eddibox	*box;
{
	if (!box) return RERR_PARAM;
	if (box->islink) sbox_unlink (box);
	if (box->isopen) sbox_close (box);
	if (box->rmsock && box->sock) unlink (box->sock);
	if (box->rminfo && box->infopath) unlink (box->infopath);
	if (box->rmfilter && box->filterpath) unlink (box->filterpath);
	if (box->rmbpath) fop_rmdir_rec (box->bpath);
	if (box->f_nbuf && box->nbuf) free (box->nbuf);
	if (box->f_name && box->name) free (box->name);
	if (box->f_sock && box->sock) free (box->sock);
	if (box->f_filterpath && box->filterpath) free (box->filterpath);
	if (box->f_infopath && box->infopath) free (box->infopath);
	bzero (box, sizeof (struct eddibox));
	return RERR_OK;
}


static
int
dobox_new (box, name, flags)
	struct eddibox	*box;
	const char		*name;
	int				flags;
{
	int	ret;

	ret = dobox_new2 (box, name, flags);
	if (ret == RERR_PARAM) return ret;
	if (!RERR_ISOK(ret)) {
		dobox_free (box);
		return ret;
	}
	return RERR_OK;
}


static
int
dobox_new2 (box, name, flags)
	struct eddibox	*box;
	const char		*name;
	int				flags;
{
	int	ret;
	char	*xname;

	if (!box || !name) return RERR_PARAM;
	bzero (box, sizeof (struct eddibox));
	name = top_skipwhiteplus (name, "/");
	box->nbuf = xname = strdup (name);
	if (!xname) return RERR_NOMEM;
	box->f_nbuf = 1;
	box->sys = top_getfield (&xname, "/", 0);
	box->name = top_getfield (&xname, "/", 0);
	xname = top_getfield (&xname, "/", 0);
	if ((xname && *xname) || !box->sys) return RERR_INVALID_NAME;
	if (!box->name) {
		box->name = box->sys;
		box->sys = (char*)cf_getval2 ("eddi_default_system", "eddi");
	}
	box->typ = EB_T_GETTYPE(flags);
	box->sig = EB_F_GETSIG(flags);
	box->flags = EB_F_GETFLAGS(flags);
	box->pid = getpid ();
	strcpy (box->bpath, "/tmp/eddibox-XXXXXX");
	if (!mkdtemp (box->bpath)) return RERR_SYSTEM;
	box->rmbpath = 1;
	box->filterpath = asprtf ("%s/filter", box->bpath);
	if (!box->filterpath) return RERR_NOMEM;
	box->f_filterpath = 1;
	box->infopath = asprtf ("%s/info", box->bpath);
	if (!box->infopath) return RERR_NOMEM;
	box->f_infopath = 1;
	box->sock = asprtf ("%s/sock", box->bpath);
	if (!box->sock) return RERR_NOMEM;
	box->f_sock = 1;
	ret = sbox_open (box);
	if (!RERR_ISOK(ret)) return ret;
	box->isopen = 1;
	ret = box_writeout (box);
	if (!RERR_ISOK(ret)) return ret;
	box->rminfo = 1;
	if (chmod (box->bpath, 0755) < 0) return RERR_SYSTEM;
	ret = sbox_link (box);
	if (!RERR_ISOK(ret)) return ret;
	box->islink = 1;
	return RERR_OK;
}


static
int
sbox_open (box)
	struct eddibox	*box;
{
	int	ret;

	if (!box) return RERR_PARAM;
	switch (box->typ) {
	case EB_T_UNIX:
		box->sd = conn_open_server (box->sock, 0, CONN_T_UNIX, 10);
		if (box->sd < 0) return box->sd;
		SCON_NEW (box->scon);
		ret = SCON_ADD (box->scon, box->sd, scon_termnl, NULL, FD_T_SERVER);
		if (!RERR_ISOK(ret)) {
			conn_close (box->sd);
			box->sd = -1;
			return ret;
		}
		break;
	case EB_T_UNIXD:
		return RERR_NOT_SUPPORTED;
		break;
	case EB_T_MQUEUE:
		return RERR_NOT_SUPPORTED;
		break;
	default:
		return RERR_INVALID_TYPE;
	}
	return RERR_OK;
}

static
int
sbox_close (box)
	struct eddibox	*box;
{
	if (!box) return RERR_PARAM;
	if (!box->isopen) return RERR_OK;
	switch (box->typ) {
	case EB_T_UNIX:
		SCON_CLOSE (box->scon, box->sd);
		box->sd = -1;
		SCON_FREE (box->scon);
		break;
	case EB_T_UNIXD:
		return RERR_NOT_SUPPORTED;
		break;
	case EB_T_MQUEUE:
		return RERR_NOT_SUPPORTED;
		break;
	default:
		return RERR_INVALID_TYPE;
	}
	return RERR_OK;
}

static
int
box_writeout (box)
	struct eddibox	*box;
{
	FILE	*f;

	if (!box) return RERR_PARAM;
	f = fopen (box->infopath, "w");
	if (!f) return RERR_SYSTEM;
	switch (box->typ) {
	case EB_T_UNIX:
		fprintf (f, "type = unix\n");
		fprintf (f, "path = %s\n", box->sock);
	case EB_T_UNIXD:
		fprintf (f, "type = unixd\n");
		fprintf (f, "path = %s\n", box->sock);
	case EB_T_MQUEUE:
		fprintf (f, "type = mqueue\n");
		fprintf (f, "mqueue = %d\n", box->mqid);
	default:
		return RERR_INVALID_TYPE;
	}
	if (box->flags & EB_F_SIGNAL) {
		fprintf (f, "signal = %d\n", box->sig);
	}
	fprintf (f, "pid = %d\n", (int)getpid ());
	if (box->filterpath) {
		fprintf (f, "filter = %s\n", box->filterpath);
	}
	fprintf (f, "response = %s\n", (box->flags & EB_F_RESPONSE) ? "yes" : "no");
	return RERR_OK;
}


static
int
sbox_link (box)
	struct eddibox	*box;
{
	char	fpath[MAXPATH+1];
	int	ret;

	if (!box || !box->name || !box->sys) return RERR_PARAM;
	ret = maycreatebase (box->sys);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "cannot create base dir for sys >>%s<<: %s",
								box->sys, rerr_getstr3(ret));
		return ret;
	}
	ret = geteddibase (fpath, MAXPATH, box->sys, box->name);
	if (!RERR_ISOK(ret)) return ret;
	if (ret >= MAXPATH) return RERR_INVALID_FILE;
	if (symlink (box->bpath, fpath) < 0) return RERR_SYSTEM;
	return RERR_OK;
}

static
int
sbox_unlink (box)
	struct eddibox	*box;
{
	char	fpath[MAXPATH];
	int	ret;

	if (!box || !box->name || !box->sys) return RERR_PARAM;
	ret = geteddibase (fpath, MAXPATH, box->sys, box->name);
	if (!RERR_ISOK(ret)) return ret;
	if (ret >= MAXPATH) return RERR_INVALID_FILE;
	if (unlink (fpath) < 0) return RERR_SYSTEM;
	return RERR_OK;
}


static
int
geteddibase (buf, sz, sys, box)
	char			*buf;
	const char	*sys, *box;
	int			sz;
{
	int	num;

	if (!buf || sz <= 0) return RERR_PARAM;
	if (!sys) sys = g_eddidefsys;
	if (box) {
		num = snprintf (buf, sz, "%s/sys/%s/box/%s", g_eddibase, sys, box);
	} else {
		num = snprintf (buf, sz, "%s/sys/%s", g_eddibase, sys);
	}
	if (num < 0) return RERR_SYSTEM;
	return num;
}


static
int
maycreatebase (sys)
	char	*sys;
{
	struct stat	buf;
	int			ret, have=0;
	char			*lpath;
	char			tdir[32];

	CF_MAYREAD;
	if (stat (g_eddibase, &buf) < 0) {
		switch (errno) {
		case EACCES:
			return RERR_FORBIDDEN;
		case ENOENT:
			break;
		default:
			return RERR_SYSTEM;
		}
		ret = fop_mkdir_rec2 (g_eddibase);
		if (!RERR_ISOK(ret)) return ret;
		/* set permissions */
		if (chmod (g_eddibase, 0775) < 0) {
			fop_rmdir_rec (g_eddibase);
			return RERR_SYSTEM;
		}
	} else {
		if (!S_ISDIR (buf.st_mode)) return RERR_INVALID_FILE;
	}
	lpath = asprtf ("%s/sys", g_eddibase);
	if (!lpath) return RERR_NOMEM;
	have = 0;
	if (lstat (lpath, &buf) < 0) {
		if (errno != ENOENT) {
			free (lpath);
			return RERR_SYSTEM;
		}
	} else if (!S_ISDIR (buf.st_mode) && !S_ISLNK (buf.st_mode)) {
		unlink (lpath);
	} else if (S_ISLNK (buf.st_mode)) {
		if (stat (lpath, &buf) < 0) {
			if (errno != ENOENT) {
				free (lpath);
				return RERR_SYSTEM;
			}
			unlink (lpath);
		} else if (!S_ISDIR (buf.st_mode)) {
			unlink (lpath);
		} else {
			have = 1;
		}
	} else {
		have = 1;
	}
	if (!have) {
		strcpy (tdir, "/tmp/eddisys_XXXXXX");
		if (!mkdtemp (tdir)) {
			free (lpath);
			return RERR_SYSTEM;
		}
		if (symlink (tdir, lpath) < 0) {
			free (lpath);
			fop_rmdir_rec (tdir);
			return RERR_SYSTEM;
		}
		/* set permissions */
		if (chmod (tdir, 0775) < 0) {
			fop_rmdir_rec (tdir);
			unlink (lpath);
			free (lpath);
			return RERR_SYSTEM;
		}
		free (lpath);
	}
	/* now we have the base dir and can create the system dir */
	if (!sys) return RERR_OK;	/* we have no system to create */
	lpath = asprtf ("%s/systems/%s", g_eddibase, sys);
	if (!lpath) return RERR_NOMEM;
	have = 0;
	if (lstat (lpath, &buf) < 0) {
		if (errno != ENOENT) {
			free (lpath);
			return RERR_SYSTEM;
		}
	} else if (!S_ISDIR (buf.st_mode) && !S_ISLNK (buf.st_mode)) {
		unlink (lpath);
	} else {
		have = 1;
	}
	if (!have) {
		if (mkdir (lpath, 0770) < 0) {
			free (lpath);
			return RERR_SYSTEM;
		}
	}
	free (lpath);
	return RERR_OK;
}


static
int
read_config ()
{
	cf_begin_read ();
	g_eddibase = cf_getval2 ("eddibase", "/var/run/eddi");
	g_eddidefsys = cf_getval2 ("eddi_default_system", "eddi");
	config_read = 1;
	cf_end_read_cb (&read_config);
	return 1;
}


/***********************
 * boxlist functions 
 ***********************/

static struct tlst	boxlist;
static int				boxlist_init = 0;

static
int
boxlist_add (box)
	struct eddibox	*box;
{
	struct eddibox	*ptr;
	int				ret, i;

	if (!boxlist_init) {
		TLST_NEW (boxlist, struct eddibox);
		boxlist_init = 1;
	}
	if (!box) return RERR_PARAM;
	for (i=0; 1; i++) {
		ret = TLST_GET (ptr, boxlist, i);
		if (ret == RERR_NOT_FOUND) break;
		if (!RERR_ISOK(ret)) return ret;
		if (!ptr) break;
	}
	ret = TLST_SET (boxlist, i, box);
	if (!RERR_ISOK(ret)) return ret;
	return i;
}



static
int
boxlist_rm (nbox)
	int	nbox;
{
	void	*ptr = NULL;

	return TLST_SET (boxlist, nbox, ptr);
}


static
int
boxlist_get (box, nbox)
	struct eddibox	**box;
	int				nbox;
{
	return TLST_GET (*box, boxlist, nbox);
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
