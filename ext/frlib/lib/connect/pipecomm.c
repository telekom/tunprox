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
#include <sys/types.h>
#include <string.h>
#include <strings.h>
#include <sys/wait.h>
#include <signal.h>




#include <fr/connect/pipecomm.h>
#include <fr/connect/readln.h>
#include <fr/base/errors.h>
#include <fr/base/errors.h>
#include <fr/base/slog.h>
#include <fr/base/config.h>
#include <fr/base/iopipe.h>
#include <fr/base/textop.h>
#include <fr/base/tmo.h>
#include <fr/base/frinit.h>



pid_t				pcon_childpid = -1;
int 				pcon_rdfd = -1;
int 				pcon_wrfd = -1;

static const char	*child_prog = NULL;
static int			child_rdwr = 0;

static
void
broken_pipe (sig)
	int	sig;
{
	if (sig != SIGPIPE) return;
	pcon_rdfd=-1;
	pcon_childpid = -1;
	FRLOGF (LOG_NOTICE, "child terminated\n");
}

int
pcon_openpipe (prog, rdwr)
	const char	*prog;
	int			rdwr;
{
	int			fds[2];
	int			wr_fd=-1, prog_rd=-1, prog_wr=-1, loc_rd_fd=-1;
	const char	**argl;
	pid_t			pid, pid2;
	int			status, ret;
	char			_buf[128], *progbuf;

	if (!prog || !*prog) return RERR_PARAM;
	prog = cf_getarr2 ("prog", prog, prog);
	if (rdwr & PCON_RDWR_WRITE) {
		if (pipe (fds) < 0) {
			FRLOGF (LOG_ERR, "error opening pipe for writing, write to stdout "
									"instead: %s", rerr_getstr3(RERR_SYSTEM));
			return RERR_SYSTEM;
		}
		wr_fd = fds[1];
		prog_rd = fds[0];
	}
	if (rdwr & PCON_RDWR_READ) {
		if (pipe (fds) < 0) {
			FRLOGF (LOG_ERR, "error opening pipe for reading: %s",
									rerr_getstr3(RERR_SYSTEM));
			if (wr_fd>0) close (wr_fd);
			if (prog_rd>0) close (prog_rd);
			return RERR_SYSTEM;
		}
		loc_rd_fd = fds[0];
		prog_wr = fds[1];
	}
	pid = fork();
	if (pid < 0) {
		FRLOGF (LOG_ERR, "error forking child: %s", rerr_getstr3(RERR_SYSTEM));
		if (loc_rd_fd>0) close (loc_rd_fd);
		if (wr_fd>0) close (wr_fd);
		if (prog_rd>0) close (prog_rd);
		if (prog_wr>0) close (prog_wr);
		return RERR_SYSTEM;
	}
	if (pid==0) {
		/* we are the child */
		/* ignore SIGINT - signals */
		signal (SIGINT, SIG_IGN);
		if (loc_rd_fd>0) close (loc_rd_fd);
		if (wr_fd>0) close (wr_fd);
		if (prog_rd>0) {
			if (dup2 (prog_rd, 0) < 0) {
				FRLOGF (LOG_ERR, "error in dup2 => read from stdin: %s",
								rerr_getstr3(RERR_SYSTEM));
			}
			close (prog_rd);
		}
		if (prog_wr>0) {
			if (dup2 (prog_wr, 1) < 0) {
				FRLOGF (LOG_ERR, "error in dup2 => write to stdout: %s", 
								rerr_getstr3(RERR_SYSTEM));
			}
			close (prog_wr);
		}
		progbuf = top_strcpdup (_buf, sizeof (_buf), prog);
		if (!progbuf) exit (RERR_NOMEM);
		ret = iopipe_parsecmd (&argl, progbuf);
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_ERR, "error in iopipe_parsecmd(): %s", 
								rerr_getstr3(ret));
			exit (ret);
		}
		execvp (argl[0], (char**)argl);
		/* if we reached here - an error occurred */
		FRLOGF (LOG_ERR, "error executing >>%s<<: %s", argl[0],
								rerr_getstr3 (ret));
		exit (-1);
		return -1;	/* to make compiler happy */
	}

	/* ok - we are the father */
	/* close not used filedescriptors */
	if (prog_rd>0) close (prog_rd);
	if (prog_wr>0) close (prog_wr);
	/* wait 100 ms and look wether the child is still living */
	tmo_msleep (100);
	pid2 = waitpid (pid, &status, WNOHANG);
	if (pid2 < 0) {
		FRLOGF (LOG_WARN, "error in waitpid: %s", rerr_getstr3(RERR_SYSTEM));
	} else if (pid2 > 0) {
		ret = NEGWEXITSTATUS (status);
		FRLOGF (LOG_ERR, "child exited with status: %d", ret);
		if (loc_rd_fd>0) { close (loc_rd_fd); loc_rd_fd=-1; }
		if (wr_fd > 0) close (wr_fd);
		return ret;
	}

	/* ok - we are done */
	if (!(rdwr & PCON_RDWR_NOSAVE)) {
		if (wr_fd>0) pcon_wrfd = wr_fd;
		pcon_childpid = pid;
		child_prog = prog;
		child_rdwr = rdwr;
		pcon_rdfd = loc_rd_fd;
		signal (SIGPIPE, broken_pipe);
	}
	/* maybe the prog has sent us some information at startup */
	if (rdwr & PCON_RDWR_TILEOT) {
		pcon_readtileot (1);
	} else if (!(rdwr & PCON_RDWR_NOEAT)) {
		pcon_eatinput (0);
	}
	return RERR_OK;
}


int
pcon_killchild ()
{
	/* myprtf (PRT_OUT, "q\n"); */
	if (pcon_childpid > 0) {
		tmo_msleep (50);
		kill (pcon_childpid, SIGTERM);
		tmo_msleep (50);
		kill (pcon_childpid, SIGKILL);
		pcon_childpid = -1;
		pcon_rdfd = -1;
		pcon_wrfd = 1;	/* write to stdout */
	}
	return RERR_OK;
}

int
pcon_reconnect ()
{
	int	ret;

	if (pcon_childpid <= 0) {
		return RERR_DISABLED;
	}
	pcon_killchild();
	ret = pcon_openpipe (child_prog, child_rdwr);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error starting child: %s", rerr_getstr3 (ret));
		return ret;
	}
#if 0
	ret = scstate_check(NULL);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_WARN, "error getting status of smartcard: %s",
								rerr_getstr3(ret));
	}
#endif
	return RERR_OK;
}



#if 0	/* no general sense */
#define READRES_NONE		0
#define READRES_INCOMPLETE	1	/* when set, stop at 92xx and 91xx as well */
#define READRES_FIRSTRESULT	2

int
read_result (result, flags)
	int	flags, *result;
{
	int	ret, res=0;
	char	*line=NULL, *s, *str;
	int	have_result=0;

	if (pcon_rdfd < 0) return RERR_DISABLED;
	while (1) {
		if (line) free (line);
		ret = fdcom_readln2 (pcon_rdfd, &line, FDCOM_LOGLN|FDCO_STRIPALLSPACE);
		if (!RERR_ISOK(ret)) return ret;
		if (!strncasecmp (line, "da_editor", 9)) {
			s = top_skipwhiteplus (line+9, "():.-;, ");
			if (s && !strncasecmp (s, "in", 2)) {
				res=-2;
				break;
			}
		}
		if ((flags & READRES_FIRSTRESULT) && !have_result && \
					!strncmp (line, "SG<-DA:", 7) && strlen (line) == 11) {
			str=line+7;
			have_result=1;
		} else if (!strncasecmp (line, "dastatus", 8)) {
			if (have_result) break;
			str = index (line, '<');
			if (!str) continue;
			str++;
			s = index (str, '>');
			if (s) *s=0;
		} else {
			continue;
		}
		for (res=0, s=str; *s; s++) {
			res*=16;
			if (*s>='0' && *s<='9') {
				res+=*s-'0';
			} else if (*s>='A' && *s <= 'F') {
				res+=*s-'A'+10;
			} else if (*s>='a' && *s <= 'f') {
				res+=*s-'a'+10;
			} else {
				break;
			}
		}
		if (!have_result) break;
#if 0
		ret = res>>8;
		if (ret == 0x90 || ret == 0x9e || ret == 0x6e) break;
		if (flags & READRES_INCOMPLETE || (flags & READRES_FIRSTRESULT)) {
			if (ret == 0x91 || ret == 0x92) break;
		}
		FRLOGF (LOG_WARN, "received status: %04x\n", res);
		break;
#endif
	}
	if (line) free (line);
	line=NULL;
	if (result) *result=res;
	return RERR_OK;
}
#endif 


int
pcon_readtileot (outfd)
	int	outfd;
{
	char	*line;
	int	ret;

	if (pcon_rdfd < 0) return RERR_DISABLED;
	while (1) {
		line=NULL;
		ret = fdcom_readln (pcon_rdfd, &line, FDCOM_EOT);
		if (!RERR_ISOK(ret) && ret != RERR_EOT) {
			FRLOGF (LOG_ERR, "error reading from pipe: %s", rerr_getstr3 (ret));
			return ret;
		}
		if (line) {
			write (outfd, line, strlen (line));
			free (line);
		}
		if (ret == RERR_EOT) break;
	}
	return RERR_OK;
}

char *
pcon_gettileot ()
{
	char	*line;
	int	ret;
	char	*obuf=NULL;
	int	len, olen=0;

	if (pcon_rdfd < 0) return NULL;
	while (1) {
		line=NULL;
		ret = fdcom_readln (pcon_rdfd, &line, FDCOM_EOT);
		if (!RERR_ISOK(ret) && ret != RERR_EOT) {
			FRLOGF (LOG_ERR, "error reading from pipe: %s", rerr_getstr3 (ret));
			return NULL;
		}
		if (line) {
			len = strlen (line);
			obuf = realloc (obuf, olen+len+1);
			if (!obuf) {
				olen=0;
				continue;
			}
			strcpy (obuf+olen, line);
			olen+=len;
			free (line);
		}
		if (ret == RERR_EOT) break;
	}
	return obuf;
}



int
pcon_eatinput (flags)
	int	flags;
{
	int	ret;
	char	*line;

	if (pcon_rdfd<0) return RERR_OK;
	while (1) {
		line=NULL;
		ret = fdcom_readln (pcon_rdfd, &line, FDCOM_NOWAIT);
		if (ret == RERR_NODATA) break;
		if (!RERR_ISOK(ret)) return ret;
		if (line) {
#if 0
			if (flags & PCON_EAT_NOLOG) {
				fdprtf (1, "%s", line);
			} else {
				myprtf (PRT_PROTIN, "< %s", line);
			}
#endif
			free (line);
		}
	}
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
