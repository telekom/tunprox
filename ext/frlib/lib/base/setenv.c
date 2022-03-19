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
#include <errno.h>
#ifdef SunOS
extern int errno;
#endif
#include <ctype.h>
#include <sys/types.h>
#include <pwd.h>




#include "config.h"
#include "slog.h"
#include "textop.h"
#include "fileop.h"
#include "errors.h"
#include "setenv.h"
#include "frinit.h"



static const char	*setenv_file = NULL;
static const char *user = NULL;
static const char *pwd = NULL;
static char *prog_to_use = NULL;
static int config_read = 0;

static int read_config();
static int read_config_for (const char *);
static int get_var (char *, char **, char **);


#ifdef SunOS
int
setenv (var, val, overwrite)
	const char	*var, *val;
	int			overwrite;
{
	char	* str;

	if (!var || !val || !*var) return -1;
	if (!overwrite && getenv (var)) return 0;
	str = malloc (strlen (var)+strlen (val) + 2);
	if (!str) return -1;
	sprintf (str, "%s=%s", var, val);
	return putenv (str);
}
int
unsetenv (var)
	const char	*var;
{
	return setenv (var, "", 1);
}
#endif



int
set_environment ()
{
	return set_environment_for (NULL);
}

int
set_environment_for (prog)
	const char	*prog;
{
	FILE	*f;
	char	*buf;
	char	*ptr, *line;
	char	*var, *val;

	read_config_for (prog);
	if (!setenv_file || !*setenv_file) return RERR_OK;
	f = fopen (setenv_file, "r");
	if (!f) {
		FRLOGF (LOG_ERR, "cannot open setenv_file >>%s<<: %s", setenv_file,
								rerr_getstr3(RERR_SYSTEM));
		return RERR_SYSTEM;
	}
	buf = fop_read_file (f);
	fclose (f);
	if (!buf) {
		FRLOGF (LOG_ERR, "error reading file >>%s<<: %s", setenv_file,
								rerr_getstr3(RERR_SYSTEM));
		return RERR_SYSTEM;
	}
	ptr = buf;
	while ((line = top_getline (&ptr, 0))) {
		if (!get_var (line, &var, &val)) continue;
		if (!*val) {
			unsetenv (var);
		} else {
			setenv (var, val, 1);
		}
	}
	free (buf);
	return RERR_OK;
}


int
set_user ()
{
	return set_user_for (NULL);
}

int
set_user_for (prog)
	const char	*prog;
{
	int				uid;
	struct passwd	*spwd;
	const char		*s;

	read_config_for (prog);
	if (!user || !*user) return RERR_OK;
	for (uid=0,s=user; *s; s++) {
		if (!isdigit (*s)) break;
		uid *= 10;
		uid += *s - '0';
	}
	errno = 0;
	if (!*s) {
		spwd = getpwuid ((uid_t)uid);
	} else {
		spwd = getpwnam (user);
	}	
	if (!spwd) {
		FRLOGF (LOG_ERR, "cannot set user to >>%s<<: %s", user,
								rerr_getstr3(RERR_SYSTEM));
		return RERR_SYSTEM;
	}
	setgid (spwd->pw_gid);
	setuid (spwd->pw_uid);
	setenv ("USER", spwd->pw_name, 1);
	setenv ("HOME", spwd->pw_dir, 1);
	if (getuid() != spwd->pw_uid) {
		FRLOGF (LOG_ERR, "error setting user to >>%s<<: %s", user,
								rerr_getstr3(RERR_SYSTEM));
		return RERR_SYSTEM;
	}
	return RERR_OK;
}


int
set_pwd ()
{
	return set_pwd_for (NULL);
}

int
set_pwd_for (prog)
	const char	*prog;
{
	const char	*mypwd = pwd;

	read_config_for (prog);
	if (!pwd || !*pwd) return RERR_OK;
	if (!strcasecmp (pwd, "%home")) {
		mypwd = getenv ("HOME");
		if (!mypwd) return RERR_OK;
	}
	if (chdir (mypwd) < 0) {
		FRLOGF (LOG_ERR, "error changing working directory to >>%s<<: %s",
								mypwd, rerr_getstr3(RERR_SYSTEM));
		return RERR_SYSTEM;
	}
	return RERR_OK;
}


int
setup_env ()
{
	return setup_env_for (NULL);
}


int
setup_env_for (prog)
	const char	*prog;
{
	int	ret, ret2 = RERR_OK;

	read_config_for (prog);
	ret = set_environment_for (prog);
	if (!RERR_ISOK (ret)) {
		FRLOGF (LOG_WARN, "error setting environment variables for >>%s<< - "
								"setup is incomplete: %s", prog?prog:"<NULL>",
								rerr_getstr3 (ret));
		if (RERR_ISOK (ret2)) ret2 = ret;
	}
	ret = set_user_for (prog);
	if (!RERR_ISOK (ret)) {
		FRLOGF (LOG_WARN, "error setting user for >>%s<< - setup is "
								"incomplete: %s", prog?prog:"<NULL>",
								rerr_getstr3 (ret));
		if (RERR_ISOK (ret2)) ret2 = ret;
	}
	ret = set_pwd_for (prog);
	if (!RERR_ISOK (ret)) {
		FRLOGF (LOG_WARN, "error setting working directory for >>%s<< - "
								"setup is incomplete: %s", prog?prog:"<NULL>",
								rerr_getstr3 (ret));
		if (RERR_ISOK (ret2)) ret2 = ret;
	}
	return ret2;
}








/*
 * static functions
 */


static
int
get_var (line, var, val)
	char	*line, **var, **val;
{
	char	*s, *s2, *value;
	if (!line || !var || !val) return 0;
	line = top_skipwhite (line);
	if (!*line || *line == '#') return 0;
	if (!strncasecmp (line, "export", 6) && iswhite (line[6])) {
		line = top_skipwhite (line+6);
	}
	s = index (line, '=');
	if (!s) return 0;
	for (s2=s-1; s2>=line && iswhite (*s2); s2--) {}; s2++;
	*s2 = 0;
	s = top_skipwhite (s+1);
	if (*s == '"') {
		value = s+1;
		s = index (value, '"');
		if (s) *s = 0;
	} else {
		value = s;
		for (; *s && !iswhite (*s); s++);
		*s =0;
	}
	if (!line) return 0;
	*var = line;
	*val = value;
	return 1;
}



static
int
read_config_for (prog)
	const char	*prog;
{
	char			*s;
	static int	first = 1;

	cf_mayread ();
	if (first) {
		first = 0;
		read_config ();
	}
	if (!prog) {
		prog = fr_getprog ();
	}
	if (prog) {
		s = rindex (prog, '/');
		if (s) prog = s+1;
	}
	if (!*prog) prog=NULL;
	if (config_read) {
		if (!prog && !prog_to_use) return 1;
		if (prog && prog_to_use && !strcmp (prog, prog_to_use)) return 1;
	}
	cf_begin_read ();
	if (prog_to_use) free (prog_to_use);
	prog_to_use = prog ? strdup (prog) : NULL;
	user = cf_getarr ("user", prog);
	setenv_file = cf_getarr ("setenv_file", prog);
	pwd = cf_getarr ("pwd", prog);
	cf_end_read ();
	return 1;
}



static
int
read_config ()
{
	config_read = 0;
	read_config_for (prog_to_use);
	cf_begin_read ();
	/* the variables are read inside read_config_for */
	config_read = 1;
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
