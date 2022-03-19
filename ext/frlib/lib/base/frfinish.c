#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdarg.h>
#include <stdint.h>
#include <unistd.h>



#include "frfinish.h"
#include "tlst.h"
#include "errors.h"
#include "slog.h"
#include "frinit.h"
#include "prtf.h"


struct func_t {
	frfinishfunc_t	func;
	void				*arg;
	uint32_t			onfinish:1,
						ondie:1,
						noauto:1,
						once:1,
						isfile:1;
};
	

static struct tlst	finishfuncs;
static int				isinit = 0;
static int				noexec = 0;

static void doinit();
static int doexec (int);
static void callonexit (int, void*);


int
frregfinish (func, flags, arg)
	frfinishfunc_t	func;
	int				flags;
	void				*arg;
{
	struct func_t	sfunc;
	int				ret;

	if (!func) return RERR_PARAM;
	if (!isinit) doinit();
	bzero (&sfunc, sizeof (struct func_t));
	sfunc.func = func;
	if (flags & FRFINISH_F_ONFINISH) sfunc.onfinish = 1;
	if (flags & FRFINISH_F_ONDIE) sfunc.ondie = 1;
	if (!sfunc.onfinish && !sfunc.ondie) {
		sfunc.onfinish = 1;
		sfunc.ondie = 1;
	}
	if (flags & FRFINISH_F_NOAUTO) sfunc.noauto = 1;
	if (flags & FRFINISH_F_ONCE) sfunc.once = 1;
	if (flags & FRFINISH_F_ISFILE) sfunc.isfile=1;
	sfunc.arg = arg;
	ret = TLST_ADD(finishfuncs,sfunc);
	if (!RERR_ISOK(ret)) return ret;
	return RERR_OK;
}

int
frunregfinish (func)
	frfinishfunc_t	func;
{
	struct func_t	*p;
	unsigned			i;

	if (!func) return RERR_PARAM;
	if (!isinit) doinit();
	TLST_FOREACHPTR2 (p, finishfuncs, i) {
		if (p->func == func) {
			return TLST_REMOVE (finishfuncs, i, TLST_F_SHIFT);
		}
	}
	return RERR_NOT_FOUND;
}

int
frregfile (file, flags)
	const char	*file;
	int			flags;
{
	if (!file) return RERR_PARAM;
	return frregfinish ((frfinishfunc_t)(void*)file, flags|FRFINISH_F_ISFILE, NULL);
}

int
frunregfile (file)
	const char	*file;
{
	if (!file) return RERR_PARAM;
	return frunregfinish ((frfinishfunc_t)(void*)file);
}


int
frfinish ()
{
	return doexec (FRFINISH_F_ONFINISH | FRFINISH_F_NOAUTO);
}

int
frcalldie ()
{
	return doexec (FRFINISH_F_ONDIE | FRFINISH_F_NOAUTO);
}

void
frdie (ret, msg)
	int			ret;
	const char	*msg;
{
	const char	*prog;

	doexec (FRFINISH_F_ONDIE | FRFINISH_F_NOAUTO);
	if (msg) {
		prog = fr_getprog ();
		if (!prog) prog="ME";
		fprintf (stderr, "%s: %s", prog, msg);
		FRLOGF (LOG_CRIT, "%s: %s", msg, rerr_getstr3 (ret));
	} else {
		FRLOGF (LOG_CRIT, "error: %s", rerr_getstr3(ret));
	}
	noexec = 1;
	exit (ret);
}

void
frdief (
	int			ret,
	const char	*fmt,
	...)
{
	va_list	ap;

	va_start (ap, fmt);
	vfrdief (ret, fmt, ap);
	va_end (ap);
}

void
vfrdief (ret, fmt, ap)
	int			ret;
	const char	*fmt;
	va_list		ap;
{
	char	*msg, buf[1024];

	msg = va2sprtf (buf, sizeof (buf), fmt, ap);
	frdie (ret, msg);
	/* normally we don't reach here ... */
	if (msg && msg != buf) free (msg);
}



static
void
callonexit (ret, arg)
	int	ret;
	void	*arg;
{
	if (!ret) {
		doexec (FRFINISH_F_ONFINISH);
	} else {
		doexec (FRFINISH_F_ONDIE);
		FRLOGF (LOG_ERR, "exiting with error code %d", ret);
	}
	noexec = 1;
}

static
__attribute__((destructor)) 
void
callonfini ()
{
	doexec (FRFINISH_F_ONDIE);
	noexec = 1;
}


static
int
doexec (flags)
	int	flags;
{
	struct func_t	*p;
	unsigned			i;
	int				isauto=0, isfinish=0, isdie=0, isonce=0;
	int				ret;

	if (noexec) return RERR_OK;
	if (!isinit) doinit();
	if (flags & FRFINISH_F_ONFINISH) isfinish=1;
	if (flags & FRFINISH_F_ONDIE) isdie=1;
	if (!(flags & FRFINISH_F_NOAUTO)) isauto=1;
	if (flags & FRFINISH_F_ONCE) isonce=1;
	flags &= ~FRFINISH_F_ONCE;

	TLST_FOREACHPTR2 (p, finishfuncs, i) {
		if (isauto && p->noauto) continue;
		if (!((isfinish && p->onfinish) || (isdie && p->ondie))) continue;
		if (p->isfile) {
			unlink ((const char *)(void*)p->func);
		} else {
			p->func (flags, p->arg);
		}
		if (p->once || isonce || p->isfile) {
			ret = TLST_REMOVE (finishfuncs, i, TLST_F_SHIFT);
			if (RERR_ISOK(ret)) i--;	/* might underflow */
		}
	}
	return RERR_OK;
}


static
void
doinit ()
{
	if (isinit) return;
	TLST_NEW(finishfuncs,struct func_t);
	on_exit (callonexit, NULL);
	isinit = 1;
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
