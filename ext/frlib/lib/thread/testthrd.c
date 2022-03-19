#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#include <fr/base.h>
#include <fr/thread.h>

int
myinit ()
{
	printf ("init by %d\n", frthr_self->fid);
	return 0;
}

int
mycleanup ()
{
	printf ("bye bye\n");
	return 0;
}

int
myrun ()
{
	int	i;

	printf ("%d: num: %d\n", frthr_self->id, frthr_self->iarg[0]);
	for (i=0; i<frthr_self->iarg[0] && i<3; i++) {
		printf ("    arg[%d] = %s\n", i, (char*)frthr_self->parg[i]);
	}
	return 0;
}


int
main (argc, argv)
	int	argc;
	char	**argv;
{
	struct frthr thr = (struct frthr) {
		.name = "test",
		.main = myrun,
		.init = myinit,
		.cleanup = mycleanup,
		.iarg[0] = argc-1,
	};
	int	i, ret;

	for (i=1; i<4 && i<argc; i++) {
			thr.parg[i-1] = argv[i];
	}
	frthr_init ();
	printf ("%d go\n", frthr_self->id);
	ret = frthr_start (&thr, 1000000LL);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR2, "error in frthr_start: %s\n", rerr_getstr3(ret));
		return ret;
	}
	sleep (2);
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
