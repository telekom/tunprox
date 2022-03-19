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
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netdb.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/file.h>
#include <time.h>
#include <ctype.h>
#include <stdarg.h>


#include "strcase.h"
#include "iopipe.h"
#include "errors.h"
#include "slog.h"
#include "config.h"
#include "prtf.h"
#include "tmo.h"
#include "textop.h"
#include "frinit.h"
#include "procop.h"


struct fd_pass {
	int			fdr, fdw;
	const char	*pass;
};

static int setNonblocking (int);
static pid_t forkCommand (const char **, int*, int*, int*, int, int, int, 
							struct fd_pass*, int);
static int start_write_child (const char *, int, int, struct fd_pass*, int);
static int parse_args (const char*, va_list, char**, struct fd_pass**, int*, int);
static int checkforchild (char*, pid_t, int);
static char *getendoftoken (char*);

#ifdef SunOS
extern int usleep (useconds_t);
#endif
static int read_config ();
static int config_read = 0;

static int debug_output = 0;


static
pid_t
forkCommand (argl, pofd, pefd, pifd, ofd, efd, ifd, fplist, numfp)
	const char		**argl;
	int				*pofd;		/* pipe fd for stdout; if NULL don't create pipe */
	int				*pefd;		/* pipe fd for stderr; if NULL don't create pipe */
	int				*pifd;		/* pipe fd for stdin; if NULL don't create pipe */
	int				ofd;		/* if >= 0 write stdout to ofd instead to pipe */
	int				efd;		/* if >= 0 write stderr to efd instead to pipe */
	int				ifd;		/* if >= 0 read stdin from ifd instead from pipe */
	struct fd_pass	*fplist;
	int				numfp;
{
	int	outpipe[2];
	int	errpipe[2];
	int	inpipe[2];
	pid_t	pid;
	int	closeout=0, closeerr=0, closein=0;
	int	pipeout=0, pipeerr=0, pipein=0;
	int	fdout, fderr, fdin;
	int	ret,i,j;

	/* creat pipe for stdout */
	if (pofd != NULL && ofd < 0) {
		closeout = 1;
		pipeout = 1;
		if (pipe (outpipe) == -1) {
			ret = RERR_SYSTEM;
			FRLOGF (LOG_ERR, "error in pipo(outpipe): %s", rerr_getstr3(ret));
			return ret;
		}
		fdout = outpipe[1];
		*pofd = outpipe[0];
	} else if (ofd >= 0) {
		fdout = ofd;
	} else {
		fdout = open ("/dev/null", O_WRONLY);
		if (fdout != -1) closeout = 1;
	}
	/* in case something goes wrong write to stderr */
	if (fdout < 0) fdout = 2;

	/* create pipe for stderr */
	if (pefd != NULL && efd < 0) {
		closeerr = 1;
		pipeerr = 1;
		if (pipe (errpipe) == -1) {
			ret = RERR_SYSTEM;
			FRLOGF (LOG_ERR, "error in pipo(errpipe): %s", rerr_getstr3(ret));
			return ret;
		}
		fderr = errpipe[1];
		* pefd = errpipe[0];
	} else if (efd >= 0) {
		fderr = efd;
	} else {
		fderr = open ("/dev/null", O_WRONLY);
		if (fderr != -1) closeerr = 1;
	}
	/* in case something goes wrong write to stderr */
	if (fderr < 0) fderr = 2;

	/* create pipe for stdin */
	if (pifd != NULL && ifd < 0) {
		closein = 1;
		pipein = 1;
		if (pipe (inpipe) == -1) {
			ret = RERR_SYSTEM;
			FRLOGF (LOG_ERR, "error in pipo(inpipe): %s", rerr_getstr3(ret));
			return ret;
		}
		fdin = inpipe[0];
		* pifd = inpipe[1];
	} else if (ifd <= 0) {
		fdin = ifd;
	} else {
		fdin = -1;
	}

	/* fork child */
	pid = fork ();
	if (pid < 0) {
		/* error */
		ret = RERR_SYSTEM;
		FRLOGF (LOG_ERR, "error in fork: %s", rerr_getstr3 (ret));
		if (pipeout) {
			close (outpipe[0]);
			close (outpipe[1]);
		}
		if (pipeerr) {
			close (errpipe[0]);
			close (errpipe[1]);
		}
		if (pipein) {
			close (inpipe[0]);
			close (inpipe[1]);
		}
		return ret;
	} 
	if (pid > 0) {
		/* father */
		if (closeout) close (fdout);
		if (closeerr) close (fderr);
		if (closein) close (fdin);
		return pid;
	}
	/* child */
	slog_setpid (getpid());
	dup2 (fdout, 1);	/* redirect stdout */
	dup2 (fderr, 2);	/* redirect stderr */
	if (fdin != -1) dup2 (fdin, 0);	/* redirect stdin */
	/* close all other file descriptors */
	int openmax = sysconf(_SC_OPEN_MAX);
	for (i=3; i<openmax; i++) {
		for (j=0; j<numfp; j++)
			if (fplist[j].fdr == i) break;
		if (j>=numfp)
			close (i);
	}
	/* reset all signals to SIG_DFL */
	for (i=0; i<64; i++)
		signal (i, SIG_DFL);

	/* start the script */
	execvp (argl[0], (char * const*)argl);
	/* if we come here an error occurred */
	FRLOGF (LOG_ERR, "cannot exec >>%s<<: %s", argl[0],
				rerr_getstr3 (RERR_SYSTEM));
	_exit (127);	/* 127 will be converted to RERR_SYSTEM in father */
	return -1;	/* to make compiler happy */
}



static
int
start_write_child (in, inlen, pifd, fplist, numfp)
	const char		*in;
	int				inlen;
	int				pifd;
	struct fd_pass	*fplist;
	int				numfp;
{
	pid_t		pid;
	int		wlen;
	int		ret, num, i, max;
	fd_set	fdset;

	pid = fork();
	if (pid < 0) {
		/* error */
		ret = RERR_SYSTEM;
		FRLOGF (LOG_ERR, "error forking write child: %s",
					rerr_getstr3(ret));
		if (pifd >= 0) close (pifd);
		return ret;
	}
	if (pid > 0) {
		/* father */
		FRLOGF (LOG_VVERB, "child forked with pid %d", (int) pid);
		if (pifd >= 0) close (pifd);
		return pid;
	}
	FRLOGF (LOG_VVERB, "hello i'm the write child and my pid is %d",
				(int) getpid());
	/* write to pipes */
	for (i=0; i<numfp; i++) 
		if (fplist[i].pass == NULL) fplist[i].pass = "";
#if 0
	num = numfp + (pifd >= 0 ? 1 : 0);
#else
	num = numfp;
#endif
	while (num) {
		FRLOGF (LOG_VVERB, "still %d secrets to pass()", num);
		FD_ZERO (&fdset);
		max = -1;
#if 0
		if (pifd >= 0) {
			FD_SET (pifd, &fdset);
			max = pifd;
		}
#endif
		for (i=0; i<numfp; i++) {
			if (!fplist[i].pass) continue;
			FD_SET (fplist[i].fdw, &fdset);
			if (fplist[i].fdw > max) max = fplist[i].fdw;
		}
select_restart:
		if (select (max+1, NULL, &fdset, NULL, NULL) == -1) {
			if (errno == EINTR) { // 4 == EINTR
				/* some signal interrupted us, so restart */
				goto select_restart;
			}
			ret = RERR_SYSTEM;
			FRLOGF (LOG_ERR, "error in select: %s", rerr_getstr3(ret));
			exit (ret);
		}
		for (i=0; i<numfp; i++) {
			if (!fplist[i].pass) continue;
			if (FD_ISSET (fplist[i].fdw, &fdset)) {
				if (write (fplist[i].fdw, fplist[i].pass, 
						strlen (fplist[i].pass)+1) < 0) {
					FRLOGF (LOG_ERR, "error writing password to pipe: %s",
								rerr_getstr3(RERR_SYSTEM));
				}
				fplist[i].pass = NULL;
				close (fplist[i].fdw);
				num--;
			}
		}
#if 0
		if (pifd >= 0) {
			if (FD_ISSET (pifd, &fdset)) {
				wlen = write (pifd, in, inlen);
				if (inlen != wlen) {
					ret = RERR_SYSTEM;
					FRLOGF (LOG_ERR, "error writing data to pipe: %s", inlen<0?
								rerr_getstr3(ret):"write truncated");
					exit (ret);
				}
				pifd = -1;
				num--;
			}
		}
#endif
	}
	if (pifd >= 0) {
		wlen = write (pifd, in, inlen);
		if (inlen != wlen) {
			ret = RERR_SYSTEM;
			FRLOGF (LOG_ERR, "error writing data to pipe: %s", inlen<0?
						rerr_getstr3(ret):"write truncated");
			exit (ret);
		}
	}

	FRLOGF (LOG_VVERB, "the write child says bye bye, have a nice time...");
	exit (0);
	return 0;	/* to make compiler happy */
}




int
iopipe64 (script, in, inlen, out, outlen, timeout, flags)
	const char 	*script, *in;
	char						**out;
	int						inlen, *outlen;
	tmo_t						timeout;
	int						flags;
{
	return iopipef64 (in, inlen, out, outlen, timeout, flags |
							IOPIPE_F_NOFMT, script);
}

int
iopipe (script, in, inlen, out, outlen, timeout, flags)
	const char 	*script, *in;
	char			**out;
	int			inlen, *outlen;
	int			timeout, flags;
{
	return iopipef (	in, inlen, out, outlen, timeout, flags |
							IOPIPE_F_NOFMT, script);
}

int
iopipearg64 (argl, in, inlen, out, outlen, timeout, flags)
	const char 	**argl, *in;
	char			**out;
	int			inlen, *outlen;
	tmo_t			timeout;
	int			flags;
{
	return iopipef64 (in, inlen, out, outlen, timeout, flags |
							IOPIPE_F_SCRIPT_IS_ARGL, (const char*)(void*)argl);
}

int
iopipearg (argl, in, inlen, out, outlen, timeout, flags)
	const char 	**argl, *in;
	char			**out;
	int			inlen, *outlen;
	int			timeout, flags;
{
	return iopipef (	in, inlen, out, outlen, timeout, flags |
							IOPIPE_F_SCRIPT_IS_ARGL, (char*)(void*)argl);
}



int
iopipef64 (
	const char	*in,
	int			inlen,
	char			**out,
	int			*outlen,
	tmo_t			timeout,
	int			flags,
	const char 	*script,
	...)
{
	va_list	ap;
	int		ret;

	va_start (ap, script);
	ret = viopipef64 (in, inlen, out, outlen, timeout, flags, script, ap);
	va_end (ap);
	return ret;
}

int
iopipef (
	const char	*in,
	int			inlen,
	char			**out,
	int			*outlen,
	int			timeout,
	int			flags,
	const char 	*script,
	...)
{
	va_list	ap;
	int		ret;

	va_start (ap, script);
	ret = viopipef (in, inlen, out, outlen, timeout, flags, script, ap);
	va_end (ap);
	return ret;
}

int
viopipef (in, inlen, out, outlen, timeout, flags, script, ap)
	const char 	*script, *in;
	char			**out;
	int			inlen, *outlen;
	int			timeout, flags;
	va_list		ap;
{
	tmo_t	tout;

	tout = (tmo_t) timeout;
	if (tout > 0) tout *= 1000000LL;
	return viopipef64 (in, inlen, out, outlen, tout, flags, script, ap);
}

int
viopipef64 (in, inlen, out, outlen, timeout, flags, script, ap)
	const char 	*script, *in;
	char			**out;
	int			inlen, *outlen;
	tmo_t			timeout;
	int			flags;
	va_list		ap;
{
	pid_t				pid, wpid;
	int				pofd=-1, pifd=-1, pefd=-1;
	int				rc, rc2;
	const char		**argl;
	tmo_t				starttime;
	tmo_t				endtime;
	char				buf[256];
	char				*obuf, *ebuf;
	int				num;
	int				havercv;
	int				ret;
	int				buflen, wlen;
	int				ebuflen, elen;
	char				*line, *str, *s;
	const char		*cs;
	int				do_debug;
	struct fd_pass	*fplist = NULL;
	char				*oscript;
	int				numfp;
	int				wexited=0, wperr=0, havewritechild;
	int				i, use_sh, olevel, mlevel;
	int				abstimeout, retimm, watcherr;
	int				multiline;
	char				tfile[sizeof("/tmp/iopipe_XXXXXX")+1];
	char				errcmd[32];
	int				tfd=-1;

	CF_MAY_READ;
	if (!script) {
		FRLOGF (LOG_ERR, "script not specified\n");
		return RERR_PARAM;
	}
	do_debug = flags & IOPIPE_F_NODEBUG ? 0 : 1;
	use_sh = flags & IOPIPE_F_USE_SH ? 1 : 0;
	multiline = flags & IOPIPE_F_MULTILINE ? 1 : 0;
	abstimeout = flags & IOPIPE_F_TIMEOUT_IS_ABS ? 1 : 0;
	retimm = flags & IOPIPE_F_RETURN_IMMEDIATE ? 1 : 0;
	watcherr = flags & IOPIPE_F_WATCH_ERROR ? 1 : 0;
	if (multiline) use_sh = 1;
	*errcmd = 0;
	if (!(flags & IOPIPE_F_SCRIPT_IS_ARGL)) {
		if (!(flags & IOPIPE_F_NOFMT)) {
			ret = parse_args (script, ap, &oscript, &fplist, &numfp, flags);
			if (!RERR_ISOK (ret)) {
				FRLOGF (LOG_ERR, "error parsing argument list (%s)", 
								rerr_getstr3(ret));
				return ret;
			}
		} else {
			oscript = strdup (script);
			numfp = 0;
			fplist = NULL;
		}
		if (do_debug) {
			FRLOGF (LOG_DEBUG, "start command >>%s<<", oscript);
		}
	} else {
		numfp = 0;
		fplist = NULL;
	}
	
	/* create argument list */
	if (flags & IOPIPE_F_SCRIPT_IS_ARGL) {
		argl = (const char**)(void*)script;
	} else if (use_sh) {
		if (multiline && !index (script, '\n')) multiline = 0;
		if (multiline) {
			strcpy (tfile, "/tmp/iopipe_XXXXXX");
			tfd = mkstemp (tfile);
			if (tfd < 0) {
				free (oscript);
				for (i=0; i<numfp; i++) {
					close (fplist[i].fdr);
					close (fplist[i].fdw);
				}
				free (fplist);
				return RERR_SYSTEM;
			}
			write (tfd, oscript, strlen (oscript));
			close (tfd);
		}
		argl = malloc (4*sizeof (char*));
		if (!argl) {
			free (oscript);
			for (i=0; i<numfp; i++) {
				close (fplist[i].fdr);
				close (fplist[i].fdw);
			}
			free (fplist);
			if (multiline) unlink (tfile);
			return RERR_NOMEM;
		}
		argl[0] = getenv ("SHELL");
		if (!argl[0]) argl[0] = "/bin/sh";
		if (!multiline) {
			argl[1] = "-c";
			argl[2] = oscript;
			argl[3] = NULL;
		} else {
			argl[1] = tfile;
			argl[2] = NULL;
		}
		if (watcherr) {
			for (str=s=oscript; !iswhite (*s) && !index ("|<>", *s); s++) {
				if (*s == '/') str = s+1;
			}
			num = s-str < (ssize_t)sizeof(errcmd)-1 ? s-str : (ssize_t)sizeof(errcmd)-1;
			strncpy (errcmd, str, num);
			errcmd[num]=0;
		}
	} else {
		ret = iopipe_parsecmd (&argl, oscript);
		if (!RERR_ISOK (ret)) {
			FRLOGF (LOG_ERR, "error parsing script command: %s",
									 rerr_getstr3(ret));
			free (oscript);
			for (i=0; i<numfp; i++) {
				close (fplist[i].fdr);
				close (fplist[i].fdw);
			}
			free (fplist);
			return ret;
		}
		if (watcherr) {
			cs = (cs = argl ? rindex (argl[0], '/') : NULL) ? cs + 1 : 
					(argl ? argl[0] : NULL);
			if (!cs) cs = "<NULL>";
			strncpy (errcmd, cs, sizeof(errcmd)-1);
			errcmd[sizeof(errcmd)-1]=0;
		}
	}

	/* fork child */
	if (retimm) out = NULL;
	pid = ret = forkCommand (	argl, out?&pofd:NULL, retimm?NULL:&pefd,
										in?&pifd:NULL, -1, -1, -1, fplist, numfp);
	if (!(flags & IOPIPE_F_SCRIPT_IS_ARGL)) {
		free (argl);
		free (oscript);
	}
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "cannot fork (forkCommand) (numfp=%d) "
							">>%s<<", numfp, rerr_getstr3(ret));
		for (i=0; i<numfp; i++) {
			close (fplist[i].fdr);
			close (fplist[i].fdw);
		}
		free (fplist);
		if (multiline) unlink (tfile);
		return ret;
	}

	/* set fds non blocking */
	if (out) setNonblocking (pofd);
	if (!retimm) setNonblocking (pefd);

	/* get time when launched */
	if (timeout > 0) starttime = tmo_now ();

	/* start writing child */
	havewritechild = in || numfp;
	if (havewritechild) {
		wpid = ret = start_write_child (in, inlen, in?pifd:-1, fplist, numfp);
		for (i=0; i<numfp; i++) {
			close (fplist[i].fdr);
			close (fplist[i].fdw);
		}
		free (fplist);
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_ERR, "cannot fork write child (numfp=%d) >>%s<<", numfp,
									rerr_getstr3(ret));
			if (pofd > 2) close (pofd);
			if (pefd > 2) close (pefd);
			if (multiline) unlink (tfile);
			/* error occurred */
			kill (pid, SIGTERM);
			tmo_msleep (100);
			kill (pid, SIGKILL);
			waitpid (pid, NULL, WNOHANG);
			return ret;
		}
	} else {
		wpid = -1;
		if (fplist) free (fplist);
	}

	if (retimm) {
		ret = RERR_OK;
		if (flags & IOPIPE_F_RET_EXIT_CODE) {
			ret = checkforchild (errcmd, pid, flags);
		}
		/* remove zombies */
		waitpid (-1, NULL, WNOHANG);
		waitpid (-1, NULL, WNOHANG);
		if (multiline) { tmo_msleep (100); unlink (tfile); }
		return ret;
	}

	/* start monitoring the child */

	obuf = ebuf = NULL;
	buflen = ebuflen = 0;
	wlen = elen = 0;
	havercv = 0;

	while (1) {
		while (pefd >= 0 && (num = read (pefd, buf, 255)) > 0) {
			buf[num] = 0;
			if (watcherr && num > 0) {
				s = index (buf, '\n');
				if (s) {
					*s = 0;
					FRLOGF (LOG_INFO, "%s(%d): %s%s", errcmd, (int) pid,
								ebuf?ebuf:"", buf);
					if (ebuf) ebuf[0] = 0;
					elen = 0;
					for (str=s+1; (s = index (str, '\n')); str=s+1) {
						*s = 0;
						FRLOGF (LOG_INFO, "%s(%d): %s", errcmd, (int) pid, str);
					}
					num = strlen (str);
				} else {
					str = buf;
				}
			} else {
				str = buf;
			}
			if (num > 0) {
				if (elen + num + 1 >= ebuflen) {
					ebuflen += 1024;
					ebuf = realloc (ebuf, ebuflen);
					if (!ebuf) {
						ebuflen=0;
						continue;
					}
				}
				strncpy (ebuf+elen, str, num);
				elen+= num;
				ebuf[elen] = 0;
			}

			havercv = 1;
		}
		if (out && pofd >=0) {
			while ((num = read (pofd, buf, 255)) > 0) {
				buf[num] = 0;
				if (wlen + num + 1>= buflen) {
					buflen += 1024;
					obuf = realloc (obuf, buflen);
					if (!obuf) {
						if (pofd > 2) close (pofd);
						if (pefd > 2) close (pefd);
						/* kill both children */
						if (wpid > 0) kill (wpid, SIGTERM);
						kill (pid, SIGTERM);
						tmo_msleep (100);
						if (wpid > 0) kill (wpid, SIGKILL);
						kill (pid, SIGKILL);
						if (ebuf) free (ebuf);
						if (multiline) unlink (tfile);
						return RERR_NOMEM;
					}
				}
				memcpy (obuf+wlen, buf, num);
				wlen+= num;
				obuf[wlen] = 0;

				havercv = 1;
			}
		}
		if (havercv && timeout > 0 && !abstimeout) starttime = tmo_now ();
		havercv = 0;
		/* check for child */
		ret = waitpid(pid,&rc,WNOHANG);
		if (ret < 0) {
			FRLOGF (LOG_WARN, "error in waitpid on child: %s",
								rerr_getstr3(RERR_SYSTEM));
			wperr = 1;
			kill (pid, SIGKILL);
			break;
		} else if (ret > 0) {
			break;
		}
		/* check wether the write child has exited abnormally */
		if (!wexited && waitpid(wpid,&rc2,WNOHANG) >= 0) {
			if (WIFEXITED (rc2)) {
				wexited = 1;
				if (WEXITSTATUS(rc2)!=0) {
					FRLOGF (LOG_NOTICE, "write child (%d) exited with "
									"exit status: %d", (int) wpid,
									NEGWEXITSTATUS(rc2));
#if 0		/* ignore error */
					if (pofd > 2) close (pofd);
					if (pefd > 2) close (pefd);
					kill (pid, SIGTERM);
					tmo_msleep (100);
					kill (pid, SIGKILL);
					if (ebuf) free (ebuf);
					if (obuf) free (obuf);
					waitpid (-1, NULL, WNOHANG);
					if (multiline) unlink (tfile);
					return RERR_CHILD;
#endif
				}
			}
		} else if (!wexited) {
			FRLOGF (LOG_NOTICE, "error in waitpid on write child: %s",
								rerr_getstr3(RERR_SYSTEM));
		}

		/* check wether we have reached timeout */
		if (timeout > 0) {
			endtime = tmo_now ();
			if (endtime-starttime > timeout) {
				if (pofd > 2) close (pofd);
				if (pefd > 2) close (pefd);
				kill (pid, SIGTERM);
				if (wpid > 0) kill (wpid, SIGTERM);
				tmo_msleep (100);
				kill (pid, SIGKILL);
				if (wpid > 0) kill (wpid, SIGKILL);
				FRLOGF (LOG_ERR, "timedout");
				if (ebuf) free (ebuf);
				if (obuf) free (obuf);
				waitpid (-1, NULL, WNOHANG);
				waitpid (-1, NULL, WNOHANG);
				if (multiline) unlink (tfile);
				return RERR_TIMEDOUT;
			}
		}
		tmo_msleep (100);
	}

	if (ebuf && elen > 0) {
		if (watcherr) {
			FRLOGF (LOG_INFO, "%s(%d): %s", errcmd, pid, ebuf);
		} else if (do_debug) {
			/* write ebuf to log file */
			FRLOGF (LOG_INFO, "--- start error output: ---");
			line = ebuf;
			while (line && *line) {
				s = index (line, '\n');
				if (s) {
					*s = 0;
					s++;
				}
				FRLOGF (LOG_INFO, "   %s", line);
				line = s;
			}
			FRLOGF (LOG_INFO, " --- end error output ---");
		}
		free (ebuf);
		ebuf = NULL;
		ebuflen = 0;
	}
	/* write obuf to log file */
	if (obuf && do_debug && debug_output) {
		olevel = out?LOG_VVERB:LOG_VERB;
		mlevel = shlog_minlevel (SLOG_H_FRLIB);
		if (olevel <= mlevel) {
			FRLOGF (olevel, "--- start output: ---");
			line = obuf;
			while (line && *line) {
				s = index (line, '\n');
				if (s) {
					*s = 0;
				}
				FRLOGF (olevel, "   %s", line);
				if (s) {
					*s = '\n';
					s++;
				}
				line = s;
			}
			FRLOGF (olevel, "--- end output ---");
			if (!out) {
				free (obuf);
				obuf = NULL;
				buflen = 0;
			}
		}
	}
	ret = RERR_OK;
	if (!wperr) {
		if (WIFEXITED(rc)) {
			if (WEXITSTATUS (rc) != 0) {
				if (do_debug) {
					FRLOGF (LOG_ERR, "script child exit status %d", 
									NEGWEXITSTATUS(rc));
				}
				if (WEXITSTATUS(rc) == 127) {
					ret = RERR_SYSTEM;
				} else if (flags & IOPIPE_F_RET_EXIT_CODE) {
					ret = NEGWEXITSTATUS(rc);
				} else {
					ret = RERR_SCRIPT;
				}
			}
		} else if (WIFSIGNALED (rc)) {
			int		sig, core, level;

			sig = WTERMSIG (rc);
#ifdef WCOREDUMP
			core = WCOREDUMP (rc);
#else
			core = 0;
#endif
			level = ((sig == SIGTERM) || (sig == SIGKILL)) ? LOG_DEBUG : 
													LOG_NOTICE;
			if (core) {
				level = LOG_ERR;
				ret = RERR_SCRIPT;
			}
			FRLOGF (level, "script child terminated by signal %d%s",
							sig, (core?" and core dumped":""));
		} else {
			FRLOGF (LOG_INFO, "script child not exited");
			kill (pid, SIGKILL);
		}
	} else {
		ret = RERR_CHILD;
	}
	if (havewritechild && !wexited) {
		if (waitpid(wpid,&rc,WNOHANG) < 0) {
			FRLOGF (LOG_WARNING, "error in waitpid on write child: %s",
								rerr_getstr3(RERR_SYSTEM));
			kill (wpid, SIGKILL);
		}
		if (!WIFEXITED(rc)) {
			FRLOGF (LOG_ERR, "write child not exited");
			kill (wpid, SIGKILL);
			/* ret = (RERR_ISOK (ret))?RERR_CHILD:ret; */
		} else if (NEGWEXITSTATUS (rc) != 0) {
			FRLOGF (LOG_ERR, "write child exit status %d", NEGWEXITSTATUS(rc));
			/* ret = (RERR_ISOK (ret)) ? ret : NEGWEXITSTATUS(rc); */
		}
	}

	/* wait for all childs (and zombies) */
	waitpid (-1, NULL, WNOHANG);
	/* close pipes and files */
	if (pofd > 2) close (pofd);
	if (pefd > 2) close (pefd);
	if (multiline) unlink (tfile);

	if (out && RERR_ISOK (ret)) {
		if (!obuf) obuf = strdup ("");
		if (!obuf) return RERR_NOMEM;
		*out = obuf;
	} else if (obuf) {
		free (obuf);
	}
	if (outlen) {
		*outlen = wlen;
	}
	/* to be sure */
	waitpid (-1, NULL, WNOHANG);
	waitpid (-1, NULL, WNOHANG);

	return ret;
}


static
int
parse_args (script, ap, oscript, fplist, numfp, flags)
	const char		*script;
	va_list			ap;
	char				**oscript;
	struct fd_pass	**fplist;
	int				*numfp;
	int				flags;
{
	int				num, i, j, k;
	char				*s, *s2, *out, *out2;
	struct fd_pass	*fpl;
	int				pfd[2];
	int				nofmt;

	if (!script || !oscript || !fplist || !numfp) return RERR_PARAM;
	nofmt = flags & IOPIPE_F_ONLY_Y;
	for (s=(char*)script, num=0; *s; s++) {
		if (*s == '%') {
			s++;
			if (*s == 0) break;
			if (*s == 'y') num++;
		}
	}
	out = malloc (strlen (script) + 8*num + 2);
	if (!out) return RERR_NOMEM;
	fpl = malloc ((num?num:1)*sizeof (struct fd_pass));
	if (!fpl) {
		free (out);
		return RERR_NOMEM;
	}
	for (s=(char*)script, s2=out, i=0; *s; s++) {
		if (*s == '%') {
			s++;
			switch (*s) {
			case 0:
				*s2 = '%';
				s2++;
				goto endloop;
			case '%':
				*s2 = '%';
				s2++;
				if (!nofmt) {
					*s2 = '%';
					s2++;
				}
				break;
			case 'y':
				fpl[i].pass = va_arg (ap, char*);
				if (pipe (pfd) == -1) {
					for (k=0; k<i; k++) {
						close (fpl[k].fdr);
						close (fpl[k].fdw);
					}
					free (out);
					free (fpl);
					FRLOGF (LOG_ERR, "error creating pipe: %s",
								rerr_getstr3(RERR_SYSTEM));
					return RERR_SYSTEM;
				}
				fpl[i].fdr = pfd[0];
				fpl[i].fdw = pfd[1];
				i++;
				sprintf (s2, "%d%n", pfd[0], &j);
				s2+=j;
				break;
			default:
				*(s2++) = '%';
				*(s2++) = *s;
				break;
			}
		} else {
			*(s2++) = *s;
		}
	}
endloop:
	*s2 = 0;
	if (!nofmt) {
		out2 = vasprtf (out, ap);
		free (out);
		out = out2;
		out2 = NULL;
		if (!out) {
			for (i=0; i<num; i++) {
				close (fpl[i].fdr);
				close (fpl[i].fdw);
			}
			free (fpl);
			return RERR_NOMEM;
		}
	}
	*oscript = out;
	*fplist = fpl;
	*numfp = num;
	return RERR_OK;
}






static
int
setNonblocking (fd)
	int	fd;
{
	int	flags;

	/* If they have O_NONBLOCK, use the Posix way to do it */
#if defined(O_NONBLOCK)
	/* Fixme: O_NONBLOCK is defined but broken on SunOS 4.1.x and AIX 3.2.5. */
	if (-1 == (flags = fcntl(fd, F_GETFL, 0)))
		flags = 0;
	return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
#else
	/* Otherwise, use the old way of doing it */
	flags = 1;
	return ioctl(fd, FIOBIO, &flags);
#endif
}



int
iopipe_parsecmd (argl, cmd)
	const char	***argl;
	char			*cmd;
{
	char	*s, *s2;
	int	numarg;

	s = top_skipwhite (cmd);
	*argl = NULL;
	numarg=0;
	while ((s2 = getendoftoken(s))) {
#if 0	/* done in getendoftoken */
		if ((*s=='"' || *s=='\'') && s2-s >= 2) {
			s++; *(s2-1)=0;
		}
#endif
		if (*s2 != 0) {
			*s2 = 0;
			s2++;
		}
		numarg++;
		*argl = realloc (*argl, (numarg+1) * sizeof (char*));
		if (!*argl) return RERR_NOMEM;
		(*argl)[numarg-1] = s;
		(*argl)[numarg] = NULL;
		s = top_skipwhite (s2);
		if (!*s) break;
	}
	if (!*argl || numarg==0) {
		FRLOGF (LOG_ERR, "empty command specified\n");
		return RERR_PARAM;
	}
	return RERR_OK;
}

static
char*
getendoftoken (s)
	char	*s;
{
	char	*s2;
	int	isdbl=0, issgl=0;

	s2 = s;
	for (; *s; s++) {
		if (*s == '\\' && *(s+1)) {
			*(s2++)=*++s;
		} else if (!issgl && *s == '"') {
			isdbl=!isdbl;
		} else if (*s == '\'') {
			issgl=!issgl;
		} else if (!issgl && !isdbl && iswhite (*s)) {
			break;
		} else {
			*(s2++)=*s;
		}
	}
	for (s--; s>=s2; s--) *s=' ';
	s++;
	return s;
}


static
int
checkforchild (prog, pid, flags)
	char	*prog;
	pid_t	pid;
	int	flags;
{
	int	ret, mayexit = 0, i;
	char	prog2[128];

	if (!prog) return RERR_PARAM;
	for (i=0; i<50; i++) {
		if ((waitpid (pid, &ret, WNOHANG) >= 0) && WIFEXITED (ret)) {
			ret = NEGWEXITSTATUS (ret);
			return ret;
		}
		if (mayexit) return RERR_SYSTEM;
		ret = pop_getcmdname (prog2, sizeof(prog2), pid);
		if (ret == RERR_NOT_FOUND) {
			mayexit=1;
			continue;
		}
		if (!RERR_ISOK (ret)) return ret;
		if (strcasecmp (prog, prog2) != 0) return RERR_OK;
		tmo_msleep (10);
	}
	return RERR_OK;
}


static
int
read_config ()
{
	cf_begin_read ();
	debug_output = cf_isyes (cf_getarr2 ("iopipe_log_output", 
									fr_getprog(), "no"));
	cf_end_read_cb (&read_config);
	config_read = 1;
	return 0;
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
