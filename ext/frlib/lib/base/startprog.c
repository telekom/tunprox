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
#include <errno.h>
#ifdef SunOS
extern int errno;
#endif
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/wait.h>
#include <signal.h>


#include "config.h"
#include "errors.h"
#include "slog.h"
#include "iopipe.h"
#include "secureconfig.h"
#include "startprog.h"
#include "frinit.h"
#include "tmo.h"
#include "textop.h"
#include "setenv.h"
#include "prtf.h"



static int getscript (char**, int*, const char*, int*, const char*, const char*);
static int getscript1 (char**, const char**, int*, const char*, int*, const char*, const char*);
static int getscript2 (char**, const char**, const char*, int*, const char*, const char*);
static int getscript3 (char**, const char**, const char*, int, const char*, const char*);
static int needspoold (const char*, int);
static int need_daemonize (const char*, int);
static int checkscript (char**, const char**, int*, int);
static int dostartprog (const char*, const char*, int, int);
static int dostartprog2 (const char*, int, int);
static int startchild (int, const char*, const char*, int, int);
static int startchild2 (const char*, int, int);
static int dofather (int, pid_t, int);
static int dofather2 (int, pid_t, int);
static int send_msg (int, int, int, int);
static int recv_msg (int, int, int*, int*, int);
static const char* mygetarr2 (const char*, const char*, const char*);


int
startprog (prog, flags)
	const char	*prog;
	int			flags;
{
	return startprog2 (prog, flags, "-c", "-y");
}


int
startprog2 (prog, flags, cfgopt, scfopt)
	const char	*prog;
	int			flags;
	const char	*cfgopt, *scfopt;
{
	char	*script;
	int	ret, numscf;

	ret = getscript (&script, &numscf, prog, &flags, cfgopt, scfopt);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error getting script command for prog >>%s<<: %s",
								prog, rerr_getstr3 (ret));
		return ret;
	}
	if (!script) return RERR_INTERNAL;
	FRLOGF (LOG_DEBUG, "starting prog >>%s<< - script >>%s<<...", prog, script);
	ret = dostartprog (script, prog, numscf, flags);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error starting prog >>%s<< - script >>%s<<: %s",
					prog, script, rerr_getstr3 (ret));
	} else {
		FRLOGF (LOG_DEBUG, "prog >>%s<< - script >>%s<< started successfully",
					prog, script);
	}
	free (script);
	return ret;
}






/* **********************
 * static functions
 * **********************/


static
int
dostartprog (script, prog, numscf, flags)
	const char	*script, *prog;
	int			numscf, flags;
{
	pid_t	pid;
	int	ret, mid;

	if (!script || !*script || numscf < 0 || numscf > 2) return RERR_PARAM;
	/* create message queue */
	if ((mid = msgget (IPC_PRIVATE, 0600 | IPC_CREAT | IPC_EXCL)) < 0) {
		return RERR_SYSTEM;
	}
	/* fork once */
	pid = fork ();
	if (pid < 0) return RERR_SYSTEM;
	if (pid == 0) {
		slog_setpid (getpid());
		ret = startchild (mid, script, prog, numscf, flags);
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_INFO, "error starting child: %s", rerr_getstr3 (ret));
		}
		exit (ret);
	} else {
		return dofather (mid, pid, flags);
	}
	return RERR_OK;	/* never comes here */
}


static
int
dostartprog2 (script, numscf, flags)
	const char	*script;
	int			numscf, flags;
{
	int			ioflags = 0;
	const char	*scfpass="";

	ioflags = IOPIPE_F_RET_EXIT_CODE | IOPIPE_F_ONLY_Y;
	if (flags & STPRG_F_WATCH_ERRORS) {
		ioflags |= IOPIPE_F_WATCH_ERROR;
	} else {
		ioflags |= IOPIPE_F_RETURN_IMMEDIATE;
	}
	if (numscf > 0) {
		scfpass = scf_getpass ();
		if (!scfpass) scfpass = "";	/* send empty password */
	}
	switch (numscf) {
	case 0:
		return iopipef (NULL, 0, NULL, NULL, -1, ioflags, script);
	case 1:
		return iopipef (NULL, 0, NULL, NULL, -1, ioflags, script, scfpass);
	case 2:
		return iopipef (NULL, 0, NULL, NULL, -1, ioflags, script, scfpass, 
							scfpass);
	case 3:
		return iopipef (NULL, 0, NULL, NULL, -1, ioflags, script, scfpass, 
							scfpass, scfpass);
	default:
		return RERR_PARAM;
	}
	return RERR_INTERNAL;	/* never reach here */
}



static
int
startchild (mid, script, prog, numscf, flags)
	const char	*script, *prog;
	int			numscf, flags, mid;
{
	int	ret;

	ret = RERR_OK;
	if (flags & (STPRG_F_DAEMONIZE | STPRG_F_WATCH_ERRORS)) {
		ret = frdaemonize ();
	}
	if (!(!RERR_ISOK(ret))) {
		send_msg (mid, 41, (int) getpid (), 10);
	} else {
		send_msg (mid, 42, ret, 10);
	}
	setup_env_for (prog);
	ret = startchild2 (script, numscf, flags);
	send_msg (mid, 42, ret, 50);
	return ret;
}


static
int
startchild2 (script, numscf, flags)
	const char	*script;
	int			numscf, flags;
{
	int	ret;

	ret = dostartprog2 (script, numscf, flags);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "startprog::startchild(): script child >>%s<< returned "
						"with error: %s", script, rerr_getstr3 (ret));
		return ret;
	}
	return RERR_OK;
}


static
int
dofather (mid, pid, flags)
	int	flags, mid;
	pid_t	pid;
{
	int	ret, status;

	ret = dofather2 (mid, pid, flags);
	/* eliminate zombies */
	waitpid (-1, &status, WNOHANG);
	waitpid (-1, &status, WNOHANG);
	/* remove message queue */
	msgctl (mid, IPC_RMID, NULL);

	return ret;
}


static
int
dofather2 (mid, pid, flags)
	int	flags, mid;
	pid_t	pid;
{
	int	ret, status;
	int	nowait, data, mtype;
	pid_t	pid2;

	nowait = (flags & STPRG_F_NOWAIT);
	ret = recv_msg (mid, 0, &mtype, &data, (nowait ? 4 : 10));
	if (RERR_ISOK(ret)) {
		if (mtype == 41) {
			pid2 = (pid_t) data;
		} else if (mtype == 42) {
			ret = data;
		} else {
			ret = RERR_CHILD;
		}
	} else if (ret == RERR_NOT_FOUND) {
		ret = RERR_CHILD;
	} else {
		FRLOGF (LOG_ERR, "error receiving initial message from child: %s",
							rerr_getstr3 (ret));
	}
	if (!RERR_ISOK(ret)) {
		waitpid (pid, &status, WNOHANG);
		return ret;
	} else if (pid != pid2) {
		waitpid (pid, &status, WNOHANG);
		if (!WIFEXITED (status)) {
			kill (pid, SIGKILL);
			tmo_msleep (1);
			waitpid (pid, &status, WNOHANG);
		}
	}

	nowait = (flags & (STPRG_F_NOWAIT | STPRG_F_WATCH_ERRORS));
	ret = recv_msg (mid, 42, &mtype, &data, (nowait ? 2 : 80));
	if (RERR_ISOK (ret)) {
		ret = data;
		return ret;
	} else if (ret == RERR_NOT_FOUND) {
		ret = nowait ? RERR_OK : RERR_CHILD;
	}
	if (pid != pid2) return ret;
	if (waitpid (pid, &status, WNOHANG) >= 0) {
		if (WIFEXITED (status)) {
			ret = NEGWEXITSTATUS (status);
		} else {
			kill (pid, SIGTERM);
			tmo_msleep (1);
			waitpid (pid, &status, WNOHANG);
		}
	} else {
		kill (pid, SIGTERM);
		tmo_msleep (1);
		waitpid (pid, &status, WNOHANG);
	}
	return ret;
}


/* ************************
 * msg send/recv functions 
 * ************************/

struct mbuf {
	long	mtype;
	char	mtext[sizeof(int)];
};

static
int
send_msg (mid, mtype, data, maxtry)
	int	mid, mtype, data, maxtry;
{
	struct mbuf	mbuf;
	int			i;

	if (mid < 0 || mtype <= 0 || maxtry < 1) return RERR_PARAM;
	mbuf.mtype = mtype;
	*(int*)(void*)(mbuf.mtext) = (int) data;
	for (i=0; i<maxtry; i++) {
		if (msgsnd (mid, (struct msgbuf*)(void*)&mbuf, sizeof(int), 
											IPC_NOWAIT) < 0) {
			if (errno != EAGAIN) return RERR_SYSTEM;
		} else {
			return RERR_OK;
		}
		if (i < maxtry-1) tmo_msleep (10);
	}
	return RERR_NOT_FOUND;
}


static
int
recv_msg (mid, ftype, mtype, data, maxtry)
	int	mid, ftype, *mtype, *data, maxtry;
{
	struct mbuf	mbuf;
	int			i, num;

	if (mid < 0 || ftype < 0 || maxtry < 1) return RERR_PARAM;
	if (mtype) *mtype = ftype;
	if (data) *data = 0;
	bzero (&mbuf, sizeof (struct mbuf));
	for (i=0; i<maxtry; i++) {
		if ((num = msgrcv (mid, (struct msgbuf*)(void*)&mbuf, sizeof(int),
											ftype, IPC_NOWAIT)) >= 0) {
			if (mtype) *mtype = mbuf.mtype;
			if (data) *data = *(int*)(void*)mbuf.mtext;
			return RERR_OK;
		} else if (errno != ENOMSG) {
			return RERR_SYSTEM;
		}
		if (i < maxtry-1) tmo_msleep (10);
	}
	return RERR_NOT_FOUND;
}



/* **********************
 * getscript functions
 * **********************/

static
int
getscript (script, numscf, prog, flags, cfgopt, scfopt)
	char			**script;
	const char	*prog, *cfgopt, *scfopt;
	int			*flags, *numscf;
{
	int			ret;
	const char	*scfpass;

	cf_mayread ();
	cf_begin_read ();
	ret = getscript1 (script, &scfpass, numscf, prog, flags, cfgopt, scfopt);
	cf_end_read ();
	return ret;
}


static
int
getscript1 (script, scfpass, numscf, prog, flags, cfgopt, scfopt)
	char			**script;
	const char	**scfpass, *prog, *cfgopt, *scfopt;
	int			*flags, *numscf;
{
	int	ret;

	ret = getscript2 (script, scfpass, prog, flags, cfgopt, scfopt);
	if (!RERR_ISOK(ret)) return ret;
	ret = checkscript (script, scfpass, numscf, *flags);
	if (!RERR_ISOK(ret)) {
		free (*script);
		return ret;
	}
	return ret;
}


static
int
getscript2 (script, scfpass, prog, flags, cfgopt, scfopt)
	char			**script;
	const char	**scfpass, *prog, *cfgopt, *scfopt;
	int			*flags;
{
	int			usespoold;
	int			ret, len;
	char			*script2, *spoold, *s, *param, *param_spoold, *out;
	const char	*dummy;

	if (!script || !scfpass || !prog || !*prog || !flags) return RERR_PARAM;
	usespoold = needspoold (prog, *flags);
	ret = getscript3 (script, scfpass, prog, *flags, cfgopt, scfopt);
	if (!RERR_ISOK(ret)) return ret;
	if (!usespoold) {
		if (!((*flags & STPRG_F_NO_DAEMONIZE) || (*flags & STPRG_F_DAEMONIZE))) {
			if (need_daemonize (prog, *flags)) *flags |= STPRG_F_DAEMONIZE;
		}
		if (!((*flags & STPRG_F_WATCH_ERRORS) || 
					(*flags & STPRG_F_NO_WATCH_ERRORS))) {
			if (cf_isyes (cf_getarr2 ("prog_watch_errors", prog, "no"))) {
				*flags |= STPRG_F_WATCH_ERRORS;
			}
		}
		return RERR_OK;
	}
	*flags &= ~STPRG_F_DAEMONIZE;
	if (!(*flags & STPRG_F_NO_WATCH_ERRORS)) *flags |= STPRG_F_WATCH_ERRORS;
	script2 = *script;
	*script = NULL;
	script2 = top_skipwhite (script2);
	if (!script2 || !*script2) {
		if (script2) free (script2);
		return RERR_INTERNAL;
	}
	ret = getscript3 (&spoold, &dummy, "spoold", *flags | STPRG_F_NO_SPOOLD,
							"-c", "-y");
	if (!RERR_ISOK(ret)) {
		free (script2);
		return ret;
	}
	if (dummy && !scfpass) *scfpass = dummy;
	for (s=script2; *s && iswhite (*s); s++);
	if (*s) {
		*s = 0;
		s = top_skipwhite (s+1);
	}
	param = *s ? s : NULL;
	for (s=spoold; *s && iswhite (*s); s++);
	if (*s) {
		*s = 0;
		s = top_skipwhite (s+1);
	}
	param_spoold = *s ? s : NULL;
	len = strlen (script2) + strlen (spoold) + 6;
	if (param) len += strlen (param) + 4;
	if (param_spoold) len += strlen (param_spoold) + 2;
	out = malloc (len);
	if (!out) {
		free (script2);
		free (spoold);
		return RERR_NOMEM;
	}
	sprintf (out, "%s -p %s%s%s%s%s", spoold, script2, (param_spoold?" ":""), 
							(param_spoold?param_spoold:""), (param?" -- ":""), 
							(param?param:""));
	free (script2);
	free (spoold);
	*script = out;
	return RERR_OK;
}


static
int
getscript3 (script, scfpass, prog, flags, cfgopt, scfopt)
	char			**script;
	const char	**scfpass, *prog, *cfgopt, *scfopt;
	int			flags;
{
	int			pass_scf, pass_cfg, haveslash;
	const char	*cfg_file = NULL, *scf_pass = NULL;
	const char	*s, *prog2, *passmore, *passmore2;
	int			len;
	char			*obuf, *buf, _buf[128];
	int			usespoold, need_scf = 0;

	if (!script || !scfpass || !prog || !*prog) return RERR_PARAM;
	pass_scf = scfopt && !(flags & STPRG_F_NO_SCF);
	pass_cfg = cfgopt && !(flags & STPRG_F_NO_CFG);
	if (pass_cfg) {
		cfg_file = cf_get_cfname ();
		if (!cfg_file) pass_cfg = 0;
	}
	scf_pass = scf_getpass ();
	if (!scf_pass) pass_scf = 0;
	for (haveslash = 0, s=prog; *s && !iswhite (*s); s++) {
		if (*s == '/') haveslash = 1;
	}
	if (s == prog) return RERR_PARAM;
	passmore = *s ? s+1 : NULL;
	passmore2 = NULL;
	buf = a2sprtf (_buf, sizeof (_buf), "%.*s", (int)(s-prog), prog);
	if (!buf) return RERR_NOMEM;
	usespoold = needspoold (buf, flags);
	if (usespoold) {
		prog2 = prog;
	} else if (!haveslash) {
		prog2 = cf_getarr2 ("prog", buf, buf);
		if (prog2 != buf) {
			for (s=prog2; *s && !iswhite (*s); s++);
			passmore2 = *s ? s+1 : NULL;
		}
	} else {
		prog2 = prog;
	}
	if (pass_cfg && usespoold && !strcmp (cfgopt, "-c")) pass_cfg = 0;
	if (pass_scf && usespoold && !strcmp (scfopt, "-y")) pass_scf = 0;
	if (pass_cfg && passmore) {
		len = strlen (cfgopt);
		for (s=passmore; *s; s++) {
			if (!strncmp (s, cfgopt, len) && iswhite (s[len])) {
				pass_cfg = 0;
			}
		}
	}
	if (pass_cfg && passmore2) {
		len = strlen (cfgopt);
		for (s=passmore2; *s; s++) {
			if (!strncmp (s, cfgopt, len) && iswhite (s[len])) {
				pass_cfg = 0;
			}
		}
	}
	if (passmore) {
		for (s=passmore; *s; s++) {
			if (!strncasecmp (s, "%y", 2) && iswhite (s[2])) {
				pass_scf = 0;
				need_scf = 1;
			}
		}
	}
	if (passmore2) {
		for (s=passmore2; *s; s++) {
			if (!strncasecmp (s, "%y", 2) && iswhite (s[2])) {
				pass_scf = 0;
				need_scf = 1;
			}
		}
	}
	if (pass_scf) need_scf = 1;
	if (prog2 == buf) passmore = NULL;
	len = strlen (prog2) + 2;
	if (passmore) len += strlen (passmore) + 1;
	if (pass_cfg) len += strlen (cfgopt) + strlen (cfg_file) + 2;
	if (pass_scf) len += strlen (scfopt) + 4;
	obuf = malloc (len);
	if (!obuf) {
		if (buf != _buf) free (buf);
		return RERR_NOMEM;
	}
	len = sprintf (obuf, "%s", prog2);
	if (passmore) len += sprintf (obuf+len, " %s", passmore);
	if (pass_cfg) len += sprintf (obuf+len, " %s %s", cfgopt, cfg_file);
	if (pass_scf) len += sprintf (obuf+len, " %s %sy", scfopt, 
													(usespoold ? "%%" : "%"));
	*script = obuf;
	if (buf != _buf) free (buf);
	if (scfpass) *scfpass = need_scf ? scf_pass : NULL;
	return RERR_OK;
}



static
int
needspoold (prog, flags)
	const char	*prog;
	int			flags;
{
	return cf_isyes (mygetarr2 ("start_spoold", prog, "no"));
}

static
int
need_daemonize (prog, flags)
	const char	*prog;
	int			flags;
{
	return cf_isyes (mygetarr2 ("prog_daemonize", prog, "no"));
}

static
const char*
mygetarr2 (var, prog, def)
	const char	*var, *def, *prog;
{
	const char	*s, *val;
	char			*buf, _buf[128], *s2;

	if (!var || !prog) return NULL;
	for (s=prog; *s && !iswhite (*s); s++);
	buf = a2sprtf (_buf, sizeof (_buf), "%.*s", (int)(s-prog), prog);
	if (!buf) return NULL;
	val = cf_getarr (var, buf);
	if (!val) {
		s2 = rindex (buf, '/');
		if (s2) val = cf_getarr (var, s2+1);
	}
	if (buf != _buf) free (buf);
	return val ? val : def;
}


static
int
checkscript (script, scfpass, numscf, flags)
	char			**script;
	const char	**scfpass;
	int			flags, *numscf;
{
	char	*s, *script2, *s2, *s3;
	int	numperc = 0, scfforce=0;
	char	*out=NULL;
	int	havescf;

	if (!script || !numscf || !scfpass) return RERR_PARAM;
	if (!*script || !**script) return RERR_INVALID_CMD;
	script2 = *script;
	/* check for number of % signs to quote */
	for (s=script2; *s; s++) {
		if (*s != '%') continue;
		s++;
		if (!*s) {
			numperc++;
			break;
		}
		if (*s == '%' || *s == 'y' || *s == 'Y') {
			if (*s == 'y') scfforce = 1;
			continue;
		}
		numperc++;
	}
	if (numperc) {
		out = malloc (strlen (script2) + numperc + 2);
		if (!out) return RERR_NOMEM;
		/* copy script to out quoting % signs */
		for (s=script2, s2=out; *s; s++, s2++) {
			*s2 = *s;
			if (*s != '%') continue;
			if (s[1] == '%') {
				s++; s2++;
				*s2 = '%';
				continue;
			}
			if (s[1] == 'y' || s[2] == 'Y') continue;
			*++s2 = '%';
		}
		free (script2);
		*script = script2 = out;
	}
	/* eliminate %Y */
	havescf = *scfpass ? 1 : 0;
	for (s=script2; *s; s++) {
		if (!strncmp (s, " -- ", 4)) break;	/* don't parse cmds of spoold */
		if (!strncmp (s, "%%", 2)) {
			s++;
			continue;
		}
		if (strncmp (s, "%Y", 2) != 0) continue;
		if (havescf) {
			s[1] = 'y';
			continue;
		}
		for (s2=s-1; s2>=script2 && iswhite (*s2); s2--);
		for (s2--; s2>=script2 && !iswhite (*s2); s2--);
		if (s2 <= script2) return RERR_INVALID_CMD;
		s3 = top_skipwhite (s+1);
		s = s2 - 1;
		for (; *s3; s3++, s2++) *s2 = *s3;
		*s2 = 0;
	}
	/* scfpass force? */
	if (scfforce && !havescf) {
		*scfpass = "";		/* send an empty password */
	}
	/* count number of %y */
	*numscf = 0;
	for (s = script2; *s; s++) {
		if (!strncmp (s, "%%", 2)) {
			s++;
			continue;
		}
		if (!strncmp (s, "%y", 2)) ++*numscf;
	}
	if (*numscf > 2) return RERR_INVALID_CMD;
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
