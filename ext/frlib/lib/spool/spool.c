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
#include <stdarg.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#ifdef SunOS
extern int errno;
#endif
#define __USE_LARGEFILE64
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <time.h>



#include "spool.h"
#include <fr/base/errors.h>
#include <fr/base/textop.h>
#include <fr/base/fileop.h>
#include <fr/base/slog.h>
#include <fr/base/config.h>
#include <fr/base/strcase.h>
#include <fr/base/tmo.h>
#include <fr/cal/cjg.h>

//#define USE_LIGHT
//#define NFS_WORKAROUND


//#define SUBDIR_NONE		0
#define SUBDIR_NAMEONLY	1
#define SUBDIR_LOCK		2
#define SUBDIR_UNLOCK	4

//#define FLAG_MKFILE_NONE	0
#define FLAG_MKFILE_NOSUB	1

//#define FLAG_RUNNUM_NONE	0
#define FLAG_RUNNUM_NOINC	1

//#define FLAG_WRSPOOLENTRY_NONE		0
#define FLAG_WRSPOOLENTRY_MAYERROR	1


#define STATEFLAG(state) (	(state)==SPOOL_STATE_UNKNOWN?SPOOL_FLAG_PRT_UNKNOWN:\
									(state)==SPOOL_STATE_ERROR?SPOOL_FLAG_PRT_ERROR:\
									(state)==SPOOL_STATE_WAIT?SPOOL_FLAG_PRT_WAIT:\
									(state)==SPOOL_STATE_ELAB?SPOOL_FLAG_PRT_ELAB:0)
#define PRT_STATE(state,flag)	(((flag) & (STATEFLAG(state))) ? 1 : 0)

struct dirdatelist {
	char						*dir;
	time_t					date;
	struct dirdatelist	*left, *right, *father;
};
#define DIRDATELIST_NULL	((struct dirdatelist){NULL, 0, NULL, NULL, NULL})


static int						num_files_per_subdir = 1000;
static int 						config_read=0;
static int						spool_to_subdir=1;
static int						use_light_spooler=1;
static int						cache_maxtime=600;
static tmo_t					max_elabtime=0;
static int						nfs_workaround=0;
static int						max_wait_for_lock = 60;
static const char				*default_spool;
static const char				*base_spooldir;
static struct dirdatelist	*dirdatelist=NULL;
int 								reinsert_ontimeout = 1;
char								*spool_what = NULL;
tmo_t								lockfile_timeout = 10000000LL;

//#define CHECK_ABSPATH(path)	((path)&&((*(path)=='/')||((*(path)=='.')&&((path)[1]=='/'))))




static int dolock (const char*, tmo_t);
static int dounlock(const char*);
static int sortlist2 (struct spoollist*, int);
static int sortlist (struct spoollist*, int);
static int del_notwanted (struct spoollist*, int);
static int del_spoollistentry (struct spoollist*, int);
static int mkfilename (char**, const char*, const char*, const char*);
static int mkfilename2 (char**, const char*, const char*, const char*, int);
static int mkerrorfilename (char**, const char*, const char*);
static int mkfilename_check (char**, const char*, const char*);
static int read_df (struct spool_entry*, const char*, char**, int*);
static int write_df (struct spool_entry*, const char*, char*, int, const char*, int, int);
static int write_spoolentry (struct spool_entry*, const char*);
static int write_spoolentry2 (struct spool_entry*, const char*, int);
static int read_spoolentry (struct spool_entry*, const char*, const char*);
static int readspool (const char*, struct spoollist*);
static int maymkdir (const char*);

static int get_runnum (const char*, int);
static int readspool_all (const char*, struct spoollist*);
static int readspool_first (const char*, struct spoollist*);
static int dospool_chstate (const char*, const char*, int, int, const char*);

static int getspooldir (char**, const char*);
static int getspooldir2 (char**, const char*);
static int dounspool (const char*, char**, int*, char**, char**, char**);
static int getspoolfile2 (struct spool_entry*, const char*, int);
static int getspoolfile (struct spool_entry*, const char*);
static int dospool (const char*, char*, int, const char*, int, int, const char*);
static int spool_in2 (const char*, char*, int, const char*, int, int, const char*);
static int lock_single_file (const char*, const char*, tmo_t);
static int unlock_single_file (const char*, const char*);
static int dolock_file (const char*, tmo_t);
static int spool_list_light (const char*, struct spoollist*);
static int readspool_light (const char*, struct spoollist*);
static int sortlist_light (struct spoollist*);
static int get_entrystate (int*, const char*, const char*);
static int check_cfile (const char*, const char*);

/* subdir should be at least 64 bytes long */
static int getsubdir (char*, const char*, unsigned, unsigned);
static int read_config();
static int getspooldirlist (struct spoollist*, const char*);
static int cache_getspoollist (struct spoollist**, const char*);
static int may_deletedir (const char*, const char*);
static int dirdatelist_freelist (struct dirdatelist*);
static struct dirdatelist* dirdatelist_search_rec (struct dirdatelist*, const char*);
static int dirdatelist_insert_rec (struct dirdatelist*, struct dirdatelist*);
static int dirdatelist_remove (const char*);
static int dirdatelist_search (time_t*, const char*);
static int dirdatelist_insert (const char*, time_t);
static int write_subdirdate (const char*, const char*);
static int get_subdirdate (time_t*, const char*, const char*);
static int doreadspool_all (const char*, struct spoollist*);
static int checkelabtime (const char*, struct spool_entry*);
static int move_cfdf (const char*, const char*, const char*);
static int del_emptyentries (struct spoollist*);
static int num_to_cf (char**, const char*, int);
static int getfilesize (int*, const char*);
static int set_spoolwhat (const char*);



/* spool_in:
 *		inserisce buffer (buf) nello spool
 *
 *		spool:	name of spool
 *		buf:		buffer to insert
 *		buflen:	length of buffer
 *		options:	options to pass to final programme
 *
 *		ritorna un codice d'errore
 */

int
spool_in (spool, buf, buflen, options)
	const char	*spool, *options;
	char			*buf;
	int	buflen;
{
	return spool_in2 (spool, buf, buflen, NULL, 0, -1, options);
}

int
spool_infile (spool, filename, dolink, options)
	const char	*spool, *options;
	const char	*filename;
	int			dolink;
{
	return spool_in2 (spool, NULL, 0, filename, dolink, -1, options);
}

int
spool_infd (spool, fd, options)
	const char	*spool, *options;
	int			fd;
{
	return spool_in2 (spool, NULL, 0, NULL, 0, fd, options);
}




int
spool_outfile (spool, cf_name, df_name, options)
	const char	*spool;
	char			**cf_name, **df_name, **options;
{
	return spool_out (spool, NULL, NULL, cf_name, df_name, options);
}

/* unspool:
 *		read first file from spool and change state into IN_ELAB
 *
 *		spool:	name of spool
 *		buf:		output (will be malloc'ed).
 *		buflen:	length of output
 *		cf_name:	name of cf file (return value)
 *		df_name:	name of df file (return value)
 *		options:	options to pass to called proc (return value)
 *
 *		return an error code
 */
int
spool_out (spool, buf, buflen, cf_name, df_name, options)
	const char	*spool;
	char			**buf, **cf_name, **df_name, **options;
	int			*buflen;
{
	char	*spooldir;
	int	ret;

	if ((!buf && !df_name) || !cf_name) return RERR_PARAM;
	ret = getspooldir (&spooldir, spool);
	if (!RERR_ISOK(ret)) return ret;
	ret = dounspool (spooldir, buf, buflen, cf_name, df_name, options);
	free (spooldir);
	return ret;
}


int
spool_outdel (spool, buf, buflen)
	const char	*spool;
	char			**buf;
	int			*buflen;
{
	char	*cf_name;
	int	ret;

	if (!buf) return RERR_PARAM;
	ret = spool_out (spool, buf, buflen, &cf_name, NULL, NULL);
	if (!RERR_ISOK(ret)) return ret;
	spool_delentry (spool, cf_name);
	free (cf_name);
	return RERR_OK;
}

int
spool_get_runnum (spooldir)
	const char	*spooldir;
{
	return get_runnum (spooldir, FLAG_RUNNUM_NOINC);
}

int
spool_lockfile_create (filename, timeout)
	const char	*filename;
	tmo_t			timeout;
{
	return dolock_file (filename, timeout);
}


/* spool_list:
 *		legge tutto lo spool (apre tutti i file cf)
 *
 *		spool:	nome dello spool
 *		list|:	puntatore del risultato
 *		flag:		vedi del_notwanted e sortlist
 *
 *		ritorna un codice d'errore
 */
int
spool_list (spool, list, flag)
	const char			*spool;
	struct spoollist	*list;
	int					flag;
{
	char	*spooldir;
	int	ret, i;

	if (!list) return RERR_PARAM;
	ret = getspooldir (&spooldir, spool);
	if (!RERR_ISOK(ret)) return ret;
	*list = SPOOL_LIST_NULL;
	if (spool_to_subdir) {
		ret = readspool_all (spooldir, list);
		if (!RERR_ISOK(ret)) {
			free (spooldir);
			return ret;
		}
		for (i=0; i<list->listnum; i++) {
			read_spoolentry (&(list->list[i]), spooldir, list->list[i].cf_name);
		}
	} else {
		ret = readspool (spooldir, list);
	}
	if (!RERR_ISOK(ret)) {
		free (spooldir);
		return ret;
	}
	list->spooldir = spooldir;
	list->spoolname = spool?strdup (spool):NULL;
	if (spool&&!list->spoolname) {
		hfree_spoollist (list);
		return RERR_NOMEM;
	}
	ret = del_notwanted (list, flag);
	if (!RERR_ISOK(ret)) {
		hfree_spoollist (list);
		return ret;
	}
	ret = sortlist (list, flag);
	if (!RERR_ISOK(ret)) {
		hfree_spoollist (list);
		return ret;
	}
	return RERR_OK;
}


/* spool_num2cf:
 *		ritorna il file cf correspondente al contatore
 *
 *		cf_name:	buffer per salvare il nome del file cf
 *		spool:	nome dello spool
 *		num:		contatore
 *
 *		ritorno un codice d'errore
 */
int
spool_num2cf (cf_name, spool, num)
	char			**cf_name;
	const char	*spool;
	int			num;
{
	char	*spooldir;
	int	ret;

	if (!cf_name) return RERR_PARAM;
	ret = getspooldir (&spooldir, spool);
	if (!RERR_ISOK(ret)) return ret;
	ret = num_to_cf (cf_name, spooldir, num);
	free (spooldir);
	return ret;
}

/* spool_read_df:
 *		reads the content of the df file
 *
 *		buf:		buffer (output)
 *		buflen:	length of buffer (output)
 *		spool:	name of spool
 *		cf_name:	name of cf_ file
 *
 *		returns an error code
 */
int
spool_read_df (buf, buflen, spool, cf_name)
	const char	*spool, *cf_name;
	char			**buf;
	int			*buflen;
{
	struct spool_entry	entry;
	char						*spooldir;
	int						ret;

	if (!buf || !buflen || !cf_name) return RERR_PARAM;
	ret = getspooldir (&spooldir, spool);
	if (!RERR_ISOK(ret)) return ret;
	entry = SPOOL_ENTRY_NULL;
	ret = read_spoolentry (&entry, spooldir, cf_name);
	if (!RERR_ISOK(ret)) {
		free (spooldir);
		return ret;
	}
	ret = read_df (&entry, spooldir, buf, buflen);
	free (spooldir);
	hfree_spoolentry (&entry);
	return ret;
}



/* hfree_spoollist
 *		destructor di struct spoollist
 *
 *		entry:	struttura da cancellare
 *
 *		ritorna 1 o 0 in caso di errore
 */
int
hfree_spoollist (list)
	struct spoollist	*list;
{
	if (!list) return 0;
	while (list->list && list->listnum) {
		if (del_spoollistentry (list, list->listnum-1) != RERR_OK) break;
	}
	if (list->list) free (list->list);
	if (list->spoolname) free (list->spoolname);
	if (list->spooldir) free (list->spooldir);
	*list = SPOOL_LIST_NULL;
	return 1;
}

/* hfree_spoolentry:
 *		destructor di struct spool_entry
 *
 *		entry:	struttura da cancellare
 *
 *		ritorna 1 o 0 in caso di errore
 */
int
hfree_spoolentry (entry)
	struct spool_entry	*entry;
{
	if (!entry) return 0;
	if (entry->cf_name) free (entry->cf_name);
	if (entry->df_name) free (entry->df_name);
	if (entry->options) free (entry->options);
	*entry = SPOOL_ENTRY_NULL;
	return 1;
}

/* spool_chstate:
 *		cambia lo stato nel cf_name
 *
 *		spool:		nome dello spool (non neccessariamente la cartella)
 *		cf_name:		nome del file cf nello spool
 *		newstate:	nuovo stato
 *		error:		codice d'errore (usato solo se newstate e` SPOOL_STATE_ERROR
 *
 *		ritorna un codice d'errore
 */
int
spool_chstate (spool, cf_name, newstate, error)
	const char	*spool, *cf_name;
	int			newstate, error;
{
	return spool_chstate2 (spool, cf_name, newstate, error, NULL);
}

/* spool_chstate2:
 *		cambia lo stato ed opzioni nel cf_name
 *
 *		spool:		nome dello spool (non neccessariamente la cartella)
 *		cf_name:		nome del file cf nello spool
 *		newstate:	nuovo stato
 *		error:		codice d'errore (usato solo se newstate e` SPOOL_STATE_ERROR
 *		options:		opzioni da settare, se NULL, non cambia
 *
 *		ritorna un codice d'errore
 */
int
spool_chstate2 (spool, cf_name, newstate, error, options)
	const char	*spool, *cf_name, *options;
	int			newstate, error;
{
	char	*spooldir;
	int	ret;

	if (!spool || !cf_name) return RERR_PARAM;
	if (newstate < 0 || newstate > SPOOL_STATE_MAX) return RERR_PARAM;
	ret = getspooldir (&spooldir, spool);
	if (!RERR_ISOK(ret)) return ret;
	ret = dospool_chstate (spooldir, cf_name, newstate, error, options);
	free (spooldir);
	return ret;
}



/* spool_delentry:
 *		cancella un file dello spool (cf e df)
 *
 *		spool:	nome dello spool
 *		cf_name:	nome del file cf
 *
 *		ritorna un codice d'errore
 */
int
spool_delentry (spool, cf_name)
	const char	*spool, *cf_name;
{
	char						*spooldir, *df_name, *cf_name2;
	struct spool_entry	entry;
	int						ret;

	if (!spool || !cf_name) return RERR_PARAM;
	ret = getspooldir (&spooldir, spool);
	if (!RERR_ISOK(ret)) return ret;

	FRLOGF (LOG_VERB, "deleting spool entry >>%s<<", cf_name);

	entry = SPOOL_ENTRY_NULL;
	ret = read_spoolentry (&entry, spooldir, cf_name);
	if (!RERR_ISOK(ret)) {
		free (spooldir);
		return ret;
	}
	ret = mkfilename_check (&df_name, spooldir, entry.df_name);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_WARN, "error getting df file name: %s", rerr_getstr3 (ret));
	} else if (df_name) {
		unlink (df_name);
		free (df_name);
	}
	ret = mkfilename_check (&cf_name2, spooldir, cf_name);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_WARN, "error getting cf file name: %s", rerr_getstr3 (ret));
	} else if (cf_name2) {
		unlink (cf_name2);
		free (cf_name2);
	}
	free (spooldir);
	hfree_spoolentry (&entry);
	FRLOGF (LOG_VERB, ">>%s<< deleted.", cf_name);
	return RERR_OK;
}


/* spool_print:
 *		stampa contenuto dello spool su stdout
 *
 *		spool:	nome dello spool
 *		flags:	vedi spool_list, piu`
 *					SPOOL_FLAG_SHOW_CTIME:	visualizza ctime, anzi che mtime
 *
 *		ritorna un codice d'errore
 */
int
spool_print (spool, flags)
	const char	*spool;
	int			flags;
{
	struct spoollist	list;
	int					ret, num, i, n, j, buflen;
	char					*buf, *buf2, *s, *line;
	char					*spooldir;
	char					stime[48];

	ret = spool_list (spool, &list, flags);
	if (!RERR_ISOK(ret)) {
		fprintf (stderr, "spool(): error %d\n", ret);
		return ret;
	}
	if (list.spoolname) {
		if (list.spooldir && !strcmp (list.spoolname, list.spooldir)) {
			printf ("list spool: %s%n\n", list.spoolname, &num);
		} else {
			printf ("list spool: %s (%s)%n\n", list.spoolname, list.spooldir, 
						&num);
		}
	} else {
		printf ("list defaultspool (%s)%n\n", list.spooldir?list.spooldir:
						"<NULL>", &num);
	}
	for (; num; num--) putchar ('=');
	putchar ('\n');
	if (!list.listnum) {
		printf ("  empty\n\n");
		return RERR_OK;
	}
	ret = getspooldir (&spooldir, spool);
#if 0
	if (!RERR_ISOK(ret)) {
		hfree_spoollist (&list);
		return ret;
	}
#endif
	printf ("     num    status  error               time    size   %n     %s%s\n", 
						&n, ((flags&SPOOL_FLAG_SHOW_DFFILE)?"df_file":"name   "),
						(!(flags&SPOOL_FLAG_SHOW_DFFILE)?"    options":""));
	printf ("-------------------------------------------------------------------");
	if (!(flags&SPOOL_FLAG_SHOW_DFFILE)) {
		printf ("--------------\n");
	} else {
		printf ("----\n");
	}
	for (i=0; i<list.listnum; i++) {
		printf ("%8d  ", list.list[i].num);
		switch (list.list[i].state) {
		case SPOOL_STATE_WAIT:
			printf ("    wait      -  ");
			break;
		case SPOOL_STATE_ELAB:
			printf ("in elab.      -  ");
			break;
		case SPOOL_STATE_ERROR:
			printf ("   error  %5d  ", list.list[i].error);
			break;
		default:
			printf (" unknown      -  ");
			break;
		}
		cjg_prttimestr (stime, sizeof (stime), ((flags & SPOOL_FLAG_SHOW_CTIME) ? 
							list.list[i].ctime : list.list[i].mtime), CJG_TSTR_T_D);
		printf ("%17s ", stime);
		printf ("%7d  ", list.list[i].size);
		if (flags & SPOOL_FLAG_SHOW_DFFILE) {
			ret = read_df (&(list.list[i]), spooldir, &buf, &buflen);
			if (!RERR_ISOK(ret)) {
				printf ("<error>\n");
				continue;
			}
			buf2 = buf;
			for (j=0; j<4; j++) {
				line = top_getline (&buf2, 0);
				if (!line || !*line) {
					if (j==0) printf ("\n");
					break;
				}
				printf ("%*s%s\n", ((j==0)?0:n), " ", line);
			}
			free (buf);
		} else {
			s=(s=rindex (list.list[i].cf_name, '/'))?s+1:list.list[i].cf_name;
			printf ("   %9s   ", s);
			if (list.list[i].options) {
				printf ("(%s)\n", list.list[i].options);
			} else {
				printf (" -\n");
			}
		}
	}
	putchar ('\n');
	free (spooldir);
	hfree_spoollist (&list);
	return RERR_OK;
}



/***************************************
 * static functions
 ***************************************/

static
int
spool_in2 (spool, buf, buflen, fname, dolink, fd, options)
	const char	*spool, *fname, *options;
	char			*buf;
	int			buflen, dolink, fd;
{
	char	*spooldir;
	int	ret;

	if (!buf && !fname && fd<0) return RERR_PARAM;
	ret = getspooldir (&spooldir, spool);
	if (!RERR_ISOK(ret)) return ret;
	if (buflen < 0) buflen = strlen (buf);
	ret = dospool (spooldir, buf, buflen, fname, dolink, fd, options);
	free (spooldir);
	return ret;
}

/* dospool:
 *		insert buffer or file into spool
 *
 *		spooldir:	spool directory
 *		buf:			buffer to insert
 *		buflen:		length of buffer
 *		fname:		filename to insert (alternative to buf)
 *		dolink:		link file into spool instead of copying it (flag)
 *		fd:			filedescriptor of file to insert (alt. to fname)
 *		options:		options to pass to elab. programme
 *
 *		returns an error code
 */
static
int
dospool (spooldir, buf, buflen, fname, dolink, fd, options)
	const char	*spooldir, *fname, *options;
	char			*buf;
	int			buflen, dolink, fd;
{
	tmo_t						now;
	int						ret, pid;
	struct spool_entry	entry;
	char						subdir[64];
	int						fsize;

	/* create spool files */
	entry = SPOOL_ENTRY_NULL;
	entry.num=get_runnum(spooldir, 0);
	if (entry.num<0) return entry.num;
	ret = getspoolfile (&entry, spooldir);
	if (!RERR_ISOK(ret)) return ret;

	/* create controll file */
	now = tmo_now ();
	entry.ctime = entry.mtime = now;
	pid = (int) getpid ();
	if (pid < 0) pid=0;
	entry.pid = pid;
	entry.state = SPOOL_STATE_WAIT;
	if (buf) {
		entry.size = buflen;
	} else if (fname) {
		ret = getfilesize (&fsize, fname);
		if (RERR_ISOK(ret)) {
			entry.size = fsize;
		} else {
			/* we report the negative error code here, and continue */
			FRLOGF (LOG_WARN, "cannot get size, use a negative error code as "
									"size");
			entry.size = ret;
		}
	} else {
		entry.size = 0;
	}
	entry.options = options?strdup(options):NULL;

	/* write spoolentry and data */
	ret = write_df (&entry, spooldir, buf, buflen, fname, dolink, fd);
	if (!RERR_ISOK(ret)) {
		unlock_single_file (spooldir, entry.cf_name);
		hfree_spoolentry (&entry);
		return ret;
	}
	if (!entry.size) {
		ret = getfilesize (&fsize, entry.df_name);
		if (RERR_ISOK(ret)) {
			entry.size = fsize;
		} else {
			/* we report the negative error code here, and continue */
			FRLOGF (LOG_WARN, "cannot get size, use a negative error code "
									"as size");
			entry.size = ret;
		}
	}
	ret = write_spoolentry (&entry, spooldir);
	if (!RERR_ISOK(ret)) {
		unlock_single_file (spooldir, entry.cf_name);
		hfree_spoolentry (&entry);
		return ret;
	}
	unlock_single_file (spooldir, entry.cf_name);
	if (spool_to_subdir) {
		ret = getsubdir (subdir, spooldir, entry.num, SUBDIR_NAMEONLY);
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_WARN, "error writing dir date: %s", rerr_getstr3 (ret));
		}
		ret = write_subdirdate (spooldir, subdir);
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_WARN, "error writing dir date: %s", rerr_getstr3 (ret));
		}
	}
	hfree_spoolentry (&entry);
	return RERR_OK;
}



static
int
getspoolfile (entry, spooldir)
	struct spool_entry	*entry;
	const char				*spooldir;
{
	return getspoolfile2 (entry, spooldir, 128);
}



/* getspoolfile2:
 *		creates the files cf and df, and the lockfile lf
 *
 *		entry:		structure with credientals of spool entry 
 *						(filled in by getspoolfile)
 *		spooldir:	spool directory
 *		num:			number of maximum tries
 *
 *		returns an error code
 */
static
int
getspoolfile2 (entry, spooldir, num)
	struct spool_entry	*entry;
	const char				*spooldir;
	int						num;
{
	char	*cfile, *dfile, *lfile, *s, subdir[64];
	int	cfd, dfd, lfd, ret;

	if (num < 0 || !spooldir || !entry) return RERR_PARAM;
	*subdir=0;
	lfile = malloc (strlen (spooldir) + 32);
	if (!lfile) return RERR_NOMEM;

	ret = getsubdir (subdir, spooldir, entry->num, SUBDIR_LOCK);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error creating >>%s<<: %s", subdir, rerr_getstr3(ret));
		free (lfile);
		return ret;
	}
	sprintf (lfile, "%s/%s/lf_%d_XXXXXX", spooldir, subdir, entry->num);
	lfd = mkstemp (lfile);
	ret = getsubdir (subdir, spooldir, entry->num, SUBDIR_UNLOCK);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error unlocking >>%s<<: %s", subdir,
									rerr_getstr3(ret));
		unlink (lfile);
		free (lfile);
		if (lfd>0) close (lfd);
		return ret;
	}
	if (lfd==-1) {
		FRLOGF (LOG_ERR, "error creating spool file: %s",
								rerr_getstr3(RERR_SYSTEM));
		free (lfile);
		return RERR_SYSTEM;
	}
	dfile = strdup (lfile);
	if (!dfile) {
		close (lfd);
		unlink (lfile);
		free (lfile);
		return RERR_NOMEM;
	}
	s=rindex (dfile, '/');
	if (!s) {
		FRLOGF (LOG_ERR, "error in filename");
		close (lfd);
		unlink (lfile);
		free (lfile);
		free (dfile);
		return RERR_INVALID_FILE;
	}
	s++;
	*s='d';

	dfd = open (dfile, O_RDWR|O_CREAT|O_EXCL|O_LARGEFILE, 0666);
	if (dfd == -1) {
		if (num <= 0) {
			FRLOGF (LOG_ERR, "error creating spoolfile: %s",
									rerr_getstr3(RERR_SYSTEM));
		}
		close (lfd);
		unlink (lfile);
		free (lfile);
		free (dfile);
		if (num > 0) {
			return getspoolfile2 (entry, spooldir, num-1);
		}
		return RERR_SYSTEM;
	}
	cfile = strdup (lfile);
	if (!cfile) {
		close (lfd);
		unlink (lfile);
		free (lfile);
		close (dfd);
		unlink (dfile);
		free (dfile);
		return RERR_NOMEM;
	}
	s=rindex (cfile, '/');
	if (!s) {
		FRLOGF (LOG_ERR, "error in filename >>%s<<", cfile);
		close (lfd);
		unlink (lfile);
		free (lfile);
		close (dfd);
		unlink (dfile);
		free (dfile);
		free (cfile);
		return RERR_INVALID_FILE;
	}
	s++;
	*s='c';
	cfd = open (cfile, O_RDWR|O_CREAT|O_EXCL, 0666);
	if (cfd == -1) {
		if (num <= 0) {
			FRLOGF (LOG_ERR, "getspoolfile(): error creating spoolfile: %s", 
									rerr_getstr3(RERR_SYSTEM));
		}
		close (lfd);
		unlink (lfile);
		free (lfile);
		close (dfd);
		unlink (dfile);
		free (dfile);
		free (cfile);
		if (num > 0) {
			return getspoolfile2 (entry, spooldir, num-1);
		}
		return RERR_SYSTEM;
	}
	close (lfd);
	close (dfd);
	close (cfd);
	entry->cf_name = cfile;
	entry->df_name = dfile;
	return RERR_OK;
}





/* get_runnum:
 *		return the actual run number (counter) and increment it
 *
 *		spooldir:	spool directory
 *		flag:			FLAG_RUNNUM_NOINC:	do not increment counter
 *
 *		returns the counter. on error returns a negative error code
 */
static
int
get_runnum (spooldir, flags)
	const char	*spooldir;
	int			flags;
{
	char	*filename=NULL;
	int	runnum=0, ret;
	FILE	*f;

	if (!spooldir) return RERR_PARAM;
	dolock(spooldir, lockfile_timeout);
	ret = mkfilename (&filename, spooldir, "cnt", NULL);
	if (!RERR_ISOK(ret)) {
		runnum=ret;
		goto finish;
	}
	f = fopen (filename, "r");
	if (f) {
		fscanf (f, "%d", &runnum);
		fclose (f);
	}
	if (!(flags & FLAG_RUNNUM_NOINC)) {
		runnum++;
		f = fopen (filename, "w+");
		if (!f) goto finish;
		if (runnum <= 0) runnum=1;
		fprintf (f, "%d", runnum);
		fclose (f);
	}
finish:
	if (filename) free (filename);
	dounlock(spooldir);
	return runnum;
}





/* dounspool:
 *		read the first file from spool and change state to IN_ELAB
 *
 *		spooldir:	spool directory
 *		buf:			pointer to output buffer (to be freed)
 *		buflen:		pointer to length of created output buffer
 *		cf_name:		name of cf file (returned - to be freed)
 *		df_name:		name of df file (returned - to be freed)
 *		options:		options to pass to elab. prog. (returned - to be freed)
 *
 *		returns an error code
 */
static
int
dounspool (spooldir, buf, buflen, cf_name, df_name, options)
	const char	*spooldir;
	char			**buf, **cf_name, **df_name, **options;
	int			*buflen;
{
	int					ret=RERR_OK,found=0;
	struct spoollist	*list;
	int					i;

	if (!spooldir || !*spooldir || (!buf && !df_name) || !cf_name)
		return RERR_PARAM;
	CF_MAY_READ;
	FRLOGF (LOG_VVERB, "getting spoollist");
	ret = cache_getspoollist (&list, spooldir);
	if (ret == RERR_NOT_FOUND) {
		return ret;
	} else if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error in cache_getspoollist(): %s",
								rerr_getstr3 (ret));
		return ret;
	}
	for (i=list->next_to_elab; i<list->listnum; i++) {
		*cf_name = list->list[i].cf_name;
		if (!*cf_name) continue;
		FRLOGF (LOG_VVERB, "create lockfile for >>%s<<", *cf_name);
		ret = lock_single_file (spooldir, *cf_name, lockfile_timeout);
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_NOTICE, "cannot create lockfile for >>%s<<: %s",
									*cf_name, rerr_getstr3(ret));
			continue;
		}
		FRLOGF (LOG_VVERB, "lockfile for >>%s<< created", *cf_name);
		if (use_light_spooler) {
			list->list[i].cf_name = NULL;
			ret = read_spoolentry (&(list->list[i]), spooldir, *cf_name);
			if (!RERR_ISOK(ret)) {
				FRLOGF (LOG_DEBUG, "cannot read entry for >>%s<<: %s", *cf_name,
											rerr_getstr3 (ret));
				list->list[i].cf_name = *cf_name;
				goto cont_loop;
			}
			free (*cf_name);
			*cf_name = list->list[i].cf_name;
			if (list->list[i].state == SPOOL_STATE_ELAB) {
				checkelabtime (spooldir, &(list->list[i]));
			}
			if (list->list[i].state != SPOOL_STATE_WAIT) goto cont_loop;
		} else {
			if (list->list[i].state == SPOOL_STATE_ELAB) {
				checkelabtime (spooldir, &(list->list[i]));
			}
			if (!check_cfile (spooldir, *cf_name)) goto cont_loop;
		}

		if (buf) {
			ret = read_df (&(list->list[i]), spooldir, buf, buflen);
			if (RERR_ISOK(ret)) {
				ret = spool_chstate (spooldir, *cf_name, SPOOL_STATE_ELAB, 0);
				if (!RERR_ISOK(ret)) {
					FRLOGF (LOG_WARN, "error changing state to \"in elab.\" for "
											"cf=>>%s<<: %s", *cf_name, rerr_getstr3(ret));
					free (*buf);
					goto cont_loop;
				}
				list->list[i].cf_name = NULL;
				list->next_to_elab = i+1;
				list->has_elab = 1;
				found=1;
			} else {
				FRLOGF (LOG_WARN, "error reading df file >>%s<<: %s",
								list->list[i].df_name, rerr_getstr3 (ret));
			}
		}
		if (df_name) {
			*df_name = list->list[i].df_name;
			if (!buf) {
				ret = spool_chstate (spooldir, *cf_name, SPOOL_STATE_ELAB, 0);
				if (!RERR_ISOK(ret)) {
					FRLOGF (LOG_WARN, "error changing state to \"in elab.\" for "
										"cf=>>%s<<: %s", *cf_name, rerr_getstr3(ret));
					free (*buf);
					goto cont_loop;
				}
				list->list[i].cf_name = NULL;
				list->list[i].df_name = NULL;
				list->next_to_elab = i+1;
				list->has_elab = 1;
				found=1;
			}
		}
		if (options) {
			*options = list->list[i].options;
			list->list[i].options=NULL;
		}
cont_loop:
		ret = unlock_single_file (spooldir, *cf_name);
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_NOTICE, "error unlocking file >>%s<<", *cf_name);
		}
		if (found) break;
	}
	if (found==0) {
		list->next_to_elab = list->listnum;
		ret = RERR_NOT_FOUND;
	}
	if (ret == RERR_NOT_FOUND) {
		FRLOGF (LOG_VERB, "no file found to unspool");
		return RERR_NOT_FOUND;
	} else if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error unspooling file: %s", rerr_getstr3 (ret));
		return ret;
	}
	FRLOGF (LOG_DEBUG, "unspooling file >>%s<<", *cf_name);
	return RERR_OK;
}


/* check_cfile:
 *		check's wether file is in state wait
 *
 *		spooldir:	spool directory
 *		cf_name:		name of cf file
 *
 *		returns 1 if file exists and is in state wait, 0 otherwise
 */
static
int
check_cfile (spooldir, cf_name)
	const char	*spooldir, *cf_name;
{
	int	state, ret;

	ret = get_entrystate (&state, spooldir, cf_name);
	if (!RERR_ISOK(ret)) return 0;
	if (state != SPOOL_STATE_WAIT) return 0;
	return 1;
}


/* lock_single_file:
 *		crea relativa lockfile per il file cf
 *
 *		spooldir:	cartella dello spool
 *		cf_name:		nome del file cf
 *		timeout:		time in micro seconds till we return unsuccessfull
 *
 *		ritorna un codice d'errore
 */
static
int
lock_single_file (spooldir, cf_name, timeout)
	const char	*spooldir, *cf_name;
	tmo_t			timeout;
{
	char	*filename;
	char	lf_name[256];
	int	ret = RERR_OK;

	if (!spooldir || !*spooldir || !cf_name || !*cf_name) return RERR_PARAM;
	if (*cf_name == '/') cf_name = rindex (cf_name, '/') + 1;
	strncpy (lf_name, cf_name, sizeof (lf_name)-1);
	lf_name[sizeof(lf_name)-1]=0;
	*lf_name = 'l';
	ret = mkfilename (&filename, spooldir, lf_name, NULL);
	if (!RERR_ISOK(ret)) return ret;
	ret = dolock_file (filename, timeout);
	free (filename);
	return ret;
}


/* unlock_single_file:
 *		cancella relativa lockfile per il file cf
 *
 *		spooldir:	cartella dello spool
 *		cf_name:	nome del file cf
 *
 *		ritorna un codice d'errore
 */
static
int
unlock_single_file (spooldir, cf_name)
	const char	*spooldir, *cf_name;
{
	char	*filename;
	char	lf_name[256];
	int	ret = RERR_OK;

	if (!spooldir || !*spooldir || !cf_name || !*cf_name) return RERR_PARAM;
	if (*cf_name == '/') cf_name = rindex (cf_name, '/') + 1;
	strncpy (lf_name, cf_name, sizeof (lf_name)-1);
	lf_name[sizeof(lf_name)-1]=0;
	*lf_name = 'l';
	ret = mkfilename (&filename, spooldir, lf_name, NULL);
	if (!RERR_ISOK(ret)) return ret;
	if (unlink (filename) == -1) ret = RERR_SYSTEM;
	free (filename);
	return ret;
}


/* dolock:
 *		crea lock file per la cartella spooldir
 *
 *		spooldir:	cartella dello spool
 *
 *		ritorna un codice d'errore
 */
static
int
dolock(spooldir, timeout)
	const char	*spooldir;
	tmo_t			timeout;
{
	char	*filename;
	int	ret;

	if (!spooldir || !*spooldir) return RERR_PARAM;
	ret = mkfilename (&filename, spooldir, "lock", NULL);
	if (!RERR_ISOK(ret)) return ret;
	ret = dolock_file (filename, timeout);
	free (filename);
	return ret;
}

/* dolock_file:
 *		crea il lockfile indicato di filename
 *
 *		filename:	nome del lockfile da creare
 *
 *		ritorna un codice d'errore
 */

static
int
dolock_file (filename, timeout)
	const char	*filename;
	tmo_t			timeout;
{
	struct stat		st;
	tmo_t				now, start;
	static tmo_t	diff=0;
	static tmo_t	last_nfscheck = 0;
	char				*tmpfile;
	const char		*s;
	int				tf_len;
	int				fd;

	if (!filename || !*filename) return RERR_PARAM;
	CF_MAY_READ;
	now = tmo_now ();
	if (nfs_workaround && (now - last_nfscheck > 600000000LL || last_nfscheck > now)) {
		s = rindex (filename, '/');
		s=s?s+1:filename;
		tf_len = (size_t)(s-filename)+16;
		tmpfile = malloc (tf_len);
		if (!tmpfile) return RERR_NOMEM;
		strncpy (tmpfile, filename, s-filename);
		strcpy (tmpfile+((size_t)(s-filename)), "tf_XXXXXX");
		now = tmo_now ();
		fd=mkstemp (tmpfile);
		if (fd < 0) {
			FRLOGF (LOG_ERR, "error creating tempfile (%s): %s", tmpfile, 
									rerr_getstr3(RERR_SYSTEM));
			free (tmpfile);
			return RERR_SYSTEM;
		}
		close (fd);
		if (stat (tmpfile, &st) != 0) {
			FRLOGF (LOG_ERR, "error stating tempfile (%s): %s", tmpfile,
									rerr_getstr3(RERR_SYSTEM));
			unlink (tmpfile);
			free (tmpfile);
			return RERR_SYSTEM;
		}
		diff = st.st_ctime - now;
		unlink (tmpfile);
		free (tmpfile);
		last_nfscheck = now;
	}

	if (last_nfscheck != now) {
		start = now;
	} else {
		start = tmo_now ();
	}
	while (1) {
		while (stat (filename, &st) == 0) {
			if (!S_ISREG (st.st_mode)) {
				if (!S_ISDIR (st.st_mode) && unlink (filename) == 0) continue;
				goto go_on;
			}
			now = tmo_now ();
			now+=diff;
			if ((stat (filename, &st) == 0) && (st.st_ctime + 2 < now)) {
				if (unlink (filename) != 0) goto go_on;
				continue;
			}
		}
		fd=open (filename, O_WRONLY|O_CREAT|O_EXCL, 0666);
		if (fd>=0) {
			close (fd);
			break;
		}
		now = tmo_now ();
		if (now-start > max_wait_for_lock && max_wait_for_lock>=0) break;
		if (timeout >= 0 && now-start > timeout) return RERR_TIMEDOUT;
		tmo_msleep (40);
	}
	return RERR_OK;
go_on:
	return RERR_INVALID_FILE;
}



/* dounlock:
 *		cancella lock file per la cartella spooldir
 *
 *		spooldir:	cartella dello spool
 *
 *		ritorna un codice d'errore
 */
static
int
dounlock(spooldir)
	const char	*spooldir;
{
	char	*filename;
	int	ret = RERR_OK;

	ret = mkfilename (&filename, spooldir, "lock", NULL);
	if (!RERR_ISOK(ret)) return ret;
	if (unlink (filename) == -1) ret = RERR_SYSTEM;
	free (filename);
	return ret;
}




/* getspooldir2:
 *		ritorna nome della cartella relativo allo spool
 *
 *		spool:	nome dello spool
 *
 *		ritorna nome della cartella dello spool
 */
static
int
getspooldir2 (spooldir_out, spool)
	char			**spooldir_out;
	const char	*spool;
{
	char			*spooldir;
	const char	*spooldir_c;

	CF_MAY_READ;
	if (!spooldir_out) return RERR_PARAM;
	*spooldir_out = NULL;
	if (!spool || !*spool) spool = default_spool;
	if (!spool || !*spool) return RERR_INVALID_SPOOL;
	if (*spool == '/' || (*spool == '.' && spool[1] == '/')) {
			spooldir = strdup (spool);
			if (spooldir_out) *spooldir_out = spooldir;
			if (!spooldir) return RERR_NOMEM;
			return RERR_OK;
	}
	spooldir_c = cf_getarr ("spooldir", spool);
	if (!spooldir_c) spooldir_c = cf_getarr2 ("spool_dir", spool, spool);
	if (*spooldir_c == '/') {
		spooldir = strdup (spooldir_c);
		if (spooldir_out) *spooldir_out = spooldir;
		if (!spooldir) return RERR_NOMEM;
		return RERR_OK;
	}
	spool = spooldir_c;
	spooldir=NULL;
	if (*base_spooldir=='/') {
		spooldir = malloc (strlen (base_spooldir) + strlen (spool) + 2);
		if (!spooldir) return RERR_NOMEM;
		sprintf (spooldir, "%s/%s", base_spooldir, spool);
	} else {
		spooldir = malloc (strlen (base_spooldir) + strlen (spool) + 14);
		if (!spooldir) return RERR_NOMEM;
		sprintf (spooldir, "/var/spool/%s/%s", base_spooldir, spool);
	}
	*spooldir_out = spooldir;
	return RERR_OK;
}



/* getspooldir:
 *		ritorna nome della cartella relativo allo spool
 *
 *		spool:	nome dello spool
 *
 *		ritorna nome della cartella dello spool
 */
static
int
getspooldir (spooldir_out, spool)
	char			**spooldir_out;
	const char	*spool;
{
	char	*spooldir;
	char	*s, *s2;
	int	ret;

	if (!spooldir_out) return RERR_PARAM;
	*spooldir_out = NULL;
	set_spoolwhat (spool);
	ret = getspooldir2 (&spooldir, spool);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error resolving spooldir: %s", rerr_getstr3 (ret));
		return ret;
	}
	for (s=s2=spooldir; *s; s2++,s++) {
		if (*s=='/') {
			*s2=*s;
			s2++;
			for (s++; *s=='/'; s++);
		}
		if (!*s) break;
		*s2=*s;
	}
	for (s=s2-1; s>spooldir && *s=='/'; s--) {}; s++;
	*s=0;
	if (!*spooldir || !strcmp (spooldir, "/") 
			|| !strcmp (spooldir, "/var/spool")) {
		FRLOGF (LOG_ERR, "invalid spooldir >>%s<<", spooldir);
		free (spooldir);
		return RERR_INVALID_SPOOL;
	}
	if ((ret = maymkdir (spooldir)) != RERR_OK) {
		FRLOGF (LOG_ERR, "error creating spooldir >>%s<<: %s", spooldir,
								rerr_getstr3 (ret));
		free (spooldir);
		return ret;
	}
	if (spooldir_out) *spooldir_out = spooldir;
	return RERR_OK;
}



/* maymkdir:
 *		controlla se la cartella dirname gia` esiste, se no lo crea
 *
 *		dirname:	cartella da creare
 *
 *		ritorna un codice d'errore
 */
static
int
maymkdir (dirname)
	const char	*dirname;
{
	struct stat	dstat;
	int			ret;
	char			*s;

	if (!dirname) return RERR_PARAM;
	if (stat (dirname, &dstat) == -1) {
		s = rindex (dirname, '/');
		if (s) {
			*s=0;
			ret = maymkdir (dirname);
			*s='/';
			if (!RERR_ISOK(ret)) return ret;
		}
		mkdir (dirname, 0777);
	}
	if (((ret=stat (dirname, &dstat))==-1) || !S_ISDIR(dstat.st_mode)) {
		if (ret == -1) {
			FRLOGF (LOG_ERR, "error creating dir >>%s<<: %s", dirname,
									rerr_getstr3(RERR_SYSTEM));
		} else {
			FRLOGF (LOG_ERR, "file >>%s<< exists, but is not a directory",
									dirname);
		}
		return RERR_SYSTEM;
	}
	return RERR_OK;
}


/* spool_list:
 *		legge lo spool (o prima sottocartella) senza aprire tutti i file cf
 *
 *		spool:	nome dello spool
 *		list|:	puntatore del risultato
 *
 *		ritorna un codice d'errore
 */
static
int
spool_list_light (spool, list)
	const char			*spool;
	struct spoollist	*list;
{
	char	*spooldir;
	int	ret;

	if (!list) return RERR_PARAM;
	ret = getspooldir (&spooldir, spool);
	if (!RERR_ISOK(ret)) return ret;
	*list = SPOOL_LIST_NULL;
	ret = readspool_first (spooldir, list);
	if (!RERR_ISOK(ret)) {
		free (spooldir);
		return ret;
	}
	list->spooldir = spooldir;
	list->spoolname = spool?strdup (spool):NULL;
	if (spool&&!list->spoolname) {
		hfree_spoollist (list);
		return RERR_NOMEM;
	}
	ret = sortlist_light (list);
	if (!RERR_ISOK(ret)) {
		hfree_spoollist (list);
		return ret;
	}
	return RERR_OK;
}



/* readspool:
 *		legge il contenuto dello spool o una sotto cartella
 *		senza aprire ogni singolo file cf
 *
 *		spooldir:	cartella dello spool
 *		list:			puntatore al risultato
 *
 *		ritorna un codice d'errore
 */

static
int
readspool_light (spooldir, list)
	const char			*spooldir;
	struct spoollist	*list;
{
	int				i;
	DIR				*d;
	struct dirent	*dent;

	if (!spooldir || !list) return RERR_PARAM;
	d = opendir (spooldir);
	if (!d) {
		FRLOGF (LOG_ERR, "error opening spooldir(%s): %s", spooldir,
								rerr_getstr3(RERR_SYSTEM));
		return RERR_SYSTEM;
	}
	while ((dent=readdir (d))) {
		if (strncmp (dent->d_name, "cf_", 3) != 0) continue;
		i=list->listnum;
		list->list = realloc (list->list, (i+1) *
							sizeof (struct spool_entry));
		if (!list->list) {
			closedir (d);
			return RERR_NOMEM;
		}
		list->list[i] = SPOOL_ENTRY_NULL;
		mkfilename2 (&(list->list[i].cf_name), spooldir, dent->d_name, NULL, FLAG_MKFILE_NOSUB);
		list->listnum++;
	}
	closedir (d);
	return RERR_OK;
}


/* readspool:
 *		legge il contenuto dello spool (aprendo ogni singolo file cf
 *		non funziona con sotto cartella
 *
 *		spooldir:	cartella dello spool
 *		list:			puntatore al risultato
 *
 *		ritorna un codice d'errore
 */
static
int
readspool (spooldir, list)
	const char			*spooldir;
	struct spoollist	*list;
{
	int				i, ret=RERR_OK;
	DIR				*d;
	struct dirent	*dent;

	if (!spooldir || !list) return RERR_PARAM;
	d = opendir (spooldir);
	if (!d) {
		FRLOGF (LOG_ERR, "error opening spooldir(%s): %s", spooldir,
								rerr_getstr3(RERR_SYSTEM));
		return RERR_SYSTEM;
	}
	while ((dent=readdir (d))) {
		if (strncmp (dent->d_name, "cf_", 3) != 0) continue;
		i=list->listnum;
		list->list = realloc (list->list, (i+1) *
							sizeof (struct spool_entry));
		if (!list->list) {
			closedir (d);
			return RERR_NOMEM;
		}
		list->list[i] = SPOOL_ENTRY_NULL;
		ret = read_spoolentry (&(list->list[i]), spooldir, dent->d_name);
		if (RERR_ISOK(ret)) {
			list->listnum++;
		}
	}
	closedir (d);
	return RERR_OK;
}

/* get_entrystate:
 *		legge il contenuto del file cf e ritorna lo stato
 *
 *		state:		puntatore al risultato
 *		spooldir:	cartella dello spool
 *		cf_name:		nome del file cf
 *
 *		ritorna un codice d'errore
 */
static
int
get_entrystate (state, spooldir, cf_name)
	int			*state;
	const char	*spooldir, *cf_name;
{
	struct spool_entry	entry;
	int						ret;

	if (!state || !spooldir || !cf_name) return RERR_PARAM;
	ret = read_spoolentry (&entry, spooldir, cf_name);
	if (!RERR_ISOK(ret)) return ret;
	*state = entry.state;
	hfree_spoolentry (&entry);
	return RERR_OK;
}


/* read_spoolentry:
 *		legge il contenuto del file cf e lo salva in entry
 *
 *		entry:		puntatore al risultato
 *		spooldir:	cartella dello spool
 *		cf_name:		nome del file cf
 *
 *		ritorna un codice d'errore
 */
static
int
read_spoolentry (entry, spooldir, cf_name)
	struct spool_entry	*entry;
	const char				*spooldir, *cf_name;
{
	char			*filename;
	FILE			*f;
	char			*buf, *ptr, *line, *var, *val, *s;
	struct stat	sbuf;
	int			ret, fd;

	if (!entry || !spooldir || !*spooldir || !cf_name || !*cf_name)
		return RERR_PARAM;
	CF_MAY_READ;
	if (spool_to_subdir) {
		ret = mkfilename_check (&filename, spooldir, cf_name);
	} else {
		ret = mkfilename (&filename, spooldir, cf_name, NULL);
	}
	if (!RERR_ISOK(ret)) return ret;
	f = fopen (filename, "r");
	if (!f) {
		FRLOGF (LOG_WARN, "error opening file >>%s<<: %s", filename,
								rerr_getstr3(RERR_SYSTEM));
		free (filename);
		return RERR_SYSTEM;
	}
	buf = fop_read_file (f);
	fclose (f);
	if (!buf) {
		FRLOGF (LOG_WARN, "error reading file >>%s<<", filename);
		free (filename);
		return RERR_SYSTEM;
	}
	if (entry->cf_name) free (entry->cf_name);
	entry->cf_name = filename;
	ptr = buf;
	while ((line=top_getline(&ptr, 0))) {
		var = line;
		s = index (line, ':');
		if (!s) continue;
		val = top_skipwhite (s+1);
		for (s--; iswhite (*s); s--) {}; s++;
		*s=0;
		sswitch (var) {
		sicase ("df")
			if (entry->df_name) free (entry->df_name);
			ret = mkfilename_check (&filename, spooldir, val);
			if (!RERR_ISOK(ret)) {
				FRLOGF (LOG_NOTICE, "error finding df file: %s",
											rerr_getstr3(ret));
				free (entry->cf_name);
				*entry = SPOOL_ENTRY_NULL;
				free (buf);
				return ret;
			}
			entry->df_name = filename;
			break;
		sicase ("size")
			entry->size = atoi (val);
			break;
		sicase ("ctime")
			entry->ctime = cjg_gettimestr (val);
			break;
		sicase ("mtime")
			entry->mtime = cjg_gettimestr (val);
			break;
		sicase ("status")
		sicase ("state")
			sswitch (val) {
			sicase ("wait") 
				entry->state = SPOOL_STATE_WAIT;
				break;
			sicase ("error")
				entry->state = SPOOL_STATE_ERROR;
				break;
			sicase ("elab")
			sicase ("inelab")
			sicase ("elaboration")
			sicase ("inelaboration")
				entry->state = SPOOL_STATE_ELAB;
				break;
			sdefault
				entry->state = SPOOL_STATE_UNKNOWN;
				break;
			} esac;
			break;
		sicase ("pid")
			entry->pid = atoi (val);
			break;
		sicase ("num")
		sicase ("cnt")
		sicase ("counter")
			entry->num = atoi (val);
			break;
		sicase ("error")
			entry->error = atoi (val);
			break;
		sicase ("options")
			entry->options = strdup (val);
			break;
		} esac;
	}
	free (buf);
	if (!entry->df_name) {
		filename = strdup (entry->cf_name);
		if (!filename) {
			free (entry->cf_name);
			*entry = SPOOL_ENTRY_NULL;
			return RERR_NOMEM;
		}
		s = rindex (filename, '/');
		if (!s) s=filename;
		s = top_skipwhiteplus (s, "/");
		if (!s || *s != 'c') {
			free (filename);
			free (entry->cf_name);
			*entry = SPOOL_ENTRY_NULL;
			return RERR_INVALID_FILE;
		}
		*s = 'd';
		entry->df_name = filename;
	}
	if (((ret=stat (entry->df_name, &sbuf))==-1) || !S_ISREG (sbuf.st_mode)
				|| ((fd=open (entry->df_name, O_RDONLY|O_LARGEFILE))==-1)) {
		if (ret == -1) {
			FRLOGF (LOG_WARN, "error stating datefile >>%s<<: %s",
									entry->df_name,rerr_getstr3(RERR_SYSTEM));
		} else if (!S_ISREG (sbuf.st_mode)) {
			FRLOGF (LOG_WARN, "datefile >>%s<< is not a regular file",
									entry->df_name);
		} else {
			FRLOGF (LOG_WARN, "error opening datefile >>%s<<: %s",
									entry->df_name, rerr_getstr3(RERR_SYSTEM));
		}
		free (entry->cf_name);
		free (entry->df_name);
		*entry = SPOOL_ENTRY_NULL;
		return RERR_SYSTEM;
	} else {
		close (fd);
	}
	return RERR_OK;
}




/* write_spoolentry:
 *		scrive il contenuto dello file cf
 *
 *		entry:		struttura con tutti i dati da scrivere
 *		spooldir:	cartella dello spool
 *
 *		ritorna un codice d'errore
 */
static
int
write_spoolentry (entry, spooldir)
	struct spool_entry	*entry;
	const char				*spooldir;
{
	return write_spoolentry2 (entry, spooldir, 0);
}




/* write_spoolentry2:
 *		scrive il contenuto dello file cf
 *
 *		entry:		struttura con tutti i dati da scrivere
 *		spooldir:	cartella dello spool
 *		flags:		FLAG_WRSPOOLENTRY_MAYERROR:	indica che il file puo trovarsi
 *																anche nella cartella error
 *
 *		ritorna un codice d'errore
 */
static
int
write_spoolentry2 (entry, spooldir, flags)
	struct spool_entry	*entry;
	const char				*spooldir;
	int						flags;
{
	char			*filename, *df_name;
	const char	*status;
	FILE			*f;
	int			ret;
	char			mtime[48], ctime[48];

	if (!entry || !spooldir || !*spooldir) return RERR_PARAM;
	CF_MAY_READ;

	if (spool_to_subdir && (flags & FLAG_WRSPOOLENTRY_MAYERROR)) {
		ret = mkfilename_check (&filename, spooldir, entry->cf_name);
	} else {
		ret = mkfilename (&filename, spooldir, entry->cf_name, NULL);
	}
	if (!RERR_ISOK(ret)) return ret;
	f = fopen (filename, "w");
	if (!f) {
		FRLOGF (LOG_ERR, "error opening file >>%s<< for writing: %s",
								filename, rerr_getstr3(RERR_SYSTEM));
	}
	free (filename);
	if (!f) return RERR_SYSTEM;
	switch (entry->state) {
	case SPOOL_STATE_WAIT:
		status="wait";
		break;
	case SPOOL_STATE_ELAB:
		status="elab";
		break;
	case SPOOL_STATE_ERROR:
		status="error";
		break;
	default:
		status="unknown";
		break;
	}

	if (entry->df_name) {
		df_name = (df_name = rindex (entry->df_name, '/')) ? df_name+1 : 
					entry->df_name;
	} else {
		df_name = (df_name = rindex (entry->cf_name, '/')) ? df_name+1 : 
					entry->cf_name;
	}
	entry->pid = (int) getpid();
	entry->mtime = tmo_now ();
	cjg_prttimestr (mtime, sizeof (mtime), entry->mtime, CJG_TSTR_T_D);
	cjg_prttimestr (ctime, sizeof (ctime), entry->ctime, CJG_TSTR_T_D);
	if (!entry->ctime) entry->ctime = entry->mtime;
	fprintf (f, "num: %d\npid: %d\nstatus: %s\nerror: %d\nctime: %s\nmtime: %s\n"
				"df: d%s\nsize: %d\n", entry->num, entry->pid, status,
				entry->state==SPOOL_STATE_ERROR?entry->error:0,
				ctime, mtime, df_name+1, entry->size);
	if (entry->options) {
		fprintf (f, "options: %s\n",  entry->options);
	}
	fclose (f);
	return RERR_OK;
}



/* getfilesize:
 *     returns file size
 *
 *     fsize:    size (return value)
 *     fname:    filname
 *
 *     returns an error code
 */
static
int
getfilesize (fsize, fname)
	int			*fsize;
	const char	*fname;
{
	struct stat	buf;

	if (!fsize || !fname) return RERR_PARAM;
	if (stat (fname, &buf) < 0) {
		FRLOGF (LOG_ERR, "error stat'ing file >>%s<<: %s", fname,
								rerr_getstr3(RERR_SYSTEM));
		return RERR_SYSTEM;
	}
	*fsize = buf.st_size;
	return RERR_OK;
}


/* write_df:
 *		scrive il file df
 *
 *		entry:		struttura che contienne il nome dell file
 *		spooldir:	cartella dello spool
 *		buf:			buffer da scrivere
 *		buflen:		lunghezza del buffer
 *
 *		ritorna un codice d'errore
 */
static
int
write_df (entry, spooldir, buf, buflen, fname, dolink, fd)
	struct spool_entry	*entry;
	const char				*spooldir;
	char						*buf;
	int						buflen, dolink, fd;
	const char				*fname;
{
	char	*filename;
	int	ret;
	int	fdd=-1, fdin=-1;
	char	tbuf[1024];
	int	num;

	ret = mkfilename (&filename, spooldir, entry->df_name, entry->cf_name);
	if (!RERR_ISOK(ret)) return ret;
	fdd = open (filename, O_WRONLY|O_LARGEFILE);
	if (fdd < 0) {
		FRLOGF (LOG_ERR, "error opening file >>%s<< for writing: %s", filename,
								rerr_getstr3(RERR_SYSTEM));
		free (filename);
		return RERR_SYSTEM;
	}
	if (buf) {
		while (buflen>0) {
			ret = write (fdd, buf, buflen);
			if (ret <= 0) {
				if (ret == -1) {
					FRLOGF (LOG_ERR, "error writing to file >>%s<<: %s", filename,
											rerr_getstr3(RERR_SYSTEM));
				} else {
					FRLOGF (LOG_WARN, "truncated write to file >>%s<<", filename);
				}
				free (filename);
				close (fdd);
				return (ret==-1)?RERR_SYSTEM:RERR_OK;
			}
			buf+=ret;
			buflen-=ret;
		}
	} else if (!dolink || fd>=0) {
		if (fd<0) {
			if (strcmp (fname, "-")!=0) {
				fdin = open (fname, O_RDONLY|O_LARGEFILE);
				if (fdin<0) {
					FRLOGF (LOG_ERR, "error opening file >>%s<< for reading: %s",
											fname, rerr_getstr3(RERR_SYSTEM));
					free (filename);
					close (fdd);
					return RERR_SYSTEM;
				}
			} else {
				fdin = 0;
			}
		} else {
			fdin = fd;
		}
		while ((num=read (fdin, tbuf, sizeof(buf)))>0) {
			ret = write (fdd, tbuf, num);
			if (ret < num) {
				if (ret == -1) {
					FRLOGF (LOG_ERR, "error writing to file >>%s<<: %s", filename,
											rerr_getstr3(RERR_SYSTEM));
				} else {
					FRLOGF (LOG_WARN, "truncated write to file >>%s<<", filename);
				}
				free (filename);
				if (fdin>0) close (fdin);
				if (fdd>0 && fdd!=fd) close (fdd);
				return (ret==-1)?RERR_SYSTEM:RERR_OK;
			}
		}
		if (fdd>0 && fdd!=fd) close (fdd);
	} else {
		close (fdd);
		fdd=-1;
		unlink (filename);
		if (link (fname, filename) < 0) {
			FRLOGF (LOG_ERR, "error linking %s->%s: %s", fname, filename,
									rerr_getstr3(RERR_SYSTEM));
			free (filename);
			return RERR_SYSTEM;
		}
		unlink (fname);
	}
	free (filename);
	if (fdd>0) close (fdd);
	return RERR_OK;
}


/* num_to_cf:
 *		ritorna il file cf correspondente al contatore
 *
 *		cf_name:		buffer per salvare il nome del file cf
 *		spooldir:	cartella dello spool
 *		num:			contatore
 *
 *		ritorno un codice d'errore
 */
static
int
num_to_cf (cf_name, spooldir, num)
	char			**cf_name;
	const char	*spooldir;
	int			num;
{
	char				subdir[64], *dname, *s;
	int				ret, n, first;
	DIR				*d;
	struct dirent	*dent;

	if (!cf_name || !spooldir || num<0) return RERR_PARAM;
	ret = getsubdir (subdir, spooldir, num, SUBDIR_NAMEONLY);
	if (!RERR_ISOK(ret)) {
		return ret;
	}
	if (spool_to_subdir) {
		ret = mkfilename2 (&dname, spooldir, subdir, NULL, FLAG_MKFILE_NOSUB);
		if (!RERR_ISOK(ret)) return ret;
		first=1;
	} else {
		dname = strdup (spooldir);
		if (!dname) return RERR_NOMEM;
		first=0;
	}
begin_loop:
	d = opendir (dname);
	if (!d) {
		if (errno == ENOENT) goto finish_loop;
		FRLOGF (LOG_ERR, "error opening dir >>%s<<: %s", dname,
								rerr_getstr3(RERR_SYSTEM));
		free (dname);
		return RERR_SYSTEM;
	}
	while ((dent=readdir (d))) {
		if (strncmp (dent->d_name, "cf_", 3) != 0) continue;
		s = dent->d_name + 3;
		n=0;
		for (; isdigit (*s); s++) {
			n*=10; n+=*s-'0';
		}
		if (n==num) {
			ret = mkfilename2 (cf_name, dname, dent->d_name, NULL, 
									FLAG_MKFILE_NOSUB);
			closedir (d);
			free (dname);
			if (!RERR_ISOK(ret)) return ret;
			return RERR_OK;
		}
	}
	closedir (d);
finish_loop:
	free (dname);
	if (first) {
		ret = mkfilename2 (&dname, spooldir, "error", NULL, FLAG_MKFILE_NOSUB);
		if (!RERR_ISOK(ret)) return ret;
		first=0;
		goto begin_loop;
	}
	return RERR_NOT_FOUND;
}

/* read_df:
 *		legge il file df
 *
 *		entry:		struttura che contienne il nome del file
 *		spooldir:	cartella dello spool
 *		buf:			buffer (output)
 *		buflen:		lunghezza del buffer (output)
 *
 *		ritorna un codice d'errore
 */
static
int
read_df (entry, spooldir, buf, buflen)
	struct spool_entry	*entry;
	const char				*spooldir;
	char						**buf;
	int						*buflen;
{
	int	fd;
	char	*filename;
	int	ret;

	if (!entry || !spooldir || !*spooldir || !buf) return RERR_PARAM;
	ret = mkfilename (&filename, spooldir, entry->df_name, entry->cf_name);
	if (!RERR_ISOK(ret)) return ret;
	fd = open (filename, O_RDONLY|O_LARGEFILE);
	if (fd<0) {
		FRLOGF (LOG_ERR, "error opening file >>%s<<: %s", filename,
								rerr_getstr3(RERR_SYSTEM));
		free (filename);
		return RERR_SYSTEM;
	}
	*buf = fop_read_fd2 (fd, buflen);
	close (fd);
	if (!*buf) {
		FRLOGF (LOG_ERR, "error reading from file >>%s<<", filename);
		free (filename);
		return RERR_SYSTEM;
	}
	free (filename);
	return RERR_OK;
}







/* del_spoollistentry:
 *		rimuove elemento della lista spoollist
 *
 *		list:	lista in quale rimuove un elemento
 *		i:		numero del elemento da rimuovere
 *
 *		ritorna un codice d'errore
 */
static
int
del_spoollistentry (list, i)
	struct spoollist	*list;
	int					i;
{
	if (!list || i<0 || i>=list->listnum) return RERR_PARAM;
	hfree_spoolentry (&(list->list[i]));
	list->listnum--;
	for (; i<list->listnum; i++) {
		list->list[i] = list->list[i+1];
	}
	return RERR_OK;
}





/* del_notwanted:
 *		rimuove di list tutti gli elementi in quale non siamo interessato
 *
 *		list:	lista in quale rimuovere degli elementi
 *		flag:	SPOOL_FLAG_PRT_ERROR:	*NON* rimuove elementi in stato di error
 *				SPOOL_FLAG_PRT_WAIT:		*NON* rimuove elementi in stato di wait
 *				SPOOL_FLAG_PRT_ELAB:		*NON* rimuove elementi in stato di elab.
 *				SPOOL_FLAG_PRT_UNKNOWN:	*NON* rimuove elementi in stato unknown
 *
 *		ritorna un codice d'errore
 */
static
int
del_notwanted (list, flag)
	struct spoollist	*list;
	int					flag;
{
	int	i;

	if (!list) return RERR_PARAM;
	for (i=0; i<list->listnum; i++) {
		if (!PRT_STATE (list->list[i].state, flag)) {
			if (del_spoollistentry (list, i) == RERR_OK) i--;
		}
	}
	return RERR_OK;
}




/* sortlist:
 *		sorta la lista
 *
 *		list:	lista da sortare
 *		flag:	SPOOL_FLAG_SORT_NUM:	sorta secondo il contatore
 *				SPOOL_FLAG_SORT_CTIME:	sorta secondo ctime
 *				SPOOL_FLAG_SORT_MTIME:	sorta secondo mtime
 *				SPOOL_FLAG_SORT_STATE:	sorta secondo lo stato
 *
 *		ritorna un codice d'errore
 */
static
int
sortlist (list, flag)
	struct spoollist	*list;
	int					flag;
{
	int	ret;

	if (!list) return RERR_PARAM;
	if (flag & SPOOL_FLAG_SORT_NUM)
		if ((ret = sortlist2 (list, SPOOL_SORT_NUM))!=RERR_OK)
			return ret;
	if (flag & SPOOL_FLAG_SORT_CTIME)
		if ((ret = sortlist2 (list, SPOOL_SORT_CTIME))!=RERR_OK)
			return ret;
	if (flag & SPOOL_FLAG_SORT_MTIME)
		if ((ret = sortlist2 (list, SPOOL_SORT_MTIME))!=RERR_OK)
			return ret;
	if (flag & SPOOL_FLAG_SORT_STATE)
		if ((ret = sortlist2 (list, SPOOL_SORT_STATE))!=RERR_OK)
			return ret;
	return RERR_OK;
}


/* sortlist_light:
 *		sorta lista secondo il contatore 
 *		usando il contatore presente nello file
 *
 *		list:	lista da sortare
 *
 *		ritorna un codice d'errore
 */
static
int
sortlist_light (list)
	struct spoollist	*list;
{
	int	changed=1, i, num;
	char	*s;

	if (!list) return RERR_PARAM;
	del_emptyentries (list);
	for (i=0; i<list->listnum; i++) {
		num=0;
		s=(s=rindex(list->list[i].cf_name,'/'))?s+1:list->list[i].cf_name;
		s=index (s, '_');
		if (!s) continue;
		for (s++; *s>='0' && *s<='9'; s++) {
			num*=10; num+=*s-'0';
		}
		list->list[i].num=num;
	}
	while (changed) {
		changed=0;
		for (i=0; i<list->listnum-1; i++) {
			if (list->list[i].num > list->list[i+1].num) {
				s=list->list[i].cf_name;
				num=list->list[i].num;
				list->list[i].cf_name = list->list[i+1].cf_name;
				list->list[i].num = list->list[i+1].num;
				list->list[i+1].cf_name = s;
				list->list[i+1].num = num;
				changed=1;
			}
		}
	}
	return RERR_OK;
}


/* sortlist2:
 *		sorta lista
 *
 *		list:	lista da sortare
 *		what:	SPOOL_SORT_STATE:	sortare secondo lo stato
 *				SPOOL_SORT_NUM:	sortare secondo il contatore
 *				SPOOL_SORT_CTIME:	sortare secondo ctime
 *				SPOOL_SORT_MTIME:	sortare secondo mtime
 *
 *		ritorna un codice d'errore
 */
static
int
sortlist2 (list, what)
	struct spoollist	*list;
	int					what;
{
	int						changed=1, toswap, i;
	struct spool_entry	dummy;

	if (!list) return RERR_PARAM;
	while (changed) {
		changed=0;
		for (i=0; i<list->listnum-1; i++) {
			toswap=0;
			switch (what) {
			case SPOOL_SORT_STATE:
				if (list->list[i].state > list->list[i+1].state) toswap=1;
				break;
			case SPOOL_SORT_NUM:
				if (list->list[i].num > list->list[i+1].num) toswap=1;
				break;
			case SPOOL_SORT_CTIME:
				if (list->list[i].ctime > list->list[i+1].ctime) toswap=1;
				break;
			case SPOOL_SORT_MTIME:
				if (list->list[i].mtime > list->list[i+1].mtime) toswap=1;
				break;
			}
			if (toswap) {
				dummy=list->list[i];
				list->list[i]=list->list[i+1];
				list->list[i+1]=dummy;
				changed=1;
			}
		}
	}
	return RERR_OK;
}









/* mkerrorfilename:
 *		create filename in error directory
 *
 *		out_name:   created name;
 *		spooldir:	spooldir name
 *		cf_name:		name of cf_ file in spooldir
 *
 *		returns an error code
 */
static
int
mkerrorfilename (out_name, spooldir, cf_name)
	char			**out_name;
	const char	*spooldir, *cf_name;
{
	char			*subdir, *filename;
	int			ret;
	const char	*s;

	if (!out_name || !spooldir || !cf_name) return RERR_PARAM;
	ret = mkfilename2 (&subdir, spooldir, "error", NULL, FLAG_MKFILE_NOSUB);
	if (!RERR_ISOK(ret)) return ret;
	s = (s=rindex (cf_name, '/'))?s+1:cf_name;
	filename = malloc (strlen (subdir) + strlen (s) + 10);
	if (!filename) {
		free (subdir);
		return RERR_NOMEM;
	}
	sprintf (filename, "%s/%s", subdir, s);
	free (subdir);
	*out_name = filename;
	return RERR_OK;
}


/* mkfilename_check:
 *		create filename either in sub directory or in error-subdir if an error occurs
 *
 *		out_name:	created filename
 *		spooldir:	name of spool dir
 *		cf_name:		name of cf_ file.
 *
 *		returns an error code
 */
static
int
mkfilename_check (out_name, spooldir, cf_name)
	const char	*spooldir, *cf_name;
	char			**out_name;
{
	char			*filename;
	struct stat	st;
	int			ret;

	if (!out_name || !spooldir || !cf_name) return RERR_PARAM;
	CF_MAY_READ;
	ret = mkfilename (&filename, spooldir, cf_name, NULL);
	if (!RERR_ISOK(ret)) return ret;
	if (stat (filename, &st) == 0) {
		*out_name = filename;
		return RERR_OK;
	}
	free (filename);
	if (spool_to_subdir) {
		ret = mkerrorfilename (&filename, spooldir, cf_name);
		if (!RERR_ISOK(ret)) return ret;
		if (stat (filename, &st) == 0) {
			*out_name = filename;
			return RERR_OK;
		}
		free (filename);
	}
	return RERR_NOT_FOUND;
}

/* mkfilename:
 *		creates the ablolute path to an file relative to spooldir or subdir
 *
 *		out_name:	created filename (have to be freed)
 *		spooldir:	spool directory
 *		filename:	relative filename
 *		file2:		alternative filename
 *
 *		returns an error code
 */
static
int
mkfilename (out_name, spooldir, filename, file2)
	char			**out_name;
	const char	*spooldir, *filename, *file2;
{
	return mkfilename2 (out_name, spooldir, filename, file2, 0);
}


/* mkfilename2:
 *		creates the ablolute path to an file relative to spooldir or subdir
 *
 *		out_name:	created filename (have to be freed)
 *		spooldir:	spool directory
 *		filename:	relative filename
 *		file2:		alternative filename
 *		flags:		FLAG_MKFILE_NOSUB: file is not in subdir
 *
 *		returns an error code
 */
static
int
mkfilename2 (out_name, spooldir, filename, file2, flags)
	char			**out_name;
	const char	*spooldir, *filename, *file2;
	int			flags;
{
	char	*fn, *s, *s2, subdir[64];
	int	num, ret, havesubdir=0;
	char	*fname;

	if (!((filename && *filename) || (file2 && *file2))) return RERR_PARAM;
	if (!out_name || !spooldir || !*spooldir) return RERR_PARAM;
	if (!filename) {
		fname=strdup (file2);
		if (!fname) return RERR_NOMEM;
		s=(s=rindex (fname, '/'))?s+1:fname;
		if (*fname != 'c') {
			free (fname);
			return RERR_INVALID_NAME;
		}
		*s='d';
	} else {
		fname = strdup (filename);
		if (!fname) return RERR_NOMEM;
	}
	if (*fname=='/' || (*fname == '.' && fname[1] == '/')) {
		*out_name = fname;
	} else {
		if (!(flags & FLAG_MKFILE_NOSUB)) {
			s=(s=rindex (fname, '/'))?s+1:fname;
			if ((s=index(s, '_')) && (s2=index (++s, '_'))) {
				*s2=0;
				if (!*s) {
					num=0;
				} else {
					num=atoi (s);
				}
				*s2='_';
				ret = getsubdir (subdir, spooldir, num, SUBDIR_NAMEONLY);
				if (!RERR_ISOK(ret)) {
					free (fname);
					return ret;
				}
				havesubdir=1;
			}
		}
		fn = malloc (strlen (spooldir) + (havesubdir?strlen(subdir):0) + 
										strlen (fname) + 4);
		if (!fn) {
			free (fname);
			return RERR_NOMEM;
		}
		if (havesubdir) {
			sprintf (fn, "%s/%s/%s", spooldir, subdir, fname);
		} else {
			sprintf (fn, "%s/%s", spooldir, fname);
		}
		free (fname);
		*out_name = fn;
	}
	return RERR_OK;
}



/* getsubdir:
 *		calculates the subdir to corresponding runnum, and
 *		creates it, if it does not exist
 *
 *		subdir:		subdir name, filled in by getsubdir
 *						the buffer needs to be at least 64 bytes long
 *		spooldir:	spool directory
 *		runnum:		counter
 *		flag:			SUBDIR_NAMEONLY:	calculate only name, not the
 *												directory itself
 *						SUBDIR_LOCK:		leave subdir locked
 *						SUBDIR_UNLOCK:		delete lock of subdir before processing
 *
 *		returns an error code
 */
static
int
getsubdir (subdir, spooldir, runnum, flag)
	char			*subdir;
	const char	*spooldir;
	unsigned		runnum, flag;
{
	char			*filename;
	struct stat	st;
	int			i, n, ret;

	if (!subdir || !spooldir) return RERR_PARAM;
	CF_MAY_READ;
	if (!spool_to_subdir) return RERR_OK;
	runnum-=runnum%num_files_per_subdir;
	sprintf (subdir, "sd_%u", runnum);
	if (flag&SUBDIR_NAMEONLY) return RERR_OK;

	filename=malloc (strlen (spooldir) + strlen (subdir) + 4);
	if (!filename) return RERR_NOMEM;
	sprintf (filename, "%s/%n%s", spooldir, &n, subdir);
	if (!((flag&SUBDIR_LOCK) || (flag&SUBDIR_UNLOCK)) && 
								(stat (filename, &st) == 0)) {
		free (filename);
		if (S_ISDIR (st.st_mode)) {
			return RERR_OK;
		} else {
			FRLOGF (LOG_ERR, ">>%s<< does exist, but is not a directory",
									filename);
			return RERR_SYSTEM;
		}
	}
	filename[n]='l';

	if (flag&SUBDIR_UNLOCK) {
		unlink (filename);
		free (filename);
		return RERR_OK;
	}

	ret = dolock_file (filename, lockfile_timeout);
	if (!RERR_ISOK(ret)) {
		free (filename);
		FRLOGF (LOG_ERR, "error creating lock file >>%s<<: %s", filename, 
								rerr_getstr3(ret));
		return ret;
	}

	for (i=2; i>0; i--) {
		filename[n]='s';
		if (mkdir (filename, 0755) < 0) {
			if (errno == EEXIST) {
				if ((stat (filename, &st) == 0) && !S_ISDIR (st.st_mode)) {
					if (i<=1) {
						FRLOGF (LOG_ERR, ">>%s<< does exist, but is not a directory",
											filename);
					}
					ret = RERR_SYSTEM;
				} else {
					ret = RERR_OK;
					break;
				}
			} else {
				if (i<=1) {
					FRLOGF (LOG_ERR, "error creating >>%s<<: %s", filename,
											rerr_getstr3(RERR_SYSTEM));
				}
				ret = RERR_SYSTEM;
			}
		} else {
			ret = RERR_OK;
			break;
		}
	}
	if ((!RERR_ISOK(ret)) || !(flag&SUBDIR_LOCK)) {
		filename[n]='l';
		unlink (filename);
	}
	free (filename);
	return ret;
}



/* checkelabtime:
 *		se il tempo massimo della elaborazione e` scaduto cambia
 *		lo stato in ERROR
 *
 *		spooldir:	cartella dello spool
 *		entry:		entry - da modificare
 *
 *		ritorna un codice d'errore
 */
static
int
checkelabtime (spooldir, entry)
	const char				*spooldir;
	struct spool_entry	*entry;
{
	tmo_t	now;
	int	ret;

	if (!entry) return RERR_PARAM;
	CF_MAY_READ;
	if (max_elabtime <= 0) return RERR_OK;		/* don't do any checks */
	now = tmo_now ();
	if (entry->mtime > now) {
		FRLOGF (LOG_NOTICE, "clock skew detected: >>%s/%s<< is in the future "
									"by %d seconds - try to correct it",
									spooldir?spooldir:"???", entry->cf_name,
									(int) ((entry->mtime - now)/1000000LL));
		entry->mtime = now;
		ret = write_spoolentry2 (entry, spooldir, FLAG_WRSPOOLENTRY_MAYERROR);
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_NOTICE, "error correcting timestamp for >>%s/%s<<: %s",
									spooldir?spooldir:"???", entry->cf_name,
									rerr_getstr3(ret));
		}
	}
	if (entry->mtime + max_elabtime < now) {
		if (reinsert_ontimeout) {
			FRLOGF (LOG_INFO, "reinserting >>%s<<, which has an elabortion time "
									"of %d seconds", entry->cf_name, 
									(int) ((now - entry->mtime)/1000000LL));
			ret = dospool_chstate (spooldir, entry->cf_name, 
								SPOOL_STATE_WAIT, RERR_OK, NULL);
			if (!RERR_ISOK(ret)) {
				FRLOGF (LOG_WARN, "error changing state to wait for cf >>%s<<: %s",
									entry->cf_name, rerr_getstr3(ret));
				return ret;
			}
			entry->state = SPOOL_STATE_ERROR;
			entry->error = RERR_TIMEDOUT;
		} else {
			FRLOGF (LOG_INFO, "making error of >>%s<<, which has an elabortion "
									"time of %d seconds", entry->cf_name,
									(int)((now - entry->mtime) / 1000000LL));
			ret = dospool_chstate (spooldir, entry->cf_name, 
								SPOOL_STATE_ERROR, RERR_TIMEDOUT, NULL);
			if (!RERR_ISOK(ret)) {
				FRLOGF (LOG_WARN, "error changing state to wait for cf "
									">>%s<<: %s", entry->cf_name, 
									rerr_getstr3(ret));
				return ret;
			}
			entry->state = SPOOL_STATE_ERROR;
			entry->error = RERR_TIMEDOUT;
		}
	}
	return RERR_OK;
}


/* getspooldirlist:
 *		legge la lista delle sotto cartelle
 *
 *		list:		puntatore sul risultato
 *		spooldir:	cartella  dello spool
 *
 *		ritorna un codice d'errore
 */
static
int
getspooldirlist (list, spooldir)
	struct spoollist	*list;
	const char			*spooldir;
{
	int				i, ret;
	DIR				*d;
	struct dirent	*dent;
	char				*s;
	struct stat		st;

	if (!spooldir || !list) return RERR_PARAM;
	d = opendir (spooldir);
	if (!d) {
		FRLOGF (LOG_ERR, "error opening spooldir(%s): %s", spooldir,
								rerr_getstr3(RERR_SYSTEM));
		return RERR_INTERNAL;
	}
	while ((dent=readdir (d))) {
		if (strncmp (dent->d_name, "sd_", 3) != 0) continue;
		i=list->listnum;
		list->list = realloc (list->list, (i+1) *
							sizeof (struct spool_entry));
		if (!list->list) {
			closedir (d);
			return RERR_NOMEM;
		}
		list->list[i] = SPOOL_ENTRY_NULL;
		ret = mkfilename2 (&(list->list[i].cf_name), spooldir, dent->d_name, NULL, 
											 FLAG_MKFILE_NOSUB);
		if (!RERR_ISOK(ret)) {
			hfree_spoollist (list);
			closedir (d);
			return ret;
		}
		if (stat (list->list[i].cf_name, &st)<0 || !S_ISDIR(st.st_mode)) {
			free (list->list[i].cf_name);
			continue;
		}
		s=(s=rindex (list->list[i].cf_name, '/'))?s+1:list->list[i].cf_name;
		s=index (s, '_');
		if (!s) {
			free (list->list[i].cf_name);
			continue;
		}
		list->list[i].num = cf_atoi (s+1);
		list->listnum++;
	}
	closedir (d);
	ret = sortlist_light (list);
	if (!RERR_ISOK(ret)) {
		hfree_spoollist (list);
		return ret;
	}
	return RERR_OK;
}


/* readspool_first:
 *		legge la prima sotto cartella che non e` vuota
 *
 *		spooldir:	cartella dello spool
 *		list:			puntatore al risultato
 *
 *		ritorna un codice d'errore
 */
static
int
readspool_first (spooldir, list)
	const char			*spooldir;
	struct spoollist	*list;
{
	int					ret;
	struct spoollist	dirlist;
	time_t				subdir_date=0, list_date;
	static int 			has_refresh=0;
	int					i;

	if (!spooldir || !list) return RERR_PARAM;
	CF_MAY_READ;
	if (!spool_to_subdir) {
		return readspool_light (spooldir, list);
	}
	dirlist = SPOOL_LIST_NULL;
	*list = SPOOL_LIST_NULL;
	ret = getspooldirlist (&dirlist, spooldir);
	if (!RERR_ISOK(ret)) return ret;
	for (i=0; i<dirlist.listnum; i++) {
		ret = dirdatelist_search (&list_date, dirlist.list[i].cf_name);
		if (RERR_ISOK(ret)) {
			ret = get_subdirdate (&subdir_date, spooldir, dirlist.list[i].cf_name);
			if (RERR_ISOK(ret)) {
				if (subdir_date == list_date) {
					FRLOGF (LOG_VERB, "skip >>%s<< because timestamp has not "
											"changed", dirlist.list[i].cf_name);
					continue;
				}
			}
		}
		dirdatelist_insert (dirlist.list[i].cf_name, subdir_date);
		ret = readspool_light (dirlist.list[i].cf_name, list);
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_WARN, "error reading spooldir >>%s<<: %s",
									dirlist.list[i].cf_name, 
									rerr_getstr3(ret));
			continue;
		}
		if (list->listnum < 1) {
			hfree_spoollist (list);
			may_deletedir (spooldir, dirlist.list[i].cf_name);
			continue;
		}
		break;
	}
	hfree_spoollist (&dirlist);
	if (list->listnum < 1) {
		if (has_refresh) {
			has_refresh=0;
			return RERR_NOT_FOUND;
		}
		has_refresh=1;
		dirdatelist_freelist (dirdatelist);
		return readspool_first (spooldir, list);
	}
	return RERR_OK;
}


/* readspool_all:
 *		legge tutte le sottecartelle di spooldir
 *		in caso che spool_to_subdir==0 legge la cartella dello spool
 *
 *		spooldir:	nome della cartella dello spool
 *		list:			puntatore sul risultato
 *
 *		ritorna un codice d'errore
 */
static
int
readspool_all (spooldir, list)
	const char			*spooldir;
	struct spoollist	*list;
{
	CF_MAY_READ;
	if (spool_to_subdir) {
		return doreadspool_all (spooldir, list);
	} else {
		return readspool_light (spooldir, list);
	}
	return RERR_INTERNAL;
}


/* doreadspool_all:
 *		legge tutte le sottecartelle di spooldir
 *
 *		spooldir:	nome della cartella dello spool
 *		list:			puntatore sul risultato
 *
 *		ritorna un codice d'errore
 */
static
int
doreadspool_all (spooldir, list)
	const char			*spooldir;
	struct spoollist	*list;
{
	struct spoollist	dirlist;
	int					ret, i;
	char					*errdirname;
	struct stat			st;

	if (!spooldir || !list) return RERR_PARAM;
	dirlist = SPOOL_LIST_NULL;
	ret = getspooldirlist (&dirlist, spooldir);
	if (!RERR_ISOK(ret)) return ret;
	for (i=0; i<dirlist.listnum; i++) {
		ret = readspool_light (dirlist.list[i].cf_name, list);
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_WARN, "error reading dir >>%s<<: %s",
									dirlist.list[i].cf_name, 
									rerr_getstr3(ret));
			continue;
		}
	}
	hfree_spoollist (&dirlist);
	ret = mkfilename2 (&errdirname, spooldir, "error", NULL, FLAG_MKFILE_NOSUB);
	if ((RERR_ISOK(ret)) && stat (errdirname, &st)==0) {
		ret = readspool_light (errdirname, list);
		if (!RERR_ISOK(ret) && ret != RERR_SYSTEM) {
			FRLOGF (LOG_WARN, "error reading dir >>%s<<: %s", errdirname,
									rerr_getstr3(ret));
		}
		free (errdirname);
	}
	return RERR_OK;
}



/* cache_getspoollist:
 *		ritorna contenuto dello spool
 *		se e` stato letto gia` in passato ritorna cache
 *		altrimenti lo leggi
 *
 *		list:		puntatore al risultato
 *		spool:	nome dello spool (non neccessariamente la cartella)
 *
 *		ritorna un codice d'errore
 */
static
int
cache_getspoollist (list, spool)
	struct spoollist	**list;
	const char			*spool;
{
	static struct spoollist	cache_spoollist = SPOOL_LIST_NULL;
	static int					cache_hasspoollist = 0;
	static char					*cache_spool = NULL;
	static tmo_t				cache_last = 0;
	static tmo_t				lastdirdaterefresch = 0;
	tmo_t							now;
	int							ret;

	if (!list || !spool) return RERR_PARAM;
	set_spoolwhat (spool);
	CF_MAY_READ;
	now = tmo_now ();
	/* update dirdatelist at least every two hours */
	if (now - lastdirdaterefresch > 7200000000LL) {
		dirdatelist_freelist (dirdatelist);
		lastdirdaterefresch = now;
	}
	if (cache_hasspoollist) {
		if (!cache_spool || strcmp (spool, cache_spool) != 0) {
			cache_hasspoollist = 0;
		}
		if (cache_maxtime==0) {
			cache_hasspoollist=0;
		} else if (cache_maxtime>0) {
			if (now - cache_last > cache_maxtime) cache_hasspoollist = 0;
		}
		if (cache_spoollist.next_to_elab >= cache_spoollist.listnum) {
			cache_hasspoollist = 0;
			if (spool_to_subdir) {
				may_deletedir (	cache_spoollist.spooldir, 
								cache_spoollist.subdir);
			}
		}
		if (!cache_hasspoollist) {
#if 0
			/* we need a refresh - so delete dirdatelist */
			dirdatelist_freelist (dirdatelist);
			lastdirdaterefresch = now;
#endif
			hfree_spoollist (&cache_spoollist);
			cache_spoollist = SPOOL_LIST_NULL;
			if (cache_spool) free (cache_spool);
			cache_spool = NULL;
		} else {
			*list = &cache_spoollist;
			FRLOGF (LOG_VERB, "we have spoollist for spool=>>%s<< in cache",
									spool);
			return RERR_OK;
		}
	}
	if (use_light_spooler) {
		FRLOGF (LOG_VVERB, "calling spool_list_light()");
		ret = spool_list_light (spool, &cache_spoollist);
	} else {
		FRLOGF (LOG_VVERB, "calling spool_list()");
		ret = spool_list (spool, &cache_spoollist, SPOOL_FLAG_PRT_WAIT |
														 SPOOL_FLAG_SORT_MTIME);
	}
	if (!RERR_ISOK(ret)) {
		if (ret != RERR_NOT_FOUND) {
			FRLOGF (LOG_ERR, "error getting spool_list%s for spool=>>%s<<: %s",
									use_light_spooler?"(_light)":"", spool,
									rerr_getstr3 (ret));
		}
		return ret;
	}
	cache_spool = strdup (spool);
	cache_last = now;
	cache_hasspoollist = 1;
	*list = &cache_spoollist;
	FRLOGF (LOG_VVERB, "successfully got spoollist");
	return RERR_OK;
}


/* dospool_chstate:
 *		cambia lo stato nel cf_name
 *
 *		spool:		cartella dello spool
 *		cf_name:		nome del file cf nello spool
 *		newstate:	nuovo stato
 *		error:		codice d'errore (usato solo se newstate e` SPOOL_STATE_ERROR
 *		options:		opzioni da settare, se NULL, non cambia
 *
 *		ritorna un codice d'errore
 */
static
int
dospool_chstate (spooldir, cf_name, newstate, error, options)
	const char	*spooldir, *cf_name, *options;
	int			newstate, error;
{
	struct spool_entry	entry;
	int						ret, oldstate;
	char						*newdir, *olddir, *cf_file, *df_file;
	const char				*s;
	int						domove, num;
	char						subdir[64];

	if (!spooldir || !cf_name) return RERR_PARAM;
	if (newstate < 0 || newstate > SPOOL_STATE_MAX) return RERR_PARAM;
	entry = SPOOL_ENTRY_NULL;
	ret = read_spoolentry (&entry, spooldir, cf_name);
	if (!RERR_ISOK(ret)) {
		return ret;
	}
	oldstate = entry.state;
	entry.state = newstate;
	if (newstate == SPOOL_STATE_ERROR) {
		entry.error = error;
	} else {
		entry.error = 0;
	}
	if (options) {
		if (entry.options) free (entry.options);
		entry.options = strdup (options);
	}
	ret = write_spoolentry2 (&entry, spooldir, FLAG_WRSPOOLENTRY_MAYERROR);
	if (!RERR_ISOK(ret)) {
		hfree_spoolentry (&entry);
		return ret;
	}
	if (oldstate != newstate) {
		domove=0;
		newdir=NULL;
		olddir=NULL;
		cf_file = NULL;
		df_file = NULL;
		ret = RERR_INTERNAL;
		if (newstate == SPOOL_STATE_ERROR) {
			ret = mkfilename2 (&newdir, spooldir, "error", NULL, FLAG_MKFILE_NOSUB);
			if (!RERR_ISOK(ret)) goto error;
			ret = mkfilename (&cf_file, spooldir, cf_name, NULL);
			if (!RERR_ISOK(ret)) goto error;
			ret = mkfilename (&df_file, spooldir, entry.df_name, NULL);
			if (!RERR_ISOK(ret)) goto error;
			domove=1;
		} else if (oldstate == SPOOL_STATE_ERROR) {
			domove=1;
			s=(s=rindex (cf_name, '/'))?s+1:cf_name;
			s=index (s, '_');
			if (!s) goto error;
			for (num=0, s++; isdigit(*s); s++) { num*=10; num+=*s-'0'; }
			ret = getsubdir (subdir, spooldir, num, SUBDIR_NAMEONLY);
			if (!RERR_ISOK(ret)) goto error;
			ret = mkfilename2 (&newdir, spooldir, subdir, NULL, FLAG_MKFILE_NOSUB);
			if (!RERR_ISOK(ret)) goto error;
			ret = mkerrorfilename (&cf_file, spooldir, cf_name);
			if (!RERR_ISOK(ret)) goto error;
			if (!entry.df_name || !*entry.df_name) {
				df_file=NULL;
			} else {
				ret = mkerrorfilename (&df_file, spooldir, entry.df_name);
				if (!RERR_ISOK(ret)) goto error;
			}
		}
		if (domove) {
			ret = move_cfdf (newdir, cf_file, df_file);
		} else {
			ret = RERR_OK;
		}
error:
		if (newdir) free (newdir);
		if (olddir) free (olddir);
		if (cf_file) free (cf_file);
		if (df_file) free (df_file);
		if (!RERR_ISOK(ret)) {
			hfree_spoolentry (&entry);
			return ret;
		}
	}
	hfree_spoolentry (&entry);
	return RERR_OK;
}



/* move_cfdf:
 *		sposta i file cf_file e df_file nella cartella newdir
 *		o entrambi saranno spostato o nessuno - mai uno solo
 *
 *		newdir:	cartella destinaria
 *		cf_file:	nome del primo file da spostare
 *		df_file:	nome del secondo file da spostare
 *
 *		ritorna un codice d'errore
 */
static
int
move_cfdf (newdir, cf_file, df_file)
	const char	*newdir, *cf_file, *df_file;
{
	int			len, len2, n=0, ret;
	const char	*s, *s2;
	char			*newfile, *s3;

	if (!newdir || !cf_file) return RERR_PARAM;
	if ((ret = maymkdir (newdir)) != RERR_OK) {
		FRLOGF (LOG_ERR, "error creating spooldir >>%s<<: %s", newdir,
								rerr_getstr3(ret));
		return ret;
	}
	s = (s = rindex (cf_file, '/'))?s+1:cf_file;
	len = strlen (s);
	if (df_file) {
		s2 = (s2 = rindex (df_file, '/'))?s2+1:df_file;
		len2 = strlen (df_file);
		if (len2 > len) len=len2;
	}
	len += strlen (newdir) + 2;
	newfile = malloc (len);
	if (!newfile) return RERR_NOMEM;
	sprintf (newfile, "%s/%n%s", newdir, &n, s);
	s3 = newfile+n;
	if (link (cf_file, newfile) < 0) {
		FRLOGF (LOG_ERR, "error moving cf_file >>%s -> %s<<: %s", cf_file,
								newfile, rerr_getstr3(RERR_SYSTEM));
		free (newfile);
		return RERR_SYSTEM;
	}
	if (df_file) {
		strcpy (s3, s2);
		if (link (df_file, newfile) < 0) {
			FRLOGF (LOG_ERR, "error movinf cf_file >>%s -> %s<<: %s", cf_file,
								newfile, rerr_getstr3(RERR_SYSTEM));
			/* move back cf_file - to maintain consistency */
			strcpy (s3, s);
			unlink (newfile);
			free (newfile);
			return RERR_SYSTEM;
		}
	}
	/* now unlink oldfiles */
	unlink (cf_file);
	if (df_file) unlink (df_file);
	free (newfile);
	return RERR_OK;
}


/* del_emptyentries:
 *		cancella tutti gli elementi vuoti nella lista
 *
 *		list:	lista da elaborare
 *
 *		ritorna un codice d'errore
 */
static
int
del_emptyentries (list)
	struct spoollist	*list;
{
	int	i;

	if (!list) return RERR_PARAM;
	for (i=0; i<list->listnum; i++) {
		if (!list->list[i].cf_name) {
			if (del_spoollistentry (list, i) == RERR_OK) i--;
		}
	}
	return RERR_OK;
}



/* may_deletedir:
 *		controlla se la cartella indicata di spooldir/subdir e` possibile
 *		da cancellare (se vuoto ed il contatore e` abastanza grande, cosi`
 *		che e` poco probabile che altri file vengono salvati li dentro)
 *		e` se possibile lo cancella
 *
 *		spooldir:	cartella dello spool
 *		subdir:		sottocartella
 *
 *		ritorna un codice d'errore
 */
static
int
may_deletedir (spooldir, subdir)
	const char	*subdir, *spooldir;
{
	char				*sdname, *fname, *s;
	const char		*sc;
	int				ret, runnum, dirnum;
	DIR				*d;
	struct dirent	*dent;

	if (!subdir || !spooldir) return RERR_PARAM;
	runnum = get_runnum (spooldir, FLAG_RUNNUM_NOINC);
	if (runnum<0) return runnum;
	sc = (s = rindex (subdir, '/'))?s+1:subdir;
	sc = index (sc, '_');
	if (sc) {
		dirnum = cf_atoi (++sc);
		if (dirnum + num_files_per_subdir > runnum) {
			/* other files will be written to that dir, so don't delete it */
			return RERR_OK;
		}
	}
	ret = mkfilename2 (&sdname, spooldir, subdir, NULL, FLAG_MKFILE_NOSUB);
	if (!RERR_ISOK(ret)) return ret;
	s = (s = rindex (sdname, '/'))?s+1:sdname;
	*s = 'l';
	ret = dolock_file (sdname, lockfile_timeout);
	if (!RERR_ISOK(ret)) return ret;
	*s = 's';
	if (rmdir (sdname) == 0) {
		*s='l';
		unlink (sdname);
		free (sdname);
		dirdatelist_remove (subdir);
		return RERR_OK;
	} else if (errno != ENOTEMPTY) {
		FRLOGF (LOG_ERR, "error removing dir >>%s<<: %s", sdname,
								rerr_getstr3(RERR_SYSTEM));
		*s='l';
		unlink (sdname);
		free (sdname);
		return RERR_SYSTEM;
	}
	d = opendir (sdname);
	while ((dent=readdir (d))) {
		if (!strncmp (dent->d_name, "cf_", 3) || 
					!strncmp (dent->d_name, "df_", 3)) {
			/* dir is not empty - cannot delete it */
			*s='l';
			unlink (sdname);
			free (sdname);
			closedir (d);
			return RERR_OK;
		}
	}
	closedir (d);
	/* ok - only rubbish inside - remove it */
	d = opendir (sdname);
	while ((dent=readdir (d))) {
		if (!strcmp (dent->d_name, ".") || 
					!strcmp (dent->d_name, "..")) {
			continue;
		}
		ret = mkfilename2 (&fname, sdname, dent->d_name, NULL, FLAG_MKFILE_NOSUB);
		if (!RERR_ISOK(ret)) continue;
		unlink (fname);
		free (fname);
	}
	closedir (d);
	if (rmdir (sdname) < 0) {
		FRLOGF (LOG_ERR, "error removing dir >>%s<<: %s", sdname,
								rerr_getstr3(RERR_SYSTEM));
		*s='l';
		unlink (sdname);
		free (sdname);
		return RERR_SYSTEM;
	}
	*s='l';
	unlink (sdname);
	free (sdname);
	dirdatelist_remove (subdir);
	return RERR_OK;
}




/* get_subdirdate:
 *		legge nella cartella indicata di spooldir/subdir il file "date" e
 *		salva il contenuto nello parametro date
 *
 *		date:			puntatore ad un variabile per il risultato
 *		spooldir:	cartella dello spool
 *		subdir:		sottocartella
 *
 *		ritorna un codice d'errore
 */
static
int
get_subdirdate (date, spooldir, subdir)
	const char		*spooldir, *subdir;
	time_t			*date;
{
	char	*sdname, *dfname, *s;
	int	ret;
	long	ldate;
	FILE	*f;

	if (!spooldir || !subdir || !date) return RERR_PARAM;
	ret = mkfilename2 (&sdname, spooldir, subdir, NULL, FLAG_MKFILE_NOSUB);
	if (!RERR_ISOK(ret)) return ret;
	ret = mkfilename2 (&dfname, sdname, "date.lock", NULL, FLAG_MKFILE_NOSUB);
	free (sdname);
	if (!RERR_ISOK(ret)) return ret;
	s = rindex (dfname, '.');
	ret = dolock_file (dfname, lockfile_timeout);
	if (!RERR_ISOK(ret)) {
		free (dfname);
		return ret;
	}
	*s=0;
	f = fopen (dfname, "r");
	if (!f) {
		f = fopen (dfname, "w");
		if (!f) {
			FRLOGF (LOG_ERR, "error opening file >>%s<< for writing: %s", dfname,
									rerr_getstr3(RERR_SYSTEM));
			*s='.';
			unlink (dfname);
			free (dfname);
			return RERR_SYSTEM;
		}
		ldate = time (NULL);
		fprintf (f, "%ld", ldate);
	} else {
		fscanf (f, "%ld", &ldate);
	}
	fclose (f);
	*date = (time_t) ldate;
	*s='.';
	unlink (dfname);
	free (dfname);
	return RERR_OK;
}


/* write_subdirdate:
 *		scrive nella cartella indicata da spooldir/subdir un file che si
 *		chiama date e contienne la data/tempo (in secondi da 1970) attuale
 *
 *		spooldir:	cartella dello spool
 *		subdir:		sottocartella
 *
 *		ritorna un codice d'errore
 */
static
int
write_subdirdate (spooldir, subdir)
	const char	*spooldir, *subdir;
{
	char	*sdname, *dfname, *s;
	int	ret;
	long	ldate;
	FILE	*f;

	if (!spooldir || !subdir) return RERR_PARAM;
	ret = mkfilename2 (&sdname, spooldir, subdir, NULL, FLAG_MKFILE_NOSUB);
	if (!RERR_ISOK(ret)) return ret;
	ret = mkfilename2 (&dfname, sdname, "date.lock", NULL, FLAG_MKFILE_NOSUB);
	free (sdname);
	if (!RERR_ISOK(ret)) return ret;
	s = rindex (dfname, '.');
	ret = dolock_file (dfname, lockfile_timeout);
	if (!RERR_ISOK(ret)) {
		free (dfname);
		return ret;
	}
	*s=0;
	f = fopen (dfname, "w");
	if (!f) {
		FRLOGF (LOG_ERR, "error opening file >>%s<< for writing: %s", dfname,
								rerr_getstr3(RERR_SYSTEM));
		*s='.';
		unlink (dfname);
		free (dfname);
		return RERR_SYSTEM;
	}
	ldate = time (NULL);
	fprintf (f, "%ld", ldate);
	fclose (f);
	*s='.';
	unlink (dfname);
	free (dfname);
	return RERR_OK;
}



/* dirdatelist_insert:
 *		crea ed inserisce un nuovo elemento con la chiave dir ed il valore
 *		date. se l'elemento gia` essiste la data vienne aggiornata
 *
 *		dir:	chiave del elemento da inserire
 *		date:	la data da inserire
 *
 *		ritorna un codice d'errore
 */
static
int
dirdatelist_insert (dir, date)
	const char		*dir;
	time_t			date;
{
	struct dirdatelist	*element;
	int						ret;

	if (!dir) return RERR_PARAM;
	element = dirdatelist_search_rec (dirdatelist, dir);
	if (element) {
		element->date = date;
		return RERR_OK;
	}
	element = malloc (sizeof (struct dirdatelist));
	if (!element) return RERR_NOMEM;
	*element = DIRDATELIST_NULL;
	element->dir = strdup (dir);
	if (!element->dir) {
		free (element);
		return RERR_NOMEM;
	}
	element->date = date;
	if (!dirdatelist) {
		dirdatelist = element;
	} else {
		ret = dirdatelist_insert_rec (dirdatelist, element);
		if (!RERR_ISOK(ret)) {
			free (element->dir);
			free (element);
			return ret;
		}
	}
	return RERR_OK;
}

/* dirdatelist_search:
 *		cerca l'elemento indicato con dir, prende la data e lo salva
 *		in date
 *
 *		date:	puntatore a un intero (time_t) in quale salvare il risultato
 *		dir:	chiave da cercare
 *
 *		ritorna un codice d'errore
 */
static
int
dirdatelist_search (date, dir)
	time_t		*date;
	const char	*dir;
{
	struct dirdatelist	*element;

	if (!date || !dir) return RERR_PARAM;
	element = dirdatelist_search_rec (dirdatelist, dir);
	if (!element) return RERR_NOT_FOUND;
	*date = element->date;
	return RERR_OK;
}


/* dirdatelist_remove:
 *		rimuove l'elemento che contienne dir del albero globale
 *
 *		dir:	chiave da cercare e rimuovere
 *
 *		ritorna un codice d'errore
 */
static
int
dirdatelist_remove (dir)
	const char	*dir;
{
	struct dirdatelist	*element, *father;
	int						ret;

	if (!dir) return RERR_PARAM;
	element = dirdatelist_search_rec (dirdatelist, dir);
	if (!element) return RERR_NOT_FOUND;
	if (element == dirdatelist) {
		if (element->left) {
			dirdatelist=element->left;
			dirdatelist->father = NULL;
			if (element->right) {
				ret = dirdatelist_insert_rec (dirdatelist, element->right);
				if (!RERR_ISOK(ret)) {
					dirdatelist_freelist (element->right);
					if (element->dir) free (element->dir);
					*element = DIRDATELIST_NULL;
					return ret;
				}
			}
		} else {
			dirdatelist=element->right;
			if (dirdatelist) dirdatelist->father = NULL;
		}
	} else {
		father = element->father;
		if (element->left) {
			if (father->left == element) {
				father->left = element->left;
			} else {
				father->right = element->left;
			}
			element->left->father = father;
			if (element->right) {
				ret = dirdatelist_insert_rec (father, element->right);
				if (!RERR_ISOK(ret)) {
					dirdatelist_freelist (element->right);
					if (element->dir) free (element->dir);
					*element = DIRDATELIST_NULL;
					return ret;
				}
			}
		} else {
			if (father->left == element) {
				father->left = element->right;
			} else {
				father->right = element->right;
			}
			if (element->right) element->right->father = father;
		}
	}
	if (element->dir) free (element->dir);
	*element = DIRDATELIST_NULL;
	free (element);
	return RERR_OK;
}

/* dirdatelist_insert_rec:
 *		cerca recursivamente nel albero che inizia con head la posizione
 *		dove inserire l'element. poi lo inserisce.
 *
 *		head:    radice del albero in quale da inserire
 *		element: elemento da inserire
 *
 *		ritorna un codice di errore
 */
static
int
dirdatelist_insert_rec (head, element)
	struct dirdatelist	*head, *element;
{
	int	cmp;

	if (!head || !element) return RERR_PARAM;
	if (!head->dir || !element->dir) return RERR_PARAM;
	cmp = strcmp (element->dir, head->dir);
	if (cmp==0) return RERR_NOT_FOUND;
	if (cmp<0) {
		if (head->left) return dirdatelist_insert_rec (head->left, element);
		head->left = element;
		element->father = head;
		return RERR_OK;
	}
	if (head->right) return dirdatelist_insert_rec (head->right, element);
	head->right = element;
	element->father = head;
	return RERR_OK;
}


/* dirdatelist_search_rec: 
 *		cerca recursivamente nel albero che inizia con head 
 *      l'elemento che contienne dir
 *
 *		head:	radice del albero in quale da cercare
 *		dir:	chiave da cercare
 *
 *		ritorna l'elemento cercato o NULL se non lo trova
 */
static
struct dirdatelist*
dirdatelist_search_rec (head, dir)
	struct dirdatelist	*head;
	const char				*dir;
{
	int	cmp;

	if (!dir || !head || !head->dir) return NULL;
	cmp = strcmp (dir, head->dir);
	if (cmp==0) return head;
	if (cmp<0) return dirdatelist_search_rec (head->left, dir);
	return dirdatelist_search_rec (head->right, dir);
}

/* dirdatelist_freelist:
 *		cancella tutto il albero iniziando con head 
 *
 *		head:	albero da cancellare
 *
 *		ritorna un codice d'errore
 */
static
int
dirdatelist_freelist (head)
	struct dirdatelist	*head;
{
	if (!head) return RERR_PARAM;
	if (head == dirdatelist) dirdatelist=NULL;
	dirdatelist_freelist (head->left);
	dirdatelist_freelist (head->right);
	if (head->dir) free (head->dir);
	*head = DIRDATELIST_NULL;
	free (head);
	return RERR_OK;
}





/* read_config:
 *		legge i variabili neccessari (del file di config) e li salva in
 *		variabili globale (statichi)
 *
 *		ritorna sempre 0
 */
static
int
read_config ()
{
	cf_begin_read ();
	num_files_per_subdir = cf_atoi (cf_getarr2 ("spool_num_files_per_subdir", 
																spool_what, "1000"));
	spool_to_subdir = cf_isyes (cf_getarr2 ("spool_to_subdir", spool_what, 
														"yes"));
	use_light_spooler = cf_isyes (cf_getarr2 ("spool_use_fast_spooler", 
														spool_what, "yes"));
	if (spool_to_subdir) use_light_spooler = 1;
	cache_maxtime = cf_atoi (cf_getarr2 ("spool_cache_maxtime", spool_what, 
														"600"));
	if (cache_maxtime < 0 || cache_maxtime > 86400 /* one day */) {
		FRLOGF (LOG_NOTICE, "spool_cache_maxtime for spool >>%s<< is "
									"larger than one day (%d seconds), this is "
									"not a good idea, set it to one day (86400 "
									"seconds)", spool_what?spool_what:"<all>",
									cache_maxtime);
		cache_maxtime = 86400;
	}
	max_elabtime = cf_atotm (cf_getarr2 ("spool_max_elabtime", spool_what, 
												"604801")); /* one week and one second */
	reinsert_ontimeout = cf_isyes (cf_getarr2 ("spool_reinsert_ontimeout", 
														spool_what, "yes"));
	nfs_workaround = cf_isyes (cf_getarr2 ("spool_nfs_workaround", spool_what, 
														"no"));
	max_wait_for_lock = cf_atoi (cf_getarr2 ("spool_lock_maxwait", spool_what,
												"60"));
	default_spool = cf_getval2 ("spool_default", "frspool");
	base_spooldir = cf_getval2 ("spool_basedir", "/var/spool");
	lockfile_timeout = cf_atotm (cf_getarr2 ("spool_lockfile_timeout", 
														spool_what, "120"));
	config_read=1;
	cf_end_read_cb (&read_config);
	return RERR_OK;
}


static
int
set_spoolwhat (what)
	const char	*what;
{
	int	differ=0;

	if (!what) {
		if (spool_what) {
			free (spool_what);
			spool_what = NULL;
			differ = 1;
		} 
	} else if (!spool_what) {
		differ = 1;
	} else if (strcasecmp (what, spool_what) != 0) {
		free (spool_what);
		spool_what = NULL;
		differ = 1;
	}
	if (differ) {
		spool_what = strdup (what);
		if (!spool_what) return RERR_NOMEM;
		config_read = 0;
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
