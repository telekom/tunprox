#include <stdlib.h>
#include <stdio.h>
#include "addr.h"
#include <fr/base.h>

int
main (argc, argv)
	int	argc;
	char	**argv;
{
	frad_t	addr;
	int		ret;
	char		buf[256];

	if (argc < 2) return 0;
	ret = frad_getaddr (&addr, argv[1], 0);
	if (!RERR_ISOK(ret)) {
		SLOGFE (LOG_ERR2, "error in address: %s", rerr_getstr3(ret));
		return ret;
	}
	ret = frad_sprint (buf, sizeof (buf), &addr);
	if (!RERR_ISOK(ret)) {
		SLOGFE (LOG_ERR2, "error in printing: %s", rerr_getstr3(ret));
		return ret;
	}
	printf ("%d: %s\n", ret, buf);
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
