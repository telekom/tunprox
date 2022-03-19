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
 * Portions created by the Initial Developer are Copyright (C) 2003-2017
 * by the Initial Developer. All Rights Reserved.
 *
 * ***** END LICENSE BLOCK ***** */


#include <fr/autogen/config.h>

#ifndef CONFIG_STACKTRACE

#include "errors.h"
#include "stacktrace.h"

int
stktr_prtstack (
	int	fd,
	int	flags,
	void	*ptr)
{
	return RERR_OK;
}

#else	/* CONFIG_STACKTRACE */



/*
 * The code is a modified version of a stack trace implementation 
 * by Jaco Kroon <jaco@kroon.co.za>
 * The original code and an explanation you'll find in stacktrace.txt
 */

/*
 * Due to a bug in gcc-4.x.x you currently have to compile as C++ if you want
 * demangling to work.
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

/* Bug in gcc prevents from using CPP_DEMANGLE in pure "C" */
#if !defined __cplusplus && !defined DEMANGLE_BUG && !defined NO_DEMANGLE_BUG
#define DEMANGLE_BUG
#endif

#include <memory.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include "frcontext.h"
#include <dlfcn.h>

#ifndef  DEMANGLE_BUG
# include <cxxabi.h>
#else
# ifdef __cplusplus
	namespace __cxxabiv1::__cxa_demangle {
     char* __cxa_demangle(const char*, char*, size_t*, int*);
	}
	using __cxxabiv1::__cxa_demangle;
# else
   char* __cxa_demangle(const char*, char*, size_t*, int*);
# endif
#endif

#include "stacktrace.h"
#include "errors.h"
#include "prtf.h"

#if defined(REG_RIP)
# define STKTR_STACK_IA64
# define REGFORMAT "%016lx"
#elif defined(REG_EIP)
# define STKTR_STACK_X86
# define REGFORMAT "%08x"
#else
# define STKTR_STACK_GENERIC
# define REGFORMAT "%x"
#endif

#if defined(STKTR_STACK_IA64) || defined(STKTR_STACK_X86)
# define DEDICATED
#endif

#ifdef DEDICATED
# include "cxademangle.h"
#else
# include <execinfo.h>
#endif



#define stkout(x,...)	fdprtf(fd, x "\n", ##__VA_ARGS__)




#ifdef __cplusplus
extern "C" {
#endif

int
stktr_prtstack (
	int	fd,
	int	flags,
	void	*ptr)
{
	int 			i;
	ucontext_t	*ctxp;
	ucontext_t	uctxbuf;
#ifdef DEDICATED
	int			f=0;
	Dl_info		dlinfo;
	void			**bp = NULL;
	void			*ip = NULL;
	char			*symname;
	char			*tmp;
	int			status;
#else
	int			sz;
	void			*bt[20];
	char			**strings;
#endif

	if (ptr) {
		ctxp = (ucontext_t*)ptr;
	} else {
		ctxp = &uctxbuf;
		if (getcontext (ctxp) < 0) return RERR_SYSTEM;
	}

#ifdef DEDICATED
	if (flags & STKTR_F_PRTREG) {
		stkout ("Register dump:");
		for(i = 0; i < NGREG; i++) {
			stkout ("   reg[%02d]       = 0x" REGFORMAT, i, 
						ctxp->uc_mcontext.gregs[i]);
		}
	}
#endif

	if (!(flags & STKTR_F_NOSTACK)) {
#ifdef DEDICATED
# if defined(STKTR_STACK_IA64)
		ip = (void*)ctxp->uc_mcontext.gregs[REG_RIP];
		bp = (void**)ctxp->uc_mcontext.gregs[REG_RBP];
# elif defined(STKTR_STACK_X86)
		ip = (void*)ctxp->uc_mcontext.gregs[REG_EIP];
		bp = (void**)ctxp->uc_mcontext.gregs[REG_EBP];
# else
#  error unknown stack format
# endif

		stkout("Stack trace:");
		while(bp && ip) {
			if(!dladdr(ip, &dlinfo)) break;

			symname = (char*)dlinfo.dli_sname;
			tmp = cxademangle(symname, NULL, 0, &status);
			//tmp = __cxa_demangle(symname, NULL, 0, &status);
			if (status == 0 && tmp) symname = tmp;

			stkout("   % 2d: %p <%s+%lu> (%s)",
						++f,
						ip,
						symname,
						(unsigned long)ip - (unsigned long)dlinfo.dli_saddr,
						dlinfo.dli_fname);
	
			if (tmp) free(tmp);

			if(dlinfo.dli_sname && !strcmp(dlinfo.dli_sname, "main")) break;

			ip = bp[1];
			bp = (void**)bp[0];
		}
#else		/* DEDICATED */
		stkout("Stack trace (non-dedicated):");
		sz = backtrace(bt, 20);
		strings = backtrace_symbols(bt, sz);
		for(i = 0; i < sz; ++i) {
			stkout("   %s", strings[i]);
		}
		free (strings);
#endif	/* DEDICATED */
		stkout("End of stack trace.");
	}
	return RERR_OK;
}











#ifdef __cplusplus
}	/* extern "C" */
#endif


#endif	/* CONFIG_STACKTRACE */









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
