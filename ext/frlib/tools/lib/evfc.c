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

#include <fr/base.h>
#include <fr/event.h>

#include "evfc.h"


static const char *PROG="evfc";

int
evfc_usage ()
{
	PROG = fr_getprog ();
	printf ("%s: usage: %s [<options>] <file>\n", PROG, PROG);
	printf ("    options are:\n"
				"\t-h                - this help screen\n"
				"\t-C <config file>  - alternative config file\n"
				"\t-c                - compile event filter\n"
				"\t-e <ending>       - ending for c-file output\n"
				"\t-E <ending>       - ending for h-file output\n"
				"\t-o <filename>     - filename for c-file output\n"
				"\t-O <filename>     - filename for h-file output\n"
				"\t-n <name>         - functions name\n"
				"\t-u                - filter is output\n"
				"\n");
	printf ("\tIf <file> is - (stdin is read). The content can be an event \n"
				"\tstring (in which case the filter will be applied) or an event \n"
				"\tfilter, which will be verified or compiled (if -c is given).\n");
	return 0;
}



int
evfc_main (argc, argv)
	int	argc;
	char	**argv;
{
	char	*filter=NULL,
			*filterid=NULL,
			*c_ending=NULL,
			*h_ending=NULL,
			*c_out=NULL,
			*h_out=NULL,
			*name=NULL,
			*fname=NULL,
			*filterbuf=NULL,
			*evstr=NULL;
	int	compile=0;
	char	*buf;
	int	ftype, ret;
	char	c, *s;
	int	oflags = 0;
	char	_r__errstr[96];

	PROG = fr_getprog ();

	while ((c=getopt (argc, argv, "hf:F:C:ce:E:o:O:n:u")) != -1) {
		switch (c) {
		case 'h':
			evfc_usage ();
			return 0;
		case 'C':
			cf_set_cfname (optarg);
			break;
		case 'f':
			filter = optarg;
			break;
		case 'F':
			filterid = optarg;
			break;
		case 'c':
			compile=1;
			break;
		case 'e':
			c_ending = optarg;
			break;
		case 'E':
			h_ending = optarg;
			break;
		case 'o':
			c_out = optarg;
			break;
		case 'O':
			h_out = optarg;
			break;
		case 'n':
			name = optarg;
			break;
		case 'u':
			oflags |= EVF_F_OUTTARGET;
			break;
		}
	}
	if (optind < argc) {
		fname = argv[optind];
		if (!strcmp (fname, "-")) {
			buf = fop_read_file (stdin);
			if (!buf) {
				fprintf (stderr, "%s: error reading from stdin: %s\n", PROG,
							rerr_getstr3(RERR_SYSTEM));
				return RERR_SYSTEM;
			}
		} else {
			buf = fop_read_fn (fname);
			if (!buf) {
				fprintf (stderr, "%s: error opening file >>%s<<: %s\n", PROG,
							fname, rerr_getstr3(RERR_SYSTEM));
				return RERR_SYSTEM;
			}
		}
		if (*buf == '*' || *buf == '#') {
			evstr = buf;
		} else {
			filterbuf = buf;
		}
	}
	if (filterbuf) {
		filter=filterbuf;
		ftype=EVFC_FT_BUF;
	} else if (filter) {
		ftype=EVFC_FT_FILE;
		fname = filter;
	} else if (filterid) {
		ftype=EVFC_FT_ID;
		fname=filterid;
		filter = filterid;
	} else {
		fprintf (stderr, "%s: no filter specified\n", PROG);
		return RERR_PARAM;
	}
	if (evstr && compile) {
		fprintf (stderr, "%s: -c cannot be combined with an event string\n",
					PROG);
		return RERR_PARAM;
	}
	if (evstr) {
		ret = evfc_apply (evstr, ftype, filter, oflags);
		if (!RERR_ISOK(ret)) {
			if (ret != RERR_FAIL) {
				fprintf (stderr, "%s: error applying filter: %s\n", PROG,
										rerr_getstr3(ret));
			}
			return ret;
		}
	} else if (!compile) {
		ret = evfc_verify (ftype, filter, oflags);
		if (!RERR_ISOK(ret)) {
			if (ret != RERR_FAIL) {
				fprintf (stderr, "%s: error verifying filter: %s\n",
							PROG, rerr_getstr3(ret));
			}
			return ret;
		}
	} else {
		if (!name) {
			if (!strcmp (fname, "-")) {
				fprintf (stderr, "%s: when reading from stdin, you must specify "
							"a name (-n)\n", PROG);
				return RERR_PARAM;
			}
			name = (name = rindex (fname, '/')) ? name + 1 : fname;
			if (ftype != EVFC_FT_ID) {
				s = rindex (name, '.');
				if (s) {
					name = strdup (name);
					if (!name) return RERR_NOMEM;
					s = rindex (name, '.');
					if (s) *s=0;
				}
			}
		}
		if (!c_ending) c_ending = (char*)"c";
		if (!h_ending) h_ending = (char*)"h";
		if (!c_out) {
			c_out = asprtf ("%s.%s", name, c_ending);
			if (!c_out) return RERR_NOMEM;
		}
		if (!h_out) {
			h_out = asprtf ("%s.%s", name, h_ending);
			if (!h_out) return RERR_NOMEM;
		}
		ret = evfc_compile (ftype, filter, name, c_out, h_out, oflags);
		if (!RERR_ISOK(ret)) {
			if (ret != RERR_FAIL) {
				fprintf (stderr, "%s: error compiling filter: %s\n", PROG,
							rerr_getstr3(ret));
			}
			return ret;
		}
	}
	/* don't clean up */
	return 0;
}

int
evfc_apply (evstr, ftype, filter, flags)
	char	*evstr, *filter;
	int	ftype, flags;
{
	int				fid, ret;
	struct event	ev;
	struct evlist	evlist, *ptr;
	char				*outstr;
	char				_r__errstr[96];

	fid = evf_new ();
	if (!RERR_ISOK(fid)) return fid;
	switch (ftype) {
	case EVFC_FT_FILE:
		ret = evf_addfile (fid, filter, flags | EVF_F_VERBOSE);
		break;
	case EVFC_FT_ID:
		ret = evf_addfilter (fid, filter, flags | EVF_F_VERBOSE);
		break;
	case EVFC_FT_BUF:
		ret = evf_addbuf (fid, filter, flags | EVF_F_VERBOSE);
		break;
	}
	if (!RERR_ISOK(ret)) {
		if (ret != RERR_FAIL) {
			fprintf (stderr, "%s: error compiling filter: %s\n", PROG,
						rerr_getstr3(ret));
		}
		evf_release (fid);
		return ret;
	}
	ret = ev_new (&ev);
	if (!RERR_ISOK(ret)) {
		evf_release (fid);
		return ret;
	}
	ret = ev_parse (&ev, evstr, 0);
	if (!RERR_ISOK(ret)) {
		fprintf (stderr, "%s: error parsing event: %s\n", PROG,
					rerr_getstr3(ret));
		evf_release (fid);
		return ret;
	}
	ret = evf_apply (&evlist, fid, &ev, flags);
	if (!RERR_ISOK(ret)) {
		fprintf (stderr, "%s: error applying filter: %s\n", PROG,
					rerr_getstr3(ret));
		evf_release (fid);
		ev_free (&ev);
		return ret;
	}
	for (ptr=&evlist; ptr; ptr=ptr->next) {
		if (ptr->target == EVF_T_DROP) continue;
		ret = ev_create (&outstr, ptr->event, 0);
		if (!RERR_ISOK(ret)) {
			fprintf (stderr, "%s: error compiling event string: %s\n", PROG,
						rerr_getstr3(ret));
			evf_evlistfree (&evlist, 0);
			evf_release (fid);
			ev_free (&ev);
			return ret;
		}
		printf ("%s\n", outstr);
		free (outstr);
	}
	evf_evlistfree (&evlist, 0);
	evf_release (fid);
	ev_free (&ev);
	return RERR_OK;
}

int
evfc_verify (ftype, filter, flags)
	char	*filter;
	int	ftype, flags;
{
	int	fid;
	int	ret;
	char	_r__errstr[96];

	fid = evf_new ();
	if (!RERR_ISOK(fid)) return fid;
	flags |= EVF_F_VERBOSE | EVF_F_STRICT;
	switch (ftype) {
	case EVFC_FT_FILE:
		ret = evf_addfile (fid, filter, flags);
		break;
	case EVFC_FT_ID:
		ret = evf_addfilter (fid, filter, flags);
		break;
	case EVFC_FT_BUF:
		ret = evf_addbuf (fid, filter, flags);
		break;
	}
	if (!RERR_ISOK(ret)) {
		if (ret != RERR_FAIL) {
			fprintf (stderr, "%s: error generating filter: %s\n", PROG,
						rerr_getstr3(ret));
		}
		evf_release (fid);
		return ret;
	}
	evf_release (fid);
	return RERR_OK;
}


int
evfc_compile (ftype, filter, name, c_out, h_out, flags)
	int	ftype, flags;
	char	*filter, *name, *c_out, *h_out;
{
	int		ret; 
	char		*buf=NULL, *obuf;
	size_t	sz;
	char		*s, *s2;
	char		*bname;
	FILE		*f;
	int		isout=0;	/* to be done... */
	char		_r__errstr[96];

	if (!filter || !name || !c_out || !h_out) return RERR_PARAM;
	ret = evfc_verify (ftype, filter, flags | EVF_F_COPY);
	if (!RERR_ISOK(ret)) return ret;
	if (ftype == EVFC_FT_FILE) {
		buf = fop_read_fn (filter);
		if (!buf) return RERR_SYSTEM;
		filter = buf;
		ftype = EVFC_FT_BUF;
	}
	/* calculate quote size */
	if (ftype == EVFC_FT_BUF) {
		for (sz=4, s=filter; *s; s++) {
			if (*s == '\n') {
				sz+=6;
			} else if (*s == '\r') {
				sz+=2;
			} else if (*s == '\t') {
				sz+=2;
			} else if (*s == '\"') {
				sz+=2;
			} else if (*s < 32) {
				sz+=4;
			} else {
				sz++;
			}
		}
		obuf = malloc (sz+1);
		if (!obuf) return RERR_NOMEM;
		*obuf='\"'; s2 = obuf+1;
		for (s=filter; *s; s++) {
			if (*s == '\n') {
				strcpy (s2, "\\n\"\n\t\"");
				s2 += 6;
			} else if (*s == '\r') {
				strcpy (s2, "\\r");
				s2 += 2;
			} else if (*s == '\t') {
				strcpy (s2, "\\t");
				s2 += 2;
			} else if (*s == '\"') {
				strcpy (s2, "\\\"");
				s2 += 2;
			} else if (*s < 32) {
				s2 += sprintf (s2, "\\x%02x", (unsigned)*s);
			} else {
				*s2 = *s;
				s2++;
			}
		}
		strcpy (s2, "\"");
		if (buf) free (buf);
	}
	bname = strdup (name);
	if (!bname) {
		if (obuf) free (obuf);
		return RERR_NOMEM;
	}
	for (s=bname; *s; s++) {
		if (islower(*s)) *s = toupper (*s);
	}
	f = fopen (c_out, "w");
	if (!f) {
		if (obuf) free (obuf);
		free (bname);
		fprintf (stderr, "%s: cannot open file >>%s<< for writing: %s\n", PROG,
					c_out, rerr_getstr3(RERR_SYSTEM));
		return RERR_SYSTEM;
	}
	fprintf (f, "/* file generated by evfc */\n"
				"#include <stdlib.h>\n"
				"#include <stdio.h>\n"
				"#include <fr/base.h>\n"
				"#include <fr/event.h>\n"
				"#include \"%s\"\n\n"
				"#ifdef __cplusplus\n"
				"extern \"C\" {\n"
				"#endif\n\n" , h_out);
	if (obuf) {
		fprintf (f, "static const char *%s_buf = %s;\n",
				name, obuf);
		free (obuf);
	}
	fprintf (f, "static int filter_id = -1;\n\n"
				"int\n%s (int flags)\n{\n"
				"   int   ret;\n\n"
				"   if (filter_id < 0) {\n"
				"      filter_id = evf_new ();\n"
				"      if (filter_id < 0) {\n"
				"         SLOGFE (LOG_ERR, \"error creating new filter: %%s\", \n"
				"                rerr_getstr3(filter_id));\n"
				"         return filter_id;\n"
				"      }\n" , name);
	if (ftype == EVFC_FT_BUF) {
		fprintf (f, "      ret = evf_addconstbuf (filter_id, %s_buf, %s);\n",
					name, isout ? "EVF_F_OUT" : "EVF_F_IN");
	} else {
		fprintf (f, "      ret = evf_addfilter (filter_id, %s, %s);\n", filter,
					isout ? "EVF_F_OUT" : "EVF_F_IN");
	}
	fprintf (f, ""
				"      if (!RERR_ISOK(ret)) {\n"
				"         SLOGFE (LOG_ERR, \"error adding filter: %%s\", \n"
				"                rerr_getstr3(ret));\n"
				"         evf_release (filter_id);\n"
				"         filter_id = -1;\n"
				"         return ret;\n"
				"      }\n"
				"   }\n"
				"   if (flags & %s_F_INSTALL) {\n"
				"      ret = eddi_addfilter (filter_id);\n"
				"      if (!RERR_ISOK(ret)) {\n"
				"         SLOGFE (LOG_ERR, \"error installing filter: %%s\", \n"
				"                rerr_getstr3(ret));\n"
				"         if (flags & %s_F_RELEASE) {\n"
				"            evf_release (filter_id);\n"
				"            filter_id = -1;\n"
				"         }\n"
				"         return ret;\n"
				"      }\n"
				"   }\n"
				"   if (flags & %s_F_RELEASE) {\n"
				"      evf_release (filter_id);\n"
				"      filter_id = -1;\n"
				"   }\n"
				"   return filter_id;\n"
				"}\n\n\n", bname, bname, bname);
	fprintf (f, ""
				"#ifdef __cplusplus\n"
				"}  /* extern \"C\" */\n"
				"#endif\n\n\n");
	fclose (f);
	f = fopen (h_out, "w");
	if (!f) {
		free (bname);
		fprintf (stderr, "%s: cannot open file >>%s<< for writing: %s\n", PROG,
					h_out, rerr_getstr3(RERR_SYSTEM));
		return RERR_SYSTEM;
	}
	fprintf (f, "/* file generated by evfc */\n"
				"#ifndef _R__EVFC_GENERATED_%s_H\n"
				"#define _R__EVFC_GENERATED_%s_H\n\n\n"
				"#ifdef __cplusplus\n"
				"extern \"C\" {\n"
				"#endif\n\n\n"
				"#define %s_F_NONE     0\n"
				"#define %s_F_RELEASE  1\n\n"
				"#define %s_F_INSTALL  2\n\n"
				"int %s (int flags);\n\n\n\n"
				"#ifdef __cplusplus\n"
				"}  /* extern \"C\" */\n"
				"#endif\n\n\n"
				"#endif  /* _R__EVFC_GENERATED_%s_H */\n",
				bname, bname, bname, bname, bname, name, bname);
	fclose (f);
	free (bname);

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
