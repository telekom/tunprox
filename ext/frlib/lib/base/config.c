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
#include <ctype.h>
#include <sys/types.h>
#include <errno.h>
#ifdef SunOS
extern int errno;
#endif
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


#include "config.h"
#include "errors.h"
#include "strcase.h"
#include "textop.h"
#include "fileop.h"
#include "prtf.h"
#include "tmo.h"
#include <fr/cal/tmo.h>


static char *config_file = NULL;
static char	*real_cfname = NULL;
static char	*DEF_CONFIG_FILE = NULL;
static int def_flags = 0;

static int config_read = 0;
static int config_reread = 0;
static int pseudo_lock = 0;
static int temp_no_read = 0;
static int no_callback = 0;

typedef struct {
	int32_t * tab;
	size_t  tabsize,
			bufsize;
} tab32_t;


#define IDXCHARS	"_-/.@:+()"
#define VALCHARS  "-/.@:+,"
#define isvarch(c)	((c) && (unsigned char)(c)<128 && \
							(c)>0 && (isalnum(c) || (c) == '_'))
#define isidxch(c)	((c) && (unsigned char)(c)<128 && \
							(c)>0 && (isalnum(c) || index (IDXCHARS, (c))))
#define isvarfch(c)	(isvarch(c) && !isdigit(c))
#define isvalch(c)	((c) && (isvarch(c) || index (VALCHARS, (c))))

static struct cf_ns	cf_ns_init = CF_NS_NULL;

static int32_t getnumstr (char *);
static int32_t getoctalstr (char *);
static int32_t gethexstr (char*);
static int isnumstr (const char *);
static int isoctalstr (const char *);
static int ishexstr (const char *);
static char * getstr (char **);
static int finish_tab (tab32_t*);
static int init_tab (tab32_t*);
static int insert_num_to_tab (int32_t, tab32_t*);
static int insert_str_to_tab (char*, tab32_t*, int);
static int insert_range_to_tab (int32_t, int32_t, tab32_t*);

static int exec_cmd (struct cf_ns*, char*, struct cf_cvlist*, int, int);
static int include_cf (struct cf_ns*, char*, struct cf_cvlist*, int, int);
static int undef_var (char*, struct cf_cvlist*, int, int);
static int insert_list_into_list (struct cf_cvlist*, struct cf_cvlist*, int);
static int insert_val_into_list (struct cf_cvlist*, int, const char*, const char*, char*, int, int);
static int get_next_entry (struct cf_ns*, char**, char**, char**, char**, int*);
static int get_val (struct cf_ns*, char**);
static int get_val2 (struct cf_ns*, char**, int*, int);
#if 0
static int need_more (char*);
#endif
static int get_end (char*, char, char**);
static int get_endsq (char*, char**);
static int split_first_line (char*, char**, char**, char**);
static int check_line (char*);
static int reset_line (char*, int);
static int get_line (char*, char**, int*);
static int free_bufs (struct cf_cvlist *);
static int cmp_cv (int, const char*, const char*, int, const char*, const char*);
static int varcmp (const char*, const char*);
static struct cf_cventry *search_pos (struct cf_cvlist*, int, const char*, const char*);
static struct cf_cventry *search_ppos (struct cf_cvlist*, int, const char*, const char*);
static struct cf_cventry *search_ppos_recidx (struct cf_cvlist*, int, const char*, const char*);
static struct cf_cventry *docf_getvarsingle (struct cf_ns*, int, char*, int);
static struct cf_cventry *docf_getarrsingle (struct cf_ns*, int, char*, char*);
static struct cf_cventry *docf_getarrwithpath (struct cf_ns*, int, char*, char*, int);
static char *docf_getarrnodef (struct cf_ns*, int, char*, char*);
static int docf_parse (struct cf_ns*, char*, struct cf_cvlist*);
static int docf_parse2 (struct cf_ns*, char*, struct cf_cvlist*, int, int);
static int insert_dir_into_list (struct cf_cvlist*, int, char*, int);
static int insert_path_into_list (struct cf_cvlist*, int, char*, const char*, char*, int);
static struct cf_cventry *docf_getpath (struct cf_ns*, int, char*);

static int read_config ();
static int ns_read_config (struct cf_ns*, const char*);
static int call_reread_funcs ();
static void cf_hup_reread_cb (int);

static char *findslash (char*, int);
static int docf_printvar (struct cf_cventry*, struct cf_cventry*);
static char *find_end_brace2 (char**);
static char *find_end_brace (char*);
static int parse_sub_list (struct cf_ns*, struct cf_cvlist*, int, char*, char*, int);
static int split_var (char**, int, char**, int*, const char*);
static int getdnum (struct cf_ns*, const char*);
static int get_def_cf ();
static char	*cf_fname = NULL;
static int open_cfile ();
static int setmyrcdir (struct cf_cvlist*);
static int setmyexedir (struct cf_cvlist*);
static int myxopen (char*);

#define MAY_READ_CONFIG(retval) do { \
		if (ns == &cf_ns_init && !config_read) { \
			int	ret = read_config(); \
			if (!RERR_ISOK (ret)) return retval; \
		} \
	} while (0)
		


int
cf_default_cfname (cf, flags)
	const char	*cf;
	int			flags;
{
	char			*s;
	const char	*sc;
	int			differ=0;

	if (cf && !*cf) cf=NULL;
	differ = cf && (!DEF_CONFIG_FILE || strcmp (cf, DEF_CONFIG_FILE) != 0 || \
			(flags & CF_F_USCORE) != (def_flags & CF_F_USCORE));
	if (differ) {
		if (DEF_CONFIG_FILE) {
			free (DEF_CONFIG_FILE);
			DEF_CONFIG_FILE=NULL;
		}
		if (cf) {
			sc = (s = rindex (cf, '/')) ? (s+1) : cf;
			if (!*sc) return RERR_INVALID_CF;
			DEF_CONFIG_FILE = strdup (sc);
			if (!DEF_CONFIG_FILE) return RERR_NOMEM;
			if (flags & CF_F_USCORE) {
				if ((s=index (DEF_CONFIG_FILE, '_')) && s>DEF_CONFIG_FILE) *s=0;
			}
		}
	}
	if (def_flags != flags) differ = 1;
	def_flags = flags;
	if (differ && DEF_CONFIG_FILE && !config_file) config_read = 0;
	return RERR_OK;
}

const char *
cf_get_defcfname ()
{
	if (!DEF_CONFIG_FILE) get_def_cf ();
	return DEF_CONFIG_FILE;
}

int 
cf_set_cfname (cf)
	const char	*cf;
{
	int		differ;

	if (cf && !*cf) return RERR_INVALID_CF;
	differ = (cf && !config_file) || (!cf && config_file) \
				|| (cf && config_file && strcmp (cf, config_file) != 0);
	if (!differ) return RERR_OK;
	if (config_file) {
		free (config_file);
		config_file = NULL;
	}
	if (real_cfname) {
		free (real_cfname);
		real_cfname = NULL;
	}
	if (cf && *cf) {
		config_file = strdup (cf);
		if (!config_file) return RERR_NOMEM;
	}
	fop_resolvepath (&real_cfname, config_file); /* don't check error */
	config_read = 0;
	return RERR_OK;
}

const char *
cf_get_cfname ()
{
	return config_file;
}

const char *
cf_get_real_cfname ()
{
	return real_cfname ? real_cfname : config_file;
}


const char *
cf_getval2 (var, defvalue)
	const char	*var, *defvalue;
{
	return cfn_getval2 (&cf_ns_init, var, defvalue);
}

const char *
cfn_getval2 (ns, var, defvalue)
	struct cf_ns	*ns;
	const char		*var, *defvalue;
{
	const char	*s;

	s = cfn_getval (ns, var);
	if (!s) s=(char*)defvalue;
	return s;
}


const char *
cf_getval (var)
	const char	*var;
{
	return cfn_getval (&cf_ns_init, var);
}

const char *
cfn_getval (ns, var)
	struct cf_ns	*ns;
	const char		*var;
{
	return cfn_getarr (ns, var, NULL);
}


const char*
cf_getarr2 (var, idx, defval)
	const char	*var, *idx, *defval;
{
	return cfn_getarr2 (&cf_ns_init, var, idx, defval);
}

const char*
cfn_getarr2 (ns, var, idx, defval)
	struct cf_ns	*ns;
	const char		*var, *idx, *defval;
{
	const char	*val;

	val = cf_getarr (var, idx);
	if (!val) val = (char*)defval;
	return val;
}


const char *
cf_getarr (var, idx)
	const char	*var, *idx;
{
	return cfn_getarr (&cf_ns_init, var, idx);
}

const char *
cfn_getarr (ns, var, idx)
	struct cf_ns	*ns;
	const char		*var, *idx;
{
	const char	*val;

	val = cfn_getarrnodef (ns, var, idx);
	if (!val && idx) val = cfn_getarrnodef (ns, var, NULL);
	return val;
}

const char*
cf_getarrnodef (var, idx)
	const char	*var, *idx;
{
	return cfn_getarrnodef (&cf_ns_init, var, idx);
}

const char*
cfn_getarrnodef (ns, var, idx)
	struct cf_ns	*ns;
	const char		*var, *idx;
{
	return docf_getarrnodef (ns, 0, (char*)var, (char*)idx);
}

static
char*
docf_getarrnodef (ns, father, var, idx)
	struct cf_ns	*ns;
	char				*var, *idx;
	int				father;
{
	struct cf_cventry	*p;

	if (!var || !*var || !ns) return NULL;
	if (findslash (var, 1)) {
		p = docf_getarrwithpath (ns, father, var, idx, 0);
	} else {
		p = docf_getarrsingle (ns, father, var, idx);
	}
	if (!p) return NULL;
	if (p->flags & CFENTRY_F_ISDIR) return NULL;
	return p->val;
}

static
struct cf_cventry*
docf_getarrwithpath (ns, father, var, idx, canbemod)
	struct cf_ns	*ns;
	int				father, canbemod;
	char				*var, *idx;
{
	char					*str, *s;
	char					*buf, _buf[128];
	struct cf_cventry	*p;

	if (!ns) return NULL;
	str = top_skipwhiteplus (var, "/");
	if (!str || !*str) return NULL;
	s = findslash (str, 1);
	if (!s) return docf_getarrsingle (ns, father, str, idx);
	if (!canbemod) {
		if ((s-str) < (ssize_t)sizeof (_buf)) {
			buf = _buf;
			strncpy (buf, str, (s-str));
			buf[s-str] = 0;
		} else {
			buf = malloc ((s-str)+1);
			if (!buf) return NULL;
			strncpy (buf, str, (s-str));
			buf[s-str] = 0;
		}
	} else {
		buf = str;
		*s = 0;
	}
	p = docf_getvarsingle (ns, father, buf, 1);
	if (canbemod) {
		*s = '/';
	} else if (buf != _buf) {
		free (buf);
	}
	s = top_skipwhiteplus (s+1, "/");
	if (!s || !*s) return p;
	if (!p) return NULL;
	if (!(p->flags & CFENTRY_F_ISDIR)) return NULL;
	return docf_getarrwithpath (ns, p->d_num, s, idx, canbemod);
}

static
struct cf_cventry*
docf_getarrsingle (ns, father, var, idx)
	struct cf_ns	*ns;
	int				father;
	char				*var, *idx;
{
	struct cf_cvlist	*list;

	if (!ns) return NULL;
	MAY_READ_CONFIG (NULL);
	if (!var || !*var) return NULL;
	list = ns->early_list ? ns->early_list : &ns->all_cvs;
	return search_ppos_recidx (list, father, var, idx);
}


static
struct cf_cventry*
docf_getpath (ns, father, var)
	struct cf_ns	*ns;
	int				father;
	char				*var;
{
	struct cf_cventry	*p;
	int					ret, needfree;
	char					_buf[128], *buf=_buf;
	char					*idx;

	if (!var || !*var || !ns) return NULL;
	MAY_READ_CONFIG (NULL);
	ret = split_var (&buf, sizeof (_buf), &idx, &needfree, var);
	if (!RERR_ISOK (ret)) return NULL;
	p = docf_getarrwithpath (ns, father, buf, idx, 1);
	if (needfree) free (buf);
	return p;
}


static
struct cf_cventry*
docf_getvarsingle (ns, father, var, canbemod)
	struct cf_ns	*ns;
	int				father, canbemod;
	char				*var;
{
	char					_buf[128], *buf, *s;
	char					*varstart, *varend, *idxstart, *idxend;
	char					vend, iend;
	int					len;
	struct cf_cventry	*p = NULL;

	if (!var || !*var || !ns) return NULL;
	s = index (var, '[');
	if (!s) return docf_getarrsingle (ns, father, var, NULL);
	if (!canbemod) {
		len = strlen (var);
		if (len < (ssize_t)sizeof (_buf)) {
			buf = _buf;
		} else {
			buf = malloc (len+1);
			if (!buf) return NULL;
		}
		strcpy (buf, var);
		s = buf + (s - var);
	} else {
		buf = var;
	}
	varstart = top_skipwhite (var);
	idxstart = top_skipwhite (s+1);
	for (s--; iswhite (*s) && s >= varstart; s--) {}; s++;
	varend = NULL;
	if (s<=varstart) goto end;
	varend = s;
	vend = *s;
	*s = 0;
	s = rindex (idxstart, ']');
	if (s) {
		for (s--; iswhite (*s) && s >= idxstart; s--) {}; s++;
		if (s<=idxstart) {
			idxstart = idxend = NULL;
		} else {
			idxend = s;
			iend = *s;
			*s = 0;
		}
	} else {
		idxend = NULL;
	}
	p = docf_getarrsingle (ns, father, varstart, idxstart);
end:
	if (canbemod) {
		if (varend) *varend = vend;
		if (idxend) *idxend = iend;
	} else if (buf != _buf) {
		free (buf);
	}
	return p;
}


const char *
cf_getarri2 (var, idx, defval)
	const char	*var, *defval;
	int			idx;
{
	return cfn_getarri2 (&cf_ns_init, var, idx, defval);
}

const char *
cfn_getarri2 (ns, var, idx, defval)
	struct cf_ns	*ns;
	const char		*var, *defval;
	int				idx;
{
	const char	*val;

	val = cfn_getarri (ns, var, idx);
	if (!val) val = (char*)defval;
	return val;
}


const char *
cf_getarri (var, idx)
	const char	*var;
	int			idx;
{
	return cfn_getarri (&cf_ns_init, var, idx);
}

const char *
cfn_getarri (ns, var, idx)
	struct cf_ns	*ns;
	const char		*var;
	int				idx;
{
	char	stridx[32];

	snprintf (stridx, sizeof(stridx)-1, "%d", idx);
	stridx[sizeof(stridx)-1] = 0;
	return cfn_getarr (ns, var, stridx);
}


int
cf_getnumarr (var)
	const char	*var;
{
	return cfn_getnumarr (&cf_ns_init, var);
}

int
cfn_getnumarr (ns, var)
	struct cf_ns	*ns;
	const char		*var;
{
	struct cf_cventry	*p;
	struct cf_cvlist	*list;
	int					num=0, hasdef=0, d_num, len;
	char					_buf[128], *buf=_buf, *s;

	if (!var || !*var) return 0;
	MAY_READ_CONFIG (0);
	list = ns->early_list ? ns->early_list : &ns->all_cvs;
	if (!list || !(list->list)) return 0;
	if ((s = rindex (var, '/'))) {
		len = strlen (var);
		if (len >= (ssize_t)sizeof (_buf)) {
			buf = malloc (len+1);
		}
		strcpy (buf, var);
		s = buf + (s-var);
		*s = 0;
		s++;
		d_num = getdnum (ns, buf);
		if (d_num < 0) return 0;
	} else {
		s = (char*)var;
		d_num = 0;
	}
	for (p=list->list; p->var; p++) {
		if (p->father != d_num) continue;
		if (!varcmp (p->var, s)) {
			if (p->idx) {
				num++;
			} else {
				hasdef = 1;
			}
		}
	}
	if (buf != _buf) free (buf);
	if (num==0 && hasdef) num=1;
	return num;
}


const char *
cfn_getarrx (ns, var, x)
	struct cf_ns	*ns;
	const char		*var;
	int				x;
{
	return cfn_getarrxwi (ns, var, x, NULL);
}

const char *
cf_getarrx (var, x)
	const char	*var;
	int			x;
{
	return cf_getarrxwi (var, x, NULL);
}


const char *
cf_getarrxwi (var, x, idx)
	const char	*var;
	int			x;
	const char	**idx;
{
	return cfn_getarrxwi (&cf_ns_init, var, x, idx);
}

const char *
cfn_getarrxwi (ns, var, x, idx)
	struct cf_ns	*ns;
	const char		*var;
	int				x;
	const char		**idx;
{
	struct cf_cventry	*p, *def=NULL;
	struct cf_cvlist	*list;
	int					num=0, d_num, len;
	char					_buf[128], *buf=_buf, *s;

	if (num<0 || !var || !*var || !ns) return NULL;
	MAY_READ_CONFIG (NULL);
	list = ns->early_list ? ns->early_list : &ns->all_cvs;
	if (!list || !(list->list)) return NULL;
	if ((s = rindex (var, '/'))) {
		len = strlen (var);
		if (len >= (ssize_t)sizeof (_buf)) {
			buf = malloc (len+1);
		}
		strcpy (buf, var);
		s = buf + (s-var);
		*s = 0;
		s++;
		d_num = getdnum (ns, buf);
		if (d_num < 0) return 0;
	} else {
		s = (char*)var;
		d_num = 0;
	}
	for (p=list->list; p->var; p++) {
		if (p->father != d_num) continue;
		if (!varcmp (p->var, s)) {
			if (p->idx) {
				if (num == x) {
					if (idx) *idx = p->idx;
					return p->val;
				}
				num++;
			} else {
				def = p;
			}
		}
	}
	if (def && x == 0) {
		if (idx) *idx = p->idx;
		return def->val;
	}
	return NULL;
}


static
struct cf_cventry *
search_ppos_recidx (list, father, var, idx)
	struct cf_cvlist	*list;
	const char			*var, *idx;
	int					father;
{
	struct cf_cventry	*p;
	char					_buf[128], *buf=_buf, *s;

	if (!list || !(list->list) || !var || !*var) return NULL;
	p = search_ppos (list, father, var, idx);
	if (p) return p;
	if (!idx || !*idx) return NULL;
	if (!(s=rindex (idx, ','))) return NULL;
	if (strlen (idx) > sizeof (_buf)-1) {
		buf = strdup (idx);
		if (!buf) return NULL;
	} else {
		strcpy (buf, idx);
	}
	s = buf + (s - idx);
	*s = 0;
	p = search_ppos_recidx (list, father, var, buf);
	if (buf != _buf) free (buf);
	return p;
}

static
struct cf_cventry *
search_ppos (list, father, var, idx)
	struct cf_cvlist	*list;
	const char			*var, *idx;
	int					father;
{
	struct cf_cventry	*p;

	p = search_pos (list, father, var, idx);
	if (!p) return NULL;
	if (cmp_cv (p->father, p->var, p->idx, father, var, idx)) return NULL;
	return p;
}


static
struct cf_cventry *
search_pos (list, father, var, idx)
	struct cf_cvlist	*list;
	const char			*var, *idx;
	int					father;
{
	struct cf_cventry	*p;
	int					cmp;

	if (!list || !list->list) return NULL;
	for (p=list->list; p->var; p++) {
		cmp = cmp_cv (p->father, p->var, p->idx, father, var, idx);
		if (cmp >= 0) return p;
	}
	return p;
}


static
int
cmp_cv (father1, var1, idx1, father2, var2, idx2)
	const char	*var1, *var2, *idx1, *idx2;
	int			father1, father2;
{
	int		cmp;
	int		num1, num2;

	if (father1 < father2) return -1;
	if (father1 > father2) return 1;
	if (!var1 && !var2) return 0;
	cmp = varcmp (var1, var2);
	if (cmp != 0) return cmp;
	if (!idx1 && idx2) return -1;
	if (!idx1 && !idx2) return 0;
	if (idx1 && !idx2) return 1;
	if (isnumstr (idx1) && isnumstr (idx2)) {
		num1 = atoi (idx1);
		num2 = atoi (idx2);
		if (num1 < num2) return -1;
		if (num1 == num2) return 0;
		return 1;
	}
	return strcasecmp (idx1, idx2);
}

static
int
varcmp (var1, var2)
	const char	*var1, *var2;
{
	int		begin=1;

	if (!var1 && !var2) return 0;
	if (!var1) return -1;
	if (!var2) return 1;
	for (; *var1 && *var2; var1++, var2++) {
		if (!begin && *var1 == '_') {
			for (; *var1 == '_'; var1++);
			if (!*var1) break;
		}
		if (!begin && *var2 == '_') {
			for (; *var2 == '_'; var2++);
			if (!*var1) break;
		}
		if (tolower (*var1) != tolower (*var2)) break;
		if (*var1 != '_') begin=0;
	}
	if (!begin) {
		for (; *var1 == '_'; var1++);
		for (; *var2 == '_'; var2++);
	}
	if (!*var1 && !*var2) return 0;
	if (!*var1) return -1;
	if (!*var2) return 1;
	if (tolower (*var1) < tolower (*var2)) return -1;
	return 1;
}


int
cf_register_cv_list (list)
	struct cf_cvlist	*list;
{
	return cfn_register_cv_list (&cf_ns_init, list);
}

int
cfn_register_cv_list (ns, list)
	struct cf_ns		*ns;
	struct cf_cvlist	*list;
{
	struct cf_cvlist	**p;

	if (!list || !ns) return RERR_PARAM;
	p = realloc (ns->cv_lists, (ns->num_cv_lists+1)*sizeof (struct cf_cvlist));
	if (!p) return RERR_NOMEM;
	ns->cv_lists = p;
	ns->cv_lists[ns->num_cv_lists] = list;
	ns->num_cv_lists++;
	return insert_list_into_list (&ns->all_cvs, list, 0);
}

int
cf_deregister_cv_list (list)
	struct cf_cvlist	*list;
{
	return cfn_deregister_cv_list (&cf_ns_init, list);
}

int
cfn_deregister_cv_list (ns, list)
	struct cf_ns		*ns;
	struct cf_cvlist	*list;
{
	int		i;

	if (!list || !ns) return RERR_NOT_FOUND;
	for (i=0; i<ns->num_cv_lists; i++) {
		if (ns->cv_lists[i] == list) {
			for (; i<ns->num_cv_lists-1; i++) {
				ns->cv_lists[i] = ns->cv_lists[i+1];
			}
			ns->cv_lists[i] = NULL;
			ns->num_cv_lists--;
			cf_hfree_cvlist (&ns->all_cvs, 0);
			for (i=0; i<ns->num_cv_lists; i++) {
				insert_list_into_list (&ns->all_cvs, ns->cv_lists[i], 0);
			}
			return RERR_OK;
		}
	}
	return RERR_NOT_FOUND;
}


int
cf_free_config ()
{
	cf_free();
	return 1;
}

void
cf_free ()
{
	cfn_free (&cf_ns_init);
}

void
cfn_free (ns)
	struct cf_ns	*ns;
{
	int		i;

	if (!ns) return;
	cf_hfree_cvlist (&ns->all_cvs, 0);
	if (ns->cv_lists) {
		for (i=0; i<ns->num_cv_lists; i++) {
			cf_hfree_cvlist (ns->cv_lists[i], 1);
			free (ns->cv_lists[i]);
		}
		free (ns->cv_lists);
		ns->cv_lists = NULL;
		ns->num_cv_lists = 0;
	}
	*ns = CF_NS_NULL;
}

int
cf_read ()
{
	return read_config ();
}

int
cfn_read (ns, fname)
	struct cf_ns	*ns;
	const char		*fname;
{
	if (!ns || !fname) return RERR_PARAM;
	return ns_read_config (ns, fname);
}

int
cf_mayread ()
{
	if (!config_read) return read_config ();
	return RERR_OK;
}

int
cf_reread ()
{
	config_reread = 1;
	config_read = 0;
	return RERR_OK;
}


static
int
open_cfile ()
{
	int			fd, ret;
	char			*envrc, *s;
	static char	cfile[1024];

	if (!config_file && !DEF_CONFIG_FILE) get_def_cf ();
	if (config_file) {
		cf_fname = config_file;
		fd = myxopen (config_file);
		if (fd < 0) return RERR_INVALID_CF;
		return fd;
	}
	if (!DEF_CONFIG_FILE) {
		ret = get_def_cf ();
		if (!RERR_ISOK(ret)) return ret;
		if (!DEF_CONFIG_FILE) return RERR_INVALID_CF;
	}
	cf_fname = cfile;
	cfile[sizeof(cfile)-1]=0;
	if (!(def_flags & CF_F_NOENV)) {
		snprintf (cfile, sizeof(cfile)-1, "%sRC", DEF_CONFIG_FILE);
		for (s=cfile; *s; s++) *s=toupper(*s);
		envrc = getenv (cfile);
		if (envrc && *envrc) {
			fd = myxopen (envrc);
			if (fd > 0) {
				cf_fname = envrc;
				return fd;
			}
			snprintf (cfile, sizeof(cfile)-1, "%s/%s.rc", envrc, DEF_CONFIG_FILE);
			fd = myxopen (cfile);
			if (fd > 0) return fd;
		}
	}
	if (!(def_flags & CF_F_NOLOCAL)) {
		snprintf (cfile, sizeof(cfile)-1, "./%s.rc", DEF_CONFIG_FILE);
		fd = myxopen (cfile);
		if (fd > 0) return fd;
		snprintf (cfile, sizeof(cfile)-1, "./%s.d/%s.rc", DEF_CONFIG_FILE,
						DEF_CONFIG_FILE);
		fd = myxopen (cfile);
		if (fd > 0) return fd;
		snprintf (cfile, sizeof(cfile)-1, "./.%src", DEF_CONFIG_FILE);
		fd = myxopen (cfile);
		if (fd > 0) return fd;
		snprintf (cfile, sizeof(cfile)-1, "./.%s.d/%s.rc", DEF_CONFIG_FILE,
						DEF_CONFIG_FILE);
		fd = myxopen (cfile);
		if (fd > 0) return fd;
	}
	if (!(def_flags & CF_F_NOHOME)) {
		snprintf (cfile, sizeof(cfile)-1, "%s/.cfg/%s.d/%s.rc", getenv("HOME"),
						DEF_CONFIG_FILE, DEF_CONFIG_FILE);
		fd = myxopen (cfile);
		if (fd > 0) return fd;
		snprintf (cfile, sizeof(cfile)-1, "%s/.cfg/%s/%s.rc", getenv("HOME"),
						DEF_CONFIG_FILE, DEF_CONFIG_FILE);
		fd = myxopen (cfile);
		if (fd > 0) return fd;
		snprintf (cfile, sizeof(cfile)-1, "%s/.cfg/%s.rc", getenv("HOME"),
						DEF_CONFIG_FILE);
		fd = myxopen (cfile);
		if (fd > 0) return fd;
		snprintf (cfile, sizeof(cfile)-1, "%s/.config/%s.d/%s.rc", getenv("HOME"),
						DEF_CONFIG_FILE, DEF_CONFIG_FILE);
		fd = myxopen (cfile);
		if (fd > 0) return fd;
		snprintf (cfile, sizeof(cfile)-1, "%s/.config/%s/%s.rc", getenv("HOME"),
						DEF_CONFIG_FILE, DEF_CONFIG_FILE);
		fd = myxopen (cfile);
		if (fd > 0) return fd;
		snprintf (cfile, sizeof(cfile)-1, "%s/.config/%s.rc", getenv("HOME"),
						DEF_CONFIG_FILE);
		fd = myxopen (cfile);
		if (fd > 0) return fd;
		snprintf (cfile, sizeof(cfile)-1, "%s/.%s.d/%s.rc", getenv("HOME"),
						DEF_CONFIG_FILE, DEF_CONFIG_FILE);
		fd = myxopen (cfile);
		if (fd > 0) return fd;
		snprintf (cfile, sizeof(cfile)-1, "%s/.%s/%s.rc", getenv("HOME"),
						DEF_CONFIG_FILE, DEF_CONFIG_FILE);
		fd = myxopen (cfile);
		if (fd > 0) return fd;
		snprintf (cfile, sizeof(cfile)-1, "%s/.%src", getenv("HOME"),
						DEF_CONFIG_FILE);
		fd = myxopen (cfile);
		if (fd > 0) return fd;
	}
	if (!(def_flags & CF_F_NOGLOBAL)) {
		snprintf (cfile, sizeof(cfile)-1, "/etc/%s.rc", DEF_CONFIG_FILE);
		fd = myxopen (cfile);
		if (fd > 0) return fd;
		snprintf (cfile, sizeof(cfile)-1, "/etc/%s.d/%s.rc", DEF_CONFIG_FILE, 
						DEF_CONFIG_FILE);
		fd = myxopen (cfile);
		if (fd > 0) return fd;
		snprintf (cfile, sizeof(cfile)-1, "/etc/%s/%s.rc", DEF_CONFIG_FILE, 
						DEF_CONFIG_FILE);
		fd = myxopen (cfile);
		if (fd > 0) return fd;
		snprintf (cfile, sizeof(cfile)-1, "/opt/%s/conf/%s.rc", DEF_CONFIG_FILE,
						DEF_CONFIG_FILE);
		fd = myxopen (cfile);
		if (fd > 0) return fd;
		snprintf (cfile, sizeof(cfile)-1, "/usr/local/opt/%s/conf/%s.rc",
						DEF_CONFIG_FILE, DEF_CONFIG_FILE);
		fd = myxopen (cfile);
		if (fd > 0) return fd;
		snprintf (cfile, sizeof(cfile)-1, "/usr/opt/%s/conf/%s.rc",
						DEF_CONFIG_FILE, DEF_CONFIG_FILE);
		fd = myxopen (cfile);
		if (fd > 0) return fd;
		snprintf (cfile, sizeof(cfile)-1, "/usr/local/%s/conf/%s.rc",
						DEF_CONFIG_FILE, DEF_CONFIG_FILE);
		fd = myxopen (cfile);
		if (fd > 0) return fd;
	}
	return RERR_INVALID_CF;
}


static
int
myxopen (fname)
	char	*fname;
{
	struct stat	buf;
	int			fd;

	if (!fname || !*fname) return RERR_PARAM;
	if (stat (fname, &buf) < 0) {
		if (errno == ENOENT) return RERR_INVALID_FILE;
		return RERR_SYSTEM;
	}
	if (!S_ISREG (buf.st_mode)) return RERR_INVALID_FILE;
	fd = open (fname, O_RDONLY);
	if (fd < 0) return RERR_SYSTEM;
	return fd;
}

int
cfn_read_buf (ns, buf)
	struct cf_ns	*ns;
	char				*buf;
{
	struct cf_cvlist	*list;
	int					ret, i;

	if (!ns || !buf) return RERR_PARAM;
	/* create cvlist */
	cfn_free (ns);
	list = malloc (sizeof (struct cf_cvlist));
	if (!list) return RERR_NOMEM;
	*list = CF_CVL_NULL;

	/* preset some variables */
	if (ns == &cf_ns_init) {
		setmyrcdir (list);		/* ignore errors */
		setmyexedir (list);		/* ignore errors */
	}

	/* parse config file */
	ret = docf_parse (ns, buf, list);
	if (!RERR_ISOK (ret)) return ret;
	ret = cfn_register_cv_list (ns, list);
	if (!RERR_ISOK (ret)) {
		for (i=0; i<list->numbufs; i++) {
			if (list->bufs[i] == buf) list->bufs[i]=NULL;
		}
		cf_hfree_cvlist (list, 1);
		return ret;
	}
	return RERR_OK;
}


static
int
read_config ()
{
	return ns_read_config (&cf_ns_init, NULL);
}

static
int
ns_read_config (ns, fname)
	struct cf_ns	*ns;
	const char		*fname;
{
	int					fd;
	char					*buf;
	int					ret;
	struct cf_cvlist	*list;

	if (ns == &cf_ns_init) {
		if (temp_no_read) return RERR_OK;
		if (pseudo_lock) return RERR_DELAYED;
		config_reread=0;
	}

	/* read in config file */
	if (fname) {
		fd = open (fname, O_RDONLY);
		if (fd == -1) fd = RERR_SYSTEM;
	} else {
		fd = open_cfile ();
	}
	if (fd < 0) return fd;
	buf = fop_read_fd (fd);
	close (fd);
	if (!buf) return RERR_NOMEM;

	/* create cvlist */
	cfn_free (ns);
	list = malloc (sizeof (struct cf_cvlist));
	if (!list) return RERR_NOMEM;
	*list = CF_CVL_NULL;

	/* preset some variables */
	if (ns == &cf_ns_init) {
		setmyrcdir (list);		/* ignore errors */
		setmyexedir (list);		/* ignore errors */
	}

	/* parse config file */
	ret = docf_parse (ns, buf, list);
	if (!RERR_ISOK (ret)) {
		free (buf);
		return ret;
	}
	ret = cfn_register_cv_list (ns, list);
	if (!RERR_ISOK (ret)) {
		cf_hfree_cvlist (list, 1);
		return ret;
	}

	/* check for reread */
	if (ns == &cf_ns_init) {
		config_read = 1;
		no_callback = cf_isyes (cf_getval2 ("config_no_reread", "no"));
		if (no_callback) return RERR_OK;
		ret = call_reread_funcs ();
		if (!RERR_ISOK (ret)) {
			/* cf_hfree_cvlist (list, 1); */
			return ret;
		}
		if (config_reread) {
			return read_config();
		}
	}

	return RERR_OK;
}

static
int
setmyrcdir (list)
	struct cf_cvlist	*list;
{
	char	*val, *s;
	int	ret;

	if (!list) return RERR_PARAM;
	if (cf_fname) {
		ret = fop_resolvepath (&val, cf_fname);
		if (!RERR_ISOK(ret)) {
			if (*cf_fname == '/') {
				/* try using cf_fname directly */
				val = strdup (cf_fname);
				if (!val) {
					/* return whatever fop_resolvepath returned */
					return ret;
				}
			} else {
				return ret;
			}
		}
	} else {
		val = strdup ("");
		if (!val) return RERR_NOMEM;
	}
	s = rindex (val, '/');
	if (s) {
		while (s>val && *s=='/') s--;
		s[1] = 0;
	}

	ret = insert_val_into_list (list, 0, "myrcdir", NULL, val, -1, CFENTRY_F_NEEDFREE);
	if (!RERR_ISOK(ret)) {
		free (val);
		return ret;
	}
	return RERR_OK;
}

static
int
setmyexedir (list)
	struct cf_cvlist	*list;
{
	char	*val, *s;
	int	ret, ret2=RERR_OK;

	if (!list) return RERR_PARAM;
	ret = fop_resolvepath (&val, "/proc/self/exe");
	if (!RERR_ISOK(ret)) return ret;
	s = strdup (val);
	if (!s) {
		free (val);
		return RERR_NOMEM;
	}
	ret = insert_val_into_list (list, 0, "myexe", NULL, val, -1, CFENTRY_F_NEEDFREE);
	if (!RERR_ISOK(ret)) {
		free (val);
		ret2 = ret;
	}
	val = s;
	s = strdup (val);
	if (s) {
		ret = insert_val_into_list (list, 0, "me", NULL, s, -1, CFENTRY_F_NEEDFREE);
		if (!RERR_ISOK(ret)) {
			free (s);
			ret2 = ret;
		}
	}
	s = rindex (val, '/');
	if (s) *s = 0;
	ret = insert_val_into_list (list, 0, "mydir", NULL, val, -1, CFENTRY_F_NEEDFREE);
	if (!RERR_ISOK(ret)) {
		free (val);
		ret2 = ret;
	}
	return ret2;
}
	

static
int
get_def_cf ()
{
	char	*buf;
	int	ret;

	if (DEF_CONFIG_FILE) return RERR_OK;
	buf = fop_read_fn ("/proc/self/cmdline");
	if (buf) {
		ret = cf_default_cfname (buf, CF_FLAG_DEF_USCORE);
		free (buf);
	} else {
		ret = cf_default_cfname ("frlib", 0);
	}
	return ret;
}


static
int
free_bufs (list)
	struct cf_cvlist	*list;
{
	int		i;

	if (!list) return 0;
	if (list->bufs) {
		for (i=0; i<list->numbufs; i++) {
			if (list->bufs[i]) free (list->bufs[i]);
			list->bufs[i]=NULL;
		}
		free (list->bufs);
	}
	list->bufs = NULL;
	list->numbufs = 0;
	return 1;
}


int
cf_hfree_cvlist (list, freebufs)
	struct cf_cvlist	*list;
	int					freebufs;
{
	int		i;

	if (!list) return 0;
	if (freebufs) free_bufs (list);
	for (i=0; i<list->listsize; i++) {
		if (list->list[i].flags & CFENTRY_F_NEEDFREE) {
			free (list->list[i].val);
			list->list[i].val = NULL;
		}
	}
	if (list->list) free (list->list);
	*list = CF_CVL_NULL;
	return 1;
}



static
int
get_line (line, next, hascr)
	char	*line, **next;
	int		*hascr;
{
	char	* s;

	if (!line || !next) return 0;
	*next = NULL;
	if (hascr) *hascr=0;
	s = index (line, '\n');
	if (!s) return 1;
	if (s > line && *(s-1) == '\r') {
		*(s-1) = 0;
		if (hascr) *hascr = 1;
	}
	*s = 0;
	*next = s+1;
	return 1;
}



static
int
reset_line (line, hascr)
	char	*line;
	int		hascr;
{
	char	*s;

	if (!line) return 0;
	for (s=line; *s; s++);
	if (s>line && hascr) {
		*s = '\r';
		s++;
	}
	*s = '\n';
	return 1;
}




static
int
check_line (line)
	char	* line;
{
	char	*s;

	if (!line) return 0;
	s=top_skipwhite (line);
	if (!*s || *s == '#') return 0;
	return 1;
}



static
int
split_first_line (line, variable, indx, value)
	char	*line, **variable,
			**indx, **value;
{
	char	*s, *str, *s2, *val, *var, *idx;
	int		num;

	if (!line || !variable || !indx || !value)
		return 0;
	*variable = *indx = *value = NULL;
	val = index (line, '=');
	if (val) {
		*val = 0;
		val++;
	}
	var = top_skipwhite (line);
	s = findslash (var, -1);
	if (!s) s = var;
	s = top_skipwhiteplus (s, "/");
	for (; isvarch(*s); s++);
	if (s==var || !isvarfch(*var)) return 0;
	*variable = var;
	if (!val) {
		return 1;
	}
	if (*s == '[') {
		idx = s+1;
	} else {
		idx = NULL;
	}
	*s = 0;
	val = top_skipwhite (val);
	*value = val;
	s = top_skipwhite (s+1);
	if (!*s) return 1;
	if (!idx) {
		if (*s != '[') {
			/* ignore it */
			return 1;
		}
		idx = s+1;
	}
	idx = top_skipwhiteplus (idx, ",");
	s = index (idx, ']');
	if (!s) return 0;
	*s = 0;
	str=idx;
	while (1) {
		for (s=str; isidxch(*s); s++);
		if (s==str && *s) return 0;
		*s = 0;
		s2=s+1;
		if (*str && isnumstr (str)) {
			num = atoi (str);
			s = str + sprintf (str, "%d", num);
			*s = 0;
		}
		s2=top_skipwhiteplus (s2, ",");
		if (!isidxch(*s2)) break;
		*s = ',';
		str=++s;
		for (;*s2; s++,s2++) *s=*s2;
		*s=0;
	}
	if (!*idx) return 1;	/* empty index is ignored */
	*indx = idx;
	return 1;
}




static
int
get_end (buf, ch, end)
	char	*buf, ch, **end;
{
	char	*s, *s2;

	if (!buf) return 1;
	s = buf-1;
	while ((s=index (s+1, ch))) {
		if (*(s-1) == '\\') {
			for (s2=s-1; s2>=buf && *s=='\\'; s2--);
			if (((s-s2)-1)%2) continue;
		}
		break;
	}
	if (s && end) *end = s;
	return (s!=NULL);
}

static
int
get_endsq (buf, end)
	char	*buf, **end;
{
	char	*s;

	if (!buf) return 1;
	s = buf-1;
	while ((s=index (s+1, '\''))) {
		if (s[1] == '\'') {
			s++;
			continue;
		}
		break;
	}
	if (s && end) *end = s;
	return (s!=NULL);
}



#if 0
static
int
need_more (val)
	char	*val;
{
	char	*s;

	if (!val) return 0;
	s = top_skipwhite (val);
	if (*s != '{') return 0;
	return !get_end (val, '}', NULL);
}
#endif


static
int
get_val (ns, val)
	struct cf_ns	*ns;
	char				**val;
{
	return get_val2 (ns, val, NULL, 0);
}

static
int
get_val2 (ns, val, needfree, flags)
	struct cf_ns	*ns;
	char				**val;
	int				*needfree, flags;
{
	int	ret, dflags=0;
	char	*s;

	if (!val || !*val || !ns) return RERR_PARAM;
	if (needfree) *needfree = 0;
	*val = top_skipwhite (*val);
	switch (**val) {
	case '"':
		(*val)++;
		s = NULL;
		get_end (*val, '"', &s);
		if (s) *s = 0;
		ret = cfn_parse_string2 (ns, &s, *val, CFPS_T_DOUBLE | flags);
		if (!RERR_ISOK (ret)) return ret;
		if (needfree) *needfree = s != *val;
		*val = s;
		break;
	case '\'':
		(*val)++;
		s = NULL;
		get_endsq (*val, &s);
		if (s) *s = 0;
		cfn_parse_string (ns, *val, CFPS_T_SINGLE);
		break;
	case '`':
		(*val)++;
		break;
	case '$':
		(*val)++;
		if (**val != '{') {
			**val = 0;
			break;
		}
		if (flags & CFPS_F_DOLLAR)
			dflags=CFPS_F_DOLLAR;
		/* fall thru */
	case '{':
		s = NULL;
		// if (!get_end (*val, '}', &s)) return 0;
		s = find_end_brace (*val);
		if (s) *s = 0;
		(*val)++;
		ret = cfn_parse_string2 (ns, &s, *val, CFPS_T_BBRACE | dflags);
		if (!RERR_ISOK (ret)) return ret;
		if (needfree) *needfree = s != *val;
		*val = s;
		break;
	default:
		for (s=*val; isvalch(*s); s++);
		*s=0;
		break;
	}
	return RERR_OK;
}


static
char*
find_end_brace (val)
	char	*val;
{
	return find_end_brace2 (&val);
}

static
char*
find_end_brace2 (val)
	char	**val;
{
	char	*s;
	int	cnt = 1, precnt=0;
	int	isdouble, issingle, isbrak;
	
	if (!val || !*val) return NULL;
	(*val)++;
	s = *val;
	if (*s == '@') {
		s++;
		if (*s == '@') {
			*val = s;
		} else {
			for (; *s && *s != '@'; s++) {
				if (*s == '{') {
					precnt++;
				} else if (*s == '}') {
					precnt--;
				}
			}
			if (precnt > 0) {
				cnt+=precnt;
				precnt = 0;
			} else if (precnt < 0) {
				precnt *= -1;
			}
			if (*s == '@') *val = s+1;
		}
	}
	isdouble = issingle = isbrak = 0;
	for (s=*val; *s; s++) {
		if (*s == '\\' && s[1]) { 
			s++; 
		} else if (isdouble) {
			if (*s != '"') continue;
			isdouble=0;
		} else if (issingle) {
			if (*s != '\'') continue;
			issingle=0;
		} else if (isbrak) {
			if (*s != ']') continue;
			isbrak=0;
		} else if (*s == '"') {
			isdouble = 1;
		} else if (*s == '\'') {
			issingle = 1;
		} else if (*s == '[') {
			if (s[1] == ']') {
				s++;
			} else if (s[1] == '^' && s[2] == ']') {
				s+=2;
			}
			isbrak = 1;
		} else if (*s == '{') {
			if (precnt) {
				precnt--;
			} else {
				cnt++;
			}
		} else if (*s == '}') {
			cnt--;
			if (cnt <= 0) break;
		}
	}
	if (*s == '}') return s;
	return NULL;
}


static
int
get_next_entry (ns, buf, var, idx, val, flags)
	struct cf_ns	*ns;
	char				**buf, **var, **idx, **val;
	int				*flags;
{
	char	*s, *line, *br, *ebr;
	int	hascr, needfree, ret;

	if (!buf || !*buf || !var || !idx || !val || !flags || !ns) return 0;
	*flags = 0;
	line = *buf;
	if (!get_line (line, buf, &hascr)) return 0;
	if (!check_line (line)) 
		return get_next_entry (ns, buf, var, idx, val, flags);
	if (!split_first_line (line, var, idx, val))
		return get_next_entry (ns, buf, var, idx, val, flags);
	if (!*var)
		return get_next_entry (ns, buf, var, idx, val, flags);
#if 0
	if (!*val) {
		for (s=*var; s && *s && isvarch (*s); s++);
	} else {
		s = *val;
	}
	while (*buf && need_more (s)) {
		reset_line (s, hascr);
		if (!get_line (*buf, buf, &hascr)) break;
	}
#else
	if (!*val) {
		br = index (*var, '{');
	} else if (**val == '{') {
		br = *val;
	} else if (**val == '$' && (*val)[1] == '{') {
		br = *val; br++;
	} else {
		br = NULL;
	}
	if (br) {
		reset_line (br, hascr);
		ebr = find_end_brace (br);
		s = ebr ? index (ebr, '\n') : NULL;
		if (s) {
			*s = 0; s++;
		}
		*buf = s;
		if (ebr) ebr[1]=0;
	}
#endif
	if (*val) {
		ret = get_val2 (ns, val, &needfree, CFPS_F_DOLLAR);
		if (!RERR_ISOK (ret)) return 0;
		*flags = needfree ? CFENTRY_F_NEEDFREE : 0;
	}
	return 1;
}




static
int
insert_val_into_list (list, father, var, idx, val, d_num, flags)
	struct cf_cvlist	*list;
	const char			*var, *idx;
	char					*val;
	int					flags, father, d_num;
{
	struct cf_cventry	*p, *q;

	if (!list || !var) return RERR_PARAM;
	if (!(flags & CFENTRY_F_ISDIR) && !val) return RERR_PARAM;
	if (!list->list || list->listsize == 0) {
		list->list = calloc (2, sizeof (struct cf_cventry));
		if (!list->list) {
			free_bufs (list);
			return RERR_NOMEM;
		}
		list->listsize = 2;
		p=list->list;
		p->father = father;
		p->var = var;
		p->idx = idx;
		if (flags & CFENTRY_F_ISDIR) {
			p->d_num = d_num;
		} else {
			p->val = val;
		}
		p->flags = flags;
		return RERR_OK;
	}
	p = search_pos (list, father, var, idx);
	if (p->var && cmp_cv (p->father, p->var, p->idx, father, var, idx) == 0) {
		p->val = val;
		return RERR_OK;
	}
	list->listsize++;
	q = realloc (list->list, list->listsize * sizeof (struct cf_cventry));
	if (!q) return RERR_NOMEM;
	if (q != list->list) {
		p = q + (p - list->list);
		list->list = q;
	}
	q = list->list+list->listsize-1;
	bzero (q, sizeof (struct cf_cventry));
	if (p->var) {
		for (q=p+1; q->var; q++);
		for (; q!=p; q--) {
			q[0] = q[-1];
		}
	}
	p->father = father;
	p->var = var;
	p->idx = idx;
	if (flags & CFENTRY_F_ISDIR) {
		p->d_num = d_num;
	} else {
		p->val = val;
	}
	p->flags = flags;
	return RERR_OK;
}


static
int
insert_list_into_list (destlist, srclist, father)
	struct cf_cvlist	*destlist, *srclist;
	int					father;
{
	struct cf_cventry	*p, *q;
	int					ret;
	int					i;
	int					myfather, mydnum;
	char					*myval;
	int					*d_num_map=NULL;
	int					new_d_num;

	if (!srclist) return RERR_PARAM;
	d_num_map = malloc (sizeof (int) * (srclist->maxnum+1));
	if (!d_num_map) return RERR_NOMEM;
	bzero (d_num_map, sizeof(int) * (srclist->maxnum+1));
	d_num_map[0] = father;
	new_d_num = destlist->maxnum + 1;
	for (i=0; i<=srclist->maxnum; i++) {
		myfather = d_num_map[i];
		for (p=srclist->list; p && p->var; p++) {
			if (p->father != i) continue;
			if (p->flags & CFENTRY_F_ISDIR) {
				q = search_ppos (destlist, myfather, p->var, p->idx);
				if (q) {
					mydnum = q->d_num;
				} else {
					mydnum = new_d_num++;
				}
				d_num_map[p->d_num] = mydnum;
				myval = NULL;
			} else {
				myval = ((p->flags & CFENTRY_F_NEEDFREE) && p->val) ? 
								strdup (p->val) : p->val;
				mydnum = 0;
				q = NULL;
			}
			if (!q) {
				ret = insert_val_into_list (	destlist, myfather, p->var, 
														p->idx, myval, mydnum, p->flags);
				if (!RERR_ISOK (ret)) {
					if (myval && (p->flags & CFENTRY_F_NEEDFREE)) free (myval);
					return ret;
				}
			}
		}
	}
	if (d_num_map) free (d_num_map);
	if (srclist->numbufs) {
		destlist->bufs = realloc (	destlist->bufs, sizeof (char*) *
									(destlist->numbufs+srclist->numbufs));
		if (!destlist->bufs) return RERR_NOMEM;
		for (i=0; i<srclist->numbufs; i++) {
			destlist->bufs[destlist->numbufs+i] = srclist->bufs[i];
		}
		destlist->numbufs += srclist->numbufs;
		destlist->maxnum += srclist->maxnum+1;
	}
	return RERR_OK;
}



static
int
include_cf (ns, cmd, list, father, flags)
	struct cf_ns		*ns;
	char					*cmd;
	struct cf_cvlist	*list;
	int					father, flags;
{
	int					ret;
	char					*buf;
	FILE					*f;
	struct cf_cvlist	ilist=CF_CVL_NULL;

	if (!cmd || !list) return RERR_PARAM;
	if (strncasecmp (cmd, "include", 7))
		return RERR_PARAM;
	cmd = top_skipwhite (cmd+=7);
	ret = get_val (ns, &cmd);
	if (!RERR_ISOK (ret)) return ret;
	f = fopen (cmd, "r");
	if (!f) {
		//fprintf (stderr, "cf_parse::include_cf(): cannot open file >>%s<<: %s\n",
		//					cmd, strerror (errno));
		return RERR_SYSTEM;
	}
	buf = fop_read_file (f);
	fclose (f);
	if (!buf) {
		fprintf (stderr, "cf_parse::include_cf(): error reading include file %s\n",
							cmd);
		return RERR_SYSTEM;
	}
	ret = cfn_parse (ns, buf, &ilist);
	if (!RERR_ISOK (ret)) {
		free (buf);
		return ret;
	}
	ret = insert_list_into_list (list, &ilist, father);
	cf_hfree_cvlist (&ilist, 0);
	if (!RERR_ISOK (ret)) {
		free (buf);
		return ret;
	}

	return RERR_OK;
}


static
int
undef_var (cmd, list, father, flags)
	char					*cmd;
	struct cf_cvlist	*list;
	int					father, flags;
{
	char					*var, *idx, *s;
	struct cf_cventry	*p, *q;

	if (!cmd || !list) return RERR_PARAM;
	if (strncasecmp (cmd, "undef", 5))
		return RERR_SYSTEM;
	if (!strncasecmp (cmd, "undefine", 8)) {
		cmd+=8;
	} else {
		cmd += 5;
	}
	cmd = top_skipwhite (cmd);
	var = cmd;
	for (s=var; isvarch (*s); s++);
	if (*s == '[') {
		idx=s+1;
	} else {
		idx=NULL;
	}
	*s=0;
	if (!*var) return RERR_OK;
	if (!idx) {
		s = top_skipwhite (s+1);
		if (*s == '[') idx=s+1;
	}
	if (idx) {
		idx = top_skipwhite (idx);
		for (s=idx; isvarch(*s); s++);
		*s = 0;
	}
	if (!idx || *idx) {
		p = search_ppos (list, father, var, idx);
		if (!p) return RERR_OK;
		for (q=p; (q+1)->var; q++) {
			*q = *(q+1);
		}
		*q = *(q+1);
		return RERR_OK;
	}
	/* else delete whole array - what is with sublists !?! */
	for (p=list->list; p->var; p++) {
		if (!varcmp (p->var, var)) break;
	}
	if (!p->var) return RERR_OK;
	while (p->var && !varcmp (p->var, var)) {
		for (q=p; (q+1)->var; q++) {
			*q = *(q+1);
		}
		*q = *(q+1);
	}
	return RERR_OK;
}


static
int
exec_cmd (ns, cmd, list, father, flags)
	struct cf_ns		*ns;
	char					*cmd;
	struct cf_cvlist	*list;
	int					father, flags;
{
	char	*s, *br, *ebr;

	if (!cmd || !list || !ns) return RERR_PARAM;
	sswitch (cmd) {
	sincase ("include")
		return include_cf (ns, cmd, list, father, flags);
	sincase ("undef")
	sincase ("undefine")
		return undef_var (cmd, list, father, flags);
	} esac;
	br = index (cmd, '{');
	if (!br) {
		fprintf (stderr, "cf_parse::exec_cmd(): unknown command >>%s<< found "
								"in config file\n", cmd);
		return RERR_NOT_SUPPORTED;
	}
	for (s=br-1; s>=cmd && iswhite (*s); s--) {}; s++;
	ebr = find_end_brace2 (&br);
	if (ebr) *ebr = 0;
	*s = 0;
	return parse_sub_list (ns, list, father, cmd, br, flags);
}


static
int
parse_sub_list (ns, list, father, dir, subbuf, flags)
	struct cf_ns		*ns;
	struct cf_cvlist	*list;
	int					father, flags;
	char					*dir, *subbuf;
{
	int	ret;

	if (!list || father < 0 || !ns) return RERR_PARAM;
	if (dir && *dir) {
		ret = insert_dir_into_list (list, father, dir, flags);
		if (!RERR_ISOK(ret)) return ret;
		father = ret;
	}
	if (subbuf && *subbuf) {
		return docf_parse2 (ns, subbuf, list, father, flags);
	}
	return RERR_OK;
}


int
cf_parse (buf, list)
	char					*buf;
	struct cf_cvlist	*list;
{
	return cfn_parse (&cf_ns_init, buf, list);
}

int
cfn_parse (ns, buf, list)
	struct cf_ns		*ns;
	char					*buf;
	struct cf_cvlist	*list;
{
	if (!list || !ns) return RERR_PARAM;
	*list = CF_CVL_NULL;
	return docf_parse (ns, buf, list);
}

static
int
docf_parse (ns, buf, list)
	struct cf_ns		*ns;
	char					*buf;
	struct cf_cvlist	*list;
{
	int	ret;

	if (!buf || !list || !ns) return RERR_PARAM;
	list->bufs = malloc (sizeof (char*));
	if (!list->bufs) return RERR_NOMEM;
	list->bufs[0] = buf;
	list->numbufs=1;
	ns->early_list = list;	/* hack for dollar evaluation */
	ret = docf_parse2 (ns, buf, list, 0, 0);
	ns->early_list = NULL;
	return ret;
}

static
int
docf_parse2 (ns, buf, list, father, flags)
	struct cf_ns		*ns;
	char					*buf;
	struct cf_cvlist	*list;
	int					father, flags;
{
	int	ret, oflags=0;
	char	*var, *idx, *val;

	if (!buf || !list || !ns) return RERR_PARAM;
	while (1) {
		var = idx = val = NULL;
		if (!get_next_entry (ns, &buf, &var, &idx, &val, &oflags)) break;
		oflags |= flags;
		if (!val) {
			ret = exec_cmd (ns, var, list, father, oflags);
		} else {
			ret = insert_path_into_list (list, father, var, idx, val, oflags);
		}
		if (ret == RERR_NOMEM) {	/* ignore other errors */
			return ret;
		}
	}
	return RERR_OK;
}


static
int
insert_path_into_list (list, father, var, idx, val, flags)
	struct cf_cvlist	*list;
	int					father, flags;
	char					*var, *val;
	const char			*idx;
{
	char	*s, *s2;
	int	ret;

	var = top_skipwhiteplus (var, "/");
	if (!var || !*var) return RERR_PARAM;
	s = findslash (var, -1);
	if (!s) return insert_val_into_list (list, father, var, idx, val, -1, flags);
	for (s2=s-1; s2 >= var && (iswhite (*s2) || *s2=='/'); s2--) {}; s2++;
	*s2 = 0;
	s = top_skipwhiteplus (s+1, "/");
	ret = insert_dir_into_list (list, father, var, flags);
	if (!RERR_ISOK(ret)) return ret;
	father = ret;
	return insert_val_into_list (list, father, s, idx, val, -1, flags);
}

static
int
insert_dir_into_list (list, father, var, flags)
	struct cf_cvlist	*list;
	int					father, flags;
	char					*var;
{
	struct cf_cventry	*p;
	char					*s, *s2, *idx=NULL;
	int					ret, d_num;

	if (!list || !var || !*var) return RERR_PARAM;
	var = top_skipwhiteplus (var, "/");
	s = findslash (var, -1);
	if (s) {
		for (s2=s-1; s2 >= var && (iswhite (*s2) || *s2=='/'); s2--) {}; s2++;
		*s2 = 0;
		s = top_skipwhiteplus (s+1, "/");
		ret = insert_dir_into_list (list, father, var, flags);
		if (!RERR_ISOK(ret)) return ret;
		father = ret;
		var = s;
	}
	s = index (var, '[');
	if (s) {
		*s = 0;
		idx = s+1;
		s = index (idx, ']');
		if (s) *s=0;
	}
	var = top_stripwhite (var, TOP_F_STRIPMIDDLE);
	if (idx) idx = top_stripwhite (idx, TOP_F_STRIPMIDDLE);
	if (idx && *idx) {
		for (s=s2=idx; *s; s++) {
			if (iswhite (*s)) continue;
			*s2 = *s;
			s2++;
		}
		*s2 = 0;
	}
	if (idx && !*idx) idx=NULL;
	p = search_ppos (list, father, var, idx);
	if (p) {
		if (p->flags & CFENTRY_F_ISDIR) {
			return p->d_num;
		} else {
			return RERR_INVALID_PATH;
		}
	}
	d_num = list->maxnum+1;
	ret = insert_val_into_list (	list, father, var, idx, NULL, d_num, 
											flags | CFENTRY_F_ISDIR);
	if (!RERR_ISOK (ret)) return ret;
	list->maxnum++;
	return d_num;
}

#define gethex(c)	(range(c,'0','9')?((c)-'0'):(range(c,'A','F')?\
							((c)-'A'+10):(range(c,'a','f')?((c)-'a'+10):0)))
#define getoctal(c)	(isoctal(c)?((c)-'0'):0)


int
cf_find_endquote (endstr, str, type)
	char			**endstr;
	const char	*str;
	int			type;
{
	if (!str) return RERR_PARAM;
	type &= CFPS_TYPEMASK;
	switch (type) {
	case CFPS_T_DOUBLE:
	case CFPS_T_DOUBLEZERO:
		for (; *str; str++) {
			if (*str=='\\' && str[1]) {
				str++;
			} else if (*str=='"') {
				break;
			}
		}
		break;
	case CFPS_T_SINGLE:
		for (; *str; str++) {
			if (*str=='\\' && (str[1]=='\\' || str[1]=='\'')) {
				str++;
			} else if (*str=='\'') {
				break;
			}
		}
		break;
	case CFPS_T_BRACE:
		for (; *str; str++) {
			if (*str=='\\' && (str[1]=='\\' || str[1]=='{')) {
				str++;
			} else if (*str=='{') {
				break;
			}
		}
		break;
	case CFPS_T_BBRACE:
		str = find_end_brace ((char*)str);
		break;
	default:
		return RERR_PARAM;
	}
	if (!str || !*str) return RERR_NOT_FOUND;
	if (endstr) *endstr = (char*)str;
	return RERR_OK;
}

int
cf_parse_string (s, type)
	char	*s;
	int	type;
{
	return cfn_parse_string (&cf_ns_init, s, type);
}

int
cfn_parse_string (ns, s, type)
	struct cf_ns	*ns;
	char				*s;
	int				type;
{
	return cfn_parse_string2 (ns, NULL, s, type & CFPS_TYPEMASK);
}


int
cf_parse_string2 (outstr, s, type)
	char	**outstr, *s;
	int	type;
{
	return cfn_parse_string2 (&cf_ns_init, outstr, s, type);
}

int
cfn_parse_string2 (ns, outstr, s, type)
	struct cf_ns	*ns;
	char				**outstr, *s;
	int				type;
{
	char			*s2, *str, *se, *tstr;
	const char	*val;
	char			c;
	int			c2, i, ret, olen, vlen;
	int			hasalloc=0, hasdollar=0, haszero=0;

	if (!s) return RERR_PARAM;

	if ((type & CFPS_TYPEMASK) == CFPS_T_BBRACE) {
		if (*s == '@') {
			s++;
			if (*s!='@') {
				for (s2=s; *s2 && *s2 != '@'; s2++) {
					if (*s2 == '$') type |= CFPS_F_DOLLAR;
				}
				if (*s2) s=s2+1;
			}
		}
	}
	if (type & CFPS_F_COPY) {
		str = strdup (s);
		if (!str) return RERR_NOMEM;
		s = str;
		hasalloc = 1;
	} else {
		str = s;
		hasalloc = 0;
	}
	olen = strlen (str);
	hasdollar = type & CFPS_F_DOLLAR ? 1 : 0;
	type &= CFPS_TYPEMASK;
	if (type == CFPS_T_DOUBLEZERO) {
		haszero = 1;
		type = CFPS_T_DOUBLE;
	}
	
	switch (type) {
	case CFPS_T_SINGLE:
		for (s2=s; *s; s++,s2++) {
			if (*s == '\'' && s[1] == '\'') s++;
			*s2 = *s;
		}
		*s2 = 0;
		break;
	case CFPS_T_BRACE:
#if 0
		for (s2=s; *s; s++,s2++) {
			if (s[0] == '\\' && (s[1] == '}' || s[1] == '\\')) {
				s++;
			}
			*s2 = *s;
		}
		*s2 = 0;
		break;
#endif
	case CFPS_T_BBRACE:
		if (!hasdollar) break;
	case CFPS_T_DOUBLE:
		for (s2=s; *s; s++,s2++) {
			if (*s == '\\' && type == CFPS_T_DOUBLE) {
backslash:
				c=0; c2=0;
				s++;
				switch (*s) {
				case 'n': c='\n'; break;
				case 't': c='\t'; break;
				case 'r': c='\r'; break;
				case 'b': c='\b'; break;
				case 'a': c='\b'; break;
				case 'v': c='\v'; break;
				case 'x': case 'X': 
					if (ishex (s[1]) && ishex (s[2])) {
						if (*s=='x') {
							c2=gethex (s[1])*16+gethex (s[2]);
						} else {
							c=gethex (s[1])*16+gethex (s[2]);
						}
						s+=2;
					} else {
						c=*s;
					}
					break;
				case 'u':
					for (i=1; i<=4; i++) if (!ishex (s[i])) break;
					if (i<=4) {
						c='u';
						break;
					}
					for (c2=0, i=1; i<=4; i++) {
						c2*=16;
						c2+=gethex (s[i]);
					}
					s+=4;
					break;
				case 'U':
					for (i=1; i<=6; i++) if (!ishex (s[i])) break;
					if (i<=6) {
						c='U';
						break;
					}
					for (c2=0, i=1; i<=6; i++) {
						c2*=16;
						c2+=gethex (s[i]);
					}
					s+=6;
					break;
				case '\\':
					if (haszero) {
						*s2 = '\\';s2++;
					}
					c='\\';
					break;
				default:
					if (isoctal (s[0]) && isoctal (s[0]) && isoctal (s[0])) {
						c2 = (s[1]-'0')*64+(s[2]-'0')*8+(s[3]-'0');
						s+=3;
					} else if (*s=='0') {
						if (haszero) {
							*s2='\\';s2++;
						}
						c='0';
					} else {
						c2=*s;
					}
					break;
				}
				if (!c && (c2 < 127)) c = c2;
				if (c) {
					*s2=c;
				} else if (c2) {
					/* convert to utf8 */
					ret = ucs4utf8_char (&s2, c2);
					if (RERR_ISOK (ret)) {
						s2--;
					} else {
						*s2 = '.';
					}
				}
			} else if (*s == '$' && hasdollar) {
				s++;
				if (*s == '{') {
					for (se=++s; *se && *se != '}'; se++);
					*se=0; se++;
					c=*se;
				} else if (*s == '\\') {
					goto backslash;
				} else if (*s == '$') {
					*s2 = '$';
					continue;
				} else {
					for (se=s; isalnum (*se); se++);
					c=*se;
					*se=0;
				}
				temp_no_read = 1;	/* disable recursive read_config call */
				val = cfn_getvar (ns, s);
				temp_no_read = 0;
				if (!val && s) val = getenv (s);
				if (!val) val="";
				*se = c;
				s = se-1;
				vlen = strlen (val);
				if (se - s2 < vlen) {
					olen += vlen - (se-s2);
					if (!hasalloc) {
						tstr = malloc (olen + 1);
						if (!tstr) return RERR_NOMEM;
						memcpy (tstr, str, (se-str) + strlen (se) + 1);
						/* strcpy (tstr, str); */
						hasalloc = 1;
					} else {
						tstr = realloc (str, olen+1);
						if (!tstr) {
							free (str);
							return RERR_NOMEM;
						}
					}
					se = tstr + (se - str);
					s2 = tstr + (s2 - str);
					s = se + (vlen - (se-s2));
					str = tstr;
					memmove (s, se, strlen (se)+1);
					s--;
				}
				strncpy (s2, val, vlen);
				s2 += strlen (val) - 1;
			} else {
				*s2=*s;
			}
		}
		*s2 = 0;
		break;
	default:
		if (hasalloc) free (str);
		return RERR_PARAM;
	}

	if (outstr) {
		*outstr = str;
	} else if (hasalloc) {
		free (str);
	}
	return RERR_OK;
}



static const char * yes_list[] = {
		"yes", "y", "true", "1", "ja", "si", "oui", NULL};


int
cf_isyes (val)
	const char	*val;
{
	const char	**p;

	if (!val || !*val) return 0;
	for (p=yes_list; *p; p++) {
		if (!strcasecmp (*p, val)) return 1;
	}
	return 0;
}


int
cf_print_config ()
{
	return cfn_print_config (&cf_ns_init);
}

int
cfn_print_config (ns)
	struct cf_ns	*ns;
{
	if (!ns) return RERR_PARAM;
	MAY_READ_CONFIG (ret);
	return cf_print_varlist (ns->all_cvs.list);
}


int
cf_print_varlist (list)
	struct cf_cventry	*list;
{
	struct cf_cventry	*p;

	for (p=list; p&&p->var; p++) {
		if (p->flags & CFENTRY_F_ISDIR) continue;
		docf_printvar (list, p);
		if (p->val && (index (p->val, '\n') || index (p->val, '"') 
							|| index (p->val, '\''))) {
			printf (" = {%s}\n", p->val);
		} else {
			printf (" = \"%s\"\n", p->val?p->val:"<NULL>");
		}
	}
	return RERR_OK;
}


static
int
docf_printvar (list, p)
	struct cf_cventry	*list, *p;
{
	struct cf_cventry	*q;

	if (!p) return RERR_PARAM;
	if (!p->var) return RERR_OK;
	if (p->father != 0) {
		for (q=list; q&&q->var; q++) {
			if (!(q->flags & CFENTRY_F_ISDIR)) continue;
			if (q->d_num == p->father) break;
		}
		docf_printvar (list, q);
	}
	if (p->idx) {
		printf ("%s[%s]", p->var, p->idx);
	} else {
		printf ("%s", p->var);
	}
	if (p->flags & CFENTRY_F_ISDIR) printf ("/");
	return RERR_OK;
}


static
char *
getstr (s)
	char	** s;
{
	static char	buf[1024];
	char		* str;
	int			num;

	str = *s;
	for (; **s && isalnum (**s); (*s)++);
	num = *s-str;
	if (num<0) num=0;
	else if (num > 1023) num=1023;
	strncpy (buf, str, num);
	buf[num]=0;
	return buf;
}

int
cf_parse_table (str, table)
	char		*str;
	int32_t	**table;
{
	char		*s, *s2, *str2;
	int32_t	num, lastnum=-1;
	int		haverange=0;
	int		ret;
	tab32_t	tab;

	if (!str || !table) return RERR_PARAM;
	s = str2 = strdup (str);
	if (!s) return RERR_NOMEM;
	init_tab (&tab);

	for (; *s; s++) {
		if (*s == '"') {
			s++; s2=s;
			while (1) {
				s2 = index (s2, '"');
				if (!s2 || *(s2-1) != '\\') break;
				s2++;
			}
			if (s2)*s2 = 0;
			cf_parse_string (s, 3);
			ret = insert_str_to_tab (s, &tab, 1);
			if (!RERR_ISOK (ret)) {
				free (str2);
				return ret;
			}
			if (!s2) break;
			s=s2;
		} else if (*s == '\'') {
			s++; s2=s;
			while (1) {
				s2 = index (s2, '\'');
				if (!s2 || *(s2-1) != '\\') break;
				s2++;
			}
			if (s2)*s2 = 0;
			if (strlen (s) == 1) {
				num = *s;
				s=s2;
				goto label_insert_num;
			}
			cf_parse_string (s, 1);
			ret = insert_str_to_tab (s, &tab, 0);
			if (!RERR_ISOK (ret)) {
				free (str2);
				return ret;
			}
			if (!s2) break;
			s=s2;
		} else if (isalnum (*s)) {
			s2 = getstr (&s); 
			if (!*s2) continue;
			s--;
			if (ishexstr (s2)) {
				num = gethexstr (s2);
			} else if (isoctalstr (s2)) {
				num = getoctalstr (s2);
			} else if (isnumstr (s2)) {
				num = getnumstr (s2);
			} else {
				/* error - ignore */
				continue;
			}
label_insert_num:
			if (haverange && lastnum != -1) {
				ret = insert_range_to_tab (lastnum+1, num, &tab);
				if (!RERR_ISOK (ret)) {
					free (str2);
					return ret;
				}
				lastnum = -1;
				haverange = 0;
			} else {
				ret = insert_num_to_tab (num, &tab);
				if (!RERR_ISOK (ret)) {
					free (str2);
					return ret;
				}
				lastnum = num;
			}
		} else if (*s == '-') {
			haverange = 1;
		}
	}
	free (str2);
	ret = finish_tab (&tab);
	if (!RERR_ISOK (ret)) return ret;
	*table = tab.tab;

	return RERR_OK;
}



static
int
ishexstr (s)
	const char	* s;
{
	if (*s == 'x' || *s == 'X') {
		s++;
	} else if ((*s == '\\' || *s == '0') && (s[1] == 'x' || s[1] == 'X')) {
		s+=2;
	} else {
		return 0;
	}
	for (; *s; s++) {
		if (!ishex (*s)) return 0;
	}
	return 1;
}


static
int
isoctalstr (s)
	const char	* s;
{
	if (*s != '0') return 0;
	for (s++; *s; s++) {
		if (!isoctal (*s)) return 0;
	}
	return 1;
}

static
int
isnumstr (s)
	const char	* s;
{
	for (; *s; s++) {
		if (!range (*s,'0','9')) return 0;
	}
	return 1;
}


static
int32_t
gethexstr (s)
	char	* s;
{	
	int32_t	num;

	if (*s == 'x') s++;
	else s+=2;
	for (num=0; *s; s++) {
		num*=16;
		num+=gethex(*s);
	}
	return num;
}

static
int32_t
getoctalstr (s)
	char	* s;
{
	int32_t	num;

	for (num=0; *s; s++) {
		num*=8;
		num+=getoctal(*s);
	}
	return num;
}

static
int32_t
getnumstr (s)
	char	*s;
{
	int32_t	num;

	for (num=0; *s; s++) {
		num*=10;
		num+=*s-'0';
	}
	return num;
}

#define swap(a,b)	{ (a)^=(b); (b)^=(a); (a)^=(b); }

static
int
insert_range_to_tab (lb, ub, tab)
	int32_t	lb, ub;
	tab32_t	* tab;
{
	int		ret;
	int32_t	i;

	if (ub < lb) swap(ub,lb);
	for (i=lb; i<=ub; i++) {
		ret = insert_num_to_tab (i, tab);
		if (!RERR_ISOK (ret)) return ret;
	}
	return RERR_OK;
}


static
int
insert_str_to_tab (str, tab, intzero)
	char	* str;
	tab32_t	* tab;
	int		intzero;	/* interprete \0 */
{
	int		ret;

	for (; *str; str++) {
		if (intzero && *str=='\\' && (str[1] == '\\' || str[1] == '0')) {
			str++;
			if (*str == '0') {
				ret = insert_num_to_tab (0, tab);
			} else {
				ret = insert_num_to_tab ('\\', tab);
			}
		} else {
			ret = insert_num_to_tab (*str, tab);
		}
		if (!RERR_ISOK (ret)) return ret;
	}
	return RERR_OK;
}


static
int
insert_num_to_tab (num, tab)
	int32_t	num;
	tab32_t	* tab;
{
	if (!tab) return RERR_PARAM;
	if (tab->tabsize >= tab->bufsize) {
		tab->bufsize += 256;
		tab->tab = realloc (tab->tab, tab->bufsize*sizeof (int32_t));
		if (!tab->tab) return RERR_NOMEM;
		bzero (tab->tab+tab->tabsize, (tab->bufsize-tab->tabsize)
				*sizeof (int32_t));
	}
	tab->tab[tab->tabsize] = num;
	tab->tabsize++;
	return RERR_OK;
}

static
int
init_tab (tab)
	tab32_t	*tab;
{
	if (!tab) return RERR_PARAM;
	bzero (tab, sizeof (tab32_t));
	return RERR_OK;
}

static
int
finish_tab (tab)
	tab32_t	*tab;
{
	if (!tab) return RERR_PARAM;
	if (tab->tabsize+1 != tab->bufsize) {
		tab->bufsize = tab->tabsize+1;
		tab->tab = realloc (tab->tab, tab->bufsize*sizeof (int32_t));
		if (!tab->tab) return RERR_NOMEM;
	}
	tab->tab[tab->tabsize] = -1;
	return RERR_OK;
}


int
cf_print_table (tab)
	int32_t	** tab;
{
	int32_t	* p;
	int		i;

	if (!tab) return 0;
	for (i=0,p=*tab; *p!=-1; p++,i++) {
		if (i==11) {
			printf ("\n");
			i=0;
		}
		if (*p == 0) {
			printf ("\'\\0\' , ");
		} else if (*p == '\n') {
			printf ("\'\\n\' , ");
		} else if (*p == '\r') {
			printf ("\'\\r\' , ");
		} else if (*p == '\t') {
			printf ("\'\\t\' , ");
		} else if (*p == '\v') {
			printf ("\'\\v\' , ");
		} else if (*p == '\b') {
			printf ("\'\\b\' , ");
		} else if (*p >= 32 && *p < 128) {
			printf ("\'%c\'  , ", (char) *p);
		} else if (*p < 0x10) {
			printf ("0x%x  , ", (unsigned) *p);
		} else if (*p < 0x100) {
			printf ("0x%x , ", (unsigned) *p);
		} else {
			printf ("0x%x, ", (unsigned) *p);
		}
	}
	printf ("\n");

	return 1;
}


int
cf_atoi (str)
	const char	*str;
{
	if (!str || !*str) return 0;
	return atoi (str);
}

tmo_t
cf_atotm (str)
	const char	*str;
{
	char	buf[64];

	if (!str || !*str) return 0;
	strncpy (buf, str, sizeof (buf)-1);
	buf[sizeof(buf)-1] = 0;
	return tmo_gettimestr64 (buf);
}


const char *
cf_getmarr2 (
	const char	*var,
	const char	*defval,
	int			numidx,
	...)
{
	va_list		ap;
	const char	*val;

	va_start (ap, numidx);
	val = cf_vgetmarr2 (var, defval, numidx, ap);
	va_end (ap);
	return val;
}



const char *
cf_vgetmarr2 (var, defval, numidx, ap)
	const char	*var,*defval;
	int			numidx;
	va_list		ap;
{
	const char	*val;

	val = cf_vgetmarr (var, numidx, ap);
	return val?val:(char*)defval;
}


const char *
cf_getmarr (
	const char	*var,
	int			numidx,
	...)
{
	va_list		ap;
	const char	*val;

	va_start (ap, numidx);
	val = cf_vgetmarr (var, numidx, ap);
	va_end (ap);
	return val;
}




const char *
cf_vgetmarr (var, numidx, ap)
	const char	*var;
	int			numidx;
	va_list		ap;
{
	int			i, num;
	char			*first, *s, *idx;
	const char	*val;
	int			idxlen;
	va_list		ap2, ap3;

	if (!var || !*var || numidx<0) return NULL;
	if (numidx == 0) return cf_getval(var);
#ifdef va_copy
	va_copy (ap3, ap);
#else
	ap3 = ap;
#endif
	first = va_arg (ap, char*);
	if (!first) return cf_getval(var);
	if (numidx == 1) return cf_getarr(var,first);
#ifdef va_copy
	va_copy (ap2, ap);
#else
	ap2 = ap;
#endif
	idxlen = strlen (first)+1;
	for (i=1; i<numidx; i++) {
		s = va_arg (ap, char*);
		if (!s) continue;
		idxlen += strlen (s)+1;
	}
	idx = malloc (idxlen+1);
	if (!idx) return NULL;
	if (isnumstr (first)) {
		num = atoi (s);
		sprintf (idx, "%d", num);
	} else {
		strcpy (idx, first);
	}
#ifdef va_copy
	va_copy (ap, ap2);
#else
	ap = ap2;
#endif
	for (i=1; i<numidx; i++) {
		s = va_arg (ap, char*);
		if (!s) continue;
		strcat (idx,",");
		if (isnumstr (s)) {
			num = atoi (s);
			sprintf (idx+strlen(idx), "%d", num);
		} else {
			strcat (idx, s);
		}
	}
	val = cf_getarrnodef (var, idx);
	free (idx);
	if (!val) return cf_vgetmarr (var, numidx-1, ap3);
	return val;
}


const char*
cf_get2arr (var, idx1, idx2)
	const char	*var, *idx1, *idx2;
{
	return cf_getmarr (var, 2, idx1, idx2);
}


const char*
cf_get2arr2 (var, idx1, idx2, defval)
	const char	*var, *idx1, *idx2, *defval;
{
	return cf_getmarr2 (var, defval, 2, idx1, idx2);
}




struct funct {
	union {
		cf_reread_t		fs;
		cf_reread2_t	fa;
	};
	void	*arg;
};


struct func_list {
	struct funct	*list;
	int				num;
};
#define FUNC_LIST_NULL	((struct func_list){NULL, 0})
static struct func_list	func_list = FUNC_LIST_NULL;


int
cf_register_reread_callback (func)
	cf_reread_t	func;
{
	int				num, i;
	struct funct	*p, f;

	if (!func) return RERR_PARAM;
	if (no_callback) return RERR_OK;
	for (i=0; i<func_list.num; i++) {
		if (func_list.list[i].fs == func) return RERR_OK;
	}
	num = (func_list.num);
	p = realloc (func_list.list, (num+1)*sizeof (struct funct));
	if (!p) return RERR_NOMEM;
	func_list.list = p;
	func_list.num++;
	f.fs = func;
	f.arg = NULL;
	func_list.list[num]=f;
	return RERR_OK;
}


int
cf_deregister_reread_callback (func)
	cf_reread_t	func;
{
	int		i;

	if (!func) return RERR_PARAM;
	if (no_callback) return RERR_OK;
	for (i=0; i<func_list.num; i++) {
		if (func_list.list[i].fs == func) goto found;
	}
	return RERR_NOT_FOUND;
found:
	for (; i<func_list.num-1; i++) {
		func_list.list[i]=func_list.list[i+1];
	}
	func_list.num--;
	return RERR_OK;
}

int
cf_register_reread_callback2 (func, arg)
	cf_reread2_t	func;
	void				*arg;
{
	int				num, i;
	struct funct	*p, f;

	if (!func) return RERR_PARAM;
	if (no_callback) return RERR_OK;
	for (i=0; i<func_list.num; i++) {
		if (func_list.list[i].fa == func && func_list.list[i].arg == arg) {
			return RERR_OK;
		}
	}
	num = (func_list.num);
	p = realloc (func_list.list, (num+1)*sizeof (struct funct));
	if (!p) return RERR_NOMEM;
	func_list.list = p;
	func_list.num++;
	f.fa = func;
	f.arg = arg;
	func_list.list[num]=f;
	return RERR_OK;
}


int
cf_deregister_reread_callback2 (func, arg)
	cf_reread2_t	func;
	void				*arg;
{
	int		i;

	if (!func) return RERR_PARAM;
	if (no_callback) return RERR_OK;
	for (i=0; i<func_list.num; i++) {
		if (func_list.list[i].fa == func && func_list.list[i].arg == arg) {
			goto found;
		}
	}
	return RERR_NOT_FOUND;
found:
	for (; i<func_list.num-1; i++) {
		func_list.list[i]=func_list.list[i+1];
	}
	func_list.num--;
	return RERR_OK;
}


static
int
call_reread_funcs ()
{
	int		i;

	for (i=0; i<func_list.num; i++) {
		if (func_list.list[i].arg) {
			if (func_list.list[i].fa) func_list.list[i].fa (func_list.list[i].arg);
		} else {
			if (func_list.list[i].fs) func_list.list[i].fs ();
		}
	}
	return RERR_OK;
}


int
cf_begin_read ()
{
	if (!pseudo_lock && !config_read) {
		read_config();
	}
	pseudo_lock++;
	return RERR_OK;
}


int
cf_end_read ()
{
	if (pseudo_lock>0) pseudo_lock--;
	return RERR_OK;
}


int
cf_end_read_cb (func)
	cf_reread_t	func;
{
	int		ret = RERR_OK;

	if (func) ret = cf_register_reread_callback (func);
	cf_end_read();
	return ret;
}

int
cf_end_read_cb2 (func, arg)
	cf_reread2_t	func;
	void				*arg;
{
	int		ret = RERR_OK;

	if (func) ret = cf_register_reread_callback2 (func, arg);
	cf_end_read();
	return ret;
}


static
void
cf_hup_reread_cb (sig)
	int		sig;
{
	if (sig != SIGHUP) return;
	cf_reread ();
	return;
}

int
cf_hup_reread ()
{
	signal (SIGHUP, &cf_hup_reread_cb);
	return RERR_OK;
}




const char*
cf_getvar2 (var, defval)
	const char	*var, *defval;
{
	return cfn_getvar2 (&cf_ns_init, var, defval);
}

const char*
cfn_getvar2 (ns, var, defval)
	struct cf_ns	*ns;
	const char		*var, *defval;
{
	const char	*ret;

	ret = cfn_getvar (ns, var);
	if (!ret) ret = (char*)defval;
	return ret;
}


const char*
cf_getvar (var)
	const char	*var;
{
	return cfn_getvar (&cf_ns_init, var);
}

const char*
cfn_getvar (ns, var)
	struct cf_ns	*ns;
	const char		*var;
{
	const char	*idx, *val;
	char	_buf[64], *buf=_buf;
	int	ret, needfree;

	if (!var || !*var || !ns) return NULL;
	ret = split_var (&buf, sizeof (_buf), (char**)&idx, &needfree, var);
	if (!RERR_ISOK (ret)) return NULL;
	val = cfn_getarr (ns, buf, idx);
	if (needfree) free (buf);
	return val;
}



static
int
split_var (varbuf, bufsz, idx, needfree, var)
	char			**varbuf, **idx;
	const char	*var;
	int			bufsz, *needfree;
{
	char	*s, *str, *slash;
	char	*buf, *idx2;
	int	len;

	if (!var || !varbuf || !idx || !needfree) return RERR_PARAM;
	buf = bufsz > 0 ? *varbuf : NULL;
	if (needfree) *needfree = 0;
	*idx = NULL;
	var = top_skipwhite (var);
	slash = (slash = findslash ((char*)var, -1)) ? (slash+1) : (char*)var;
	s = rindex (slash, '[');
	if (!s) {
		*varbuf = (char*)var;
		*needfree = 0;
		return RERR_OK;
	}
	if (!buf || (len=strlen (var)) > bufsz-1) {
		str = strdup (var);
		if (!str) return RERR_NOMEM;
	} else {
		strcpy (buf, var);
		str = buf;
	}
	s = str + (s - var);
	var = str;
	*s = 0;
	idx2 = s+1;
	s = index (idx2, ']');
	*s = 0;
	var = top_skipwhite (var);
	*idx = top_stripwhite (idx2, 0);
	if (str && str != buf && needfree) *needfree = 1;
	*varbuf = str;
	return RERR_OK;
}



const char*
cf_getvarf2 (
	const char	*defval,
	const char	*fmt,
	...)
{
	va_list		ap;
	const char	*val;

	va_start (ap,fmt);
	val = cf_vgetvarf2 (defval, fmt, ap);
	va_end (ap);
	return val;
}

const char*
cfn_getvarf2 (
	struct cf_ns	*ns,
	const char		*defval,
	const char		*fmt,
	...)
{
	va_list		ap;
	const char	*val;

	va_start (ap,fmt);
	val = cfn_vgetvarf2 (ns, defval, fmt, ap);
	va_end (ap);
	return val;
}


const char*
cf_vgetvarf2 (defval, fmt, ap)
	const char	*defval;
	const char	*fmt;
	va_list		ap;
{
	return cfn_vgetvarf2 (&cf_ns_init, defval, fmt, ap);
}

const char*
cfn_vgetvarf2 (ns, defval, fmt, ap)
	struct cf_ns	*ns;
	const char		*defval;
	const char		*fmt;
	va_list			ap;
{
	const char	*val;

	val = cfn_vgetvarf (ns, fmt, ap);
	if (!val) val = defval;
	return val;
}

const char*
cf_getvarf (
	const char	*fmt,
	...)
{
	va_list		ap;
	const char	*val;

	va_start (ap,fmt);
	val = cf_vgetvarf (fmt, ap);
	va_end (ap);
	return val;
}

const char*
cfn_getvarf (
	struct cf_ns	*ns,
	const char		*fmt,
	...)
{
	va_list		ap;
	const char	*val;

	va_start (ap,fmt);
	val = cfn_vgetvarf (ns, fmt, ap);
	va_end (ap);
	return val;
}


const char*
cf_vgetvarf (fmt, ap)
	const char		*fmt;
	va_list			ap;
{
	return cfn_vgetvarf (&cf_ns_init, fmt, ap);
}

const char*
cfn_vgetvarf (ns, fmt, ap)
	struct cf_ns	*ns;
	const char		*fmt;
	va_list			ap;
{
	char			*str;
	const char	*val;

	str = vasprtf (fmt, ap);	
	if (!str) return NULL;
	val = cf_getvar (str);
	free (str);
	return val;
}


static
int
getdnum (ns, dir)
	struct cf_ns	*ns;
	const char		*dir;
{
	struct cf_cventry	*p;

	if (!dir) dir="";
	dir = top_skipwhiteplus (dir, "/");
	if (!*dir) return 0;
	p = docf_getpath (ns, 0, (char*)dir);
	if (!p) return RERR_NOT_FOUND;
	if (!(p->flags & CFENTRY_F_ISDIR)) return RERR_NOT_FOUND;
	return p->d_num;
}


int
cf_getdirnum (dir)
	const char	*dir;
{
	return cfn_getdirnum (&cf_ns_init, dir);
}

int
cfn_getdirnum (ns, dir)
	struct cf_ns	*ns;
	const char		*dir;
{
	struct cf_cventry	*p;
	struct cf_cvlist	*list;
	int					num, i, d_num;

	d_num = getdnum (ns, dir);
	if (d_num < 0) return 0;
	list = ns->early_list ? ns->early_list : &ns->all_cvs;
	if (!list || !(list->list)) return 0;
	for (num=i=0; i<list->listsize; i++) {
		p = list->list + i;
		if (p->father == d_num) num++;
	}
	return num;
}

int
cf_getdirentry (dir, num, var, idx, isdir, val)
	const char	*dir;
	const char	**var, **idx, **val;
	int			num, *isdir;
{
	return cfn_getdirentry (&cf_ns_init, dir, num, var, idx, isdir, val);
}

int
cfn_getdirentry (ns, dir, num, var, idx, isdir, val)
	struct cf_ns	*ns;
	const char		*dir;
	const char		**var, **idx, **val;
	int				num, *isdir;
{
	struct cf_cventry	*p;
	struct cf_cvlist	*list;
	int					i, d_num;

	if (num < 0) return RERR_PARAM;
	d_num = getdnum (ns, dir);
	if (d_num < 0) return d_num;
	list = ns->early_list ? ns->early_list : &ns->all_cvs;
	if (!list || !(list->list)) return RERR_NOT_FOUND;
	for (i=0, num++; num > 0 && i<list->listsize; i++) {
		p = list->list + i;
		if (p->father == d_num) num--;
	}
	if (num > 0) return RERR_NOT_FOUND;
	if (var) *var = p->var;
	if (idx) *idx = p->idx;
	if (isdir) *isdir = (p->flags & CFENTRY_F_ISDIR) ? 1 : 0;
	if (val) *val = (p->flags & CFENTRY_F_ISDIR) ? p->val : NULL;
	return RERR_OK;
}



static
char*
findslash (str, pos)
	char	*str;
	int	pos;
{
	char	*s;
	int	inbrak = 0;

	if (!str || !*str) return NULL;
	if (pos == 0) return str;
	if (pos < 0) {
		s = str + strlen (str) - 1;
		for (; s >= str; s--) {
			if (*s == ']') {
				inbrak = 1;
			} else if (*s == '[') {
				inbrak = 0;
			} else if (!inbrak && *s == '/') {
				pos++;
				if (!pos) break;
			}
		}
		if (s >= str) return s;
	} else {
		for (s=str; *s; s++) {
			if (*s == '[') {
				inbrak = 1;
			} else if (*s == ']') {
				inbrak = 0;
			} else if (!inbrak && *s == '/') {
				pos--;
				if (!pos) break;
			}
		}
		if (*s) return s;
	}
	return NULL;
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
