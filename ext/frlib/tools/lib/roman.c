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
#include <errno.h>
#include <getopt.h>
#include <ctype.h>

#include <fr/base.h>

#include "roman.h"


static const char * PROG = "roman";
static const char * usage1 = "\tThis program converts arabic numbers to roman and"
						"vice versa.\n";
static const char * usage2 = 
				"\t -a, --oldstyle, --additive\n"
				"\t -s, --newstyle, --subtractive\n"
				"\t -l, --lowercase, --minuscola\n"
				"\t -u, --uppercase, --maiuscola\n"
				"\t -m, --multiply\n"
				"\t -c, --cifrao\n"
				"\t -d, --dot\n"
				"\t -n, --neg\n"
				"\t -r, --resultonly\n";

/* struct for getopt */
static const struct option lopts [] = {
					{"oldstyle", 0, NULL, 'a'},
					{"additive", 0, NULL, 'a'},
					{"newstyle", 0, NULL, 's'},
					{"subtractive", 0, NULL, 's'},
					{"minuscola", 0, NULL, 'l'},
					{"lowercase", 0, NULL, 'l'},
					{"maiuscola", 0, NULL, 'u'},
					{"uppercase", 0, NULL, 'u'},
					{"resultonly", 0, NULL, 'r'},
					{"result-only", 0, NULL, 'r'},
					{"cifrao", 0, NULL, 'c'},
					{"multiply", 0, NULL, 'm'},
					{"dot", 0, NULL, 'd'},
					{"help", 0, NULL, 'h'},
					{"usage", 0, NULL, 'h'},
					{"neg", 0, NULL, 'n'},
					{NULL, 0, NULL, 0}};
static const char sopts[] = "aslumcdnr";

static char* getnumber ();
static int convert (char*, int, int);



int
roman_usage ()
{
	PROG = fr_getprog ();
	printf ("%s: usage: %s [<options>] [<numbers>...]\n", PROG, PROG);
	printf ("%soptions are:\n%s", usage1, usage2);
	return 0;
}

	
int 
roman_main (argc, argv)
	int 	argc;
	char 	**argv;
{	
	char	**numbers = NULL;
	int	numnumbers = 0;
	int	opt, optindex;
	int	ret=0;
	int 	dashdash=0;
	int	i, flags;
	char	*s;
	int	resultonly;

	PROG = fr_getprog ();

	/* scan the command line */
	flags = 0;
	while ((opt = getopt_long (argc, argv, sopts, lopts, &optindex))
					!= -1) {
		switch (opt) {
		case 's':		/* --newstyle */
			flags |= ROM_SUB;
			break;
		case 'a':		/* --oldstyle */
			flags |= ROM_ADD;
			break;
		case 'l':		/* --minuscola */
			flags |= ROM_LCASE;
			break;
		case 'u':		/* --maiuscola */
			flags |= ROM_UCASE;
			break;
		case 'r':		/* --resultonly */
			resultonly = 1;
			break;
		case 'm':
			flags |= ROM_MUL;
			break;
		case 'c':
			flags |= ROM_CIFRAO;
			break;
		case 'd':
			flags |= ROM_DOT;
			break;
		case 'n':
			flags |= ROM_NEG;
			break;
		case 'h':		/* --help */
			roman_usage ();
			return 0;
		}
	}

	/* get numbers to process */
	if (optind > 0 && optind < argc) {
		numbers = calloc (sizeof (char*), argc - optind);
		if (!numbers) {
			FRLOGF (LOG_ERR2, "out of memory");
			return RERR_NOMEM;
		}
		for (i=optind; i<argc; i++) {
			if (!strcmp (argv[i], "--")) 
				if (!dashdash) {
					dashdash=1;
					continue;
				}
			if (!strcmp (argv[i], "-") && !dashdash)
				numbers[numnumbers] = strdup ("");
			numbers[numnumbers] = strdup (argv[i]);
			numnumbers++;
        }
    }
	/* if no numbers are given, read from stdin */
	if (!numnumbers) {
		if (!numbers) 
			numbers = malloc (sizeof (char *));
		numbers[0] = strdup ("");
		numnumbers = 1;
	}

	/* make some default settings, if neccessary */

	/* now process all numbers */
	for (i=0; i<numnumbers; i++) {
		if (!numbers[i][0]) {
			while ((s=getnumber ())) {
				convert (s, flags, resultonly);
			}
		} else {
			convert (numbers[i], flags, resultonly);
		}
	}
	return ret;
}



#define isvalch(c)	(isalnum(c)||((c)=='$')||((c)=='.')||((c)=='*')||((c)=='-'))


static
char *
getnumber ()
{
	int			c;
	static char	str[1024];
	char			*s=str;
	
	while ((c=getc (stdin)) != EOF && !isvalch (c));
	while (c != EOF && isvalch (c)) {
		if (s-str == 1023) break;
		*s++=(unsigned char) c;
		c = getc (stdin);
	}
	*s = 0;
	if (c == EOF) {
		return NULL;
	} else {
		return str;
	}
}


static
int
convert (number, flags, resultonly)
	char	*number;
	int	flags, resultonly;
{
	int 	n;
	char	*endptr;
	int	ret;
	char	buf[1024];

	if (!number) return RERR_PARAM;
	n = strtoul (number, &endptr, 0);
	if (!*endptr) {
		ret = num2roman (buf, sizeof(buf), n, flags);
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_ERR2, "error converting >>%d<<: %s", n, 
									rerr_getstr3(ret));
			return ret;
		}
		if (!resultonly) {
			printf ("%d = %s\n", n, buf);
		} else {
			printf ("%s\n", buf);
		}
	} else {
		ret = roman2num (&n, number, flags);
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_ERR2, "error converting >>%s<<: %s", number, 
									rerr_getstr3(ret));
			return ret;
		}
		if (!resultonly) {
			printf ("%s = %d\n", number, n);
		}else {
			printf ("%d\n", n);
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

