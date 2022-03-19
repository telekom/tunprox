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
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <errno.h>
#ifdef SunOS
extern int errno;
#endif
#include <sys/types.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/stat.h>



#include <fr/base/config.h>
#include <fr/base/slog.h>
#include <fr/base/tmo.h>
#include <fr/base/textop.h>
#include <fr/base/errors.h>
#include <fr/base/strcase.h>
#include <fr/base/dlrt.h>
#include "shm.h"


static int sm_max_wait = 3000;
static int config_read = 0;
static int page_size = 4096;
static const char	*vol_dir = "/dev/shm/";
static const char	*perm_dir = "/var/permshm/";

static int read_config ();

struct fpage {
	sm_fpage_t		*info;
	void				*base;
	int				my;
	struct fpage	*left, *right, *bleft, *bright, *father, *bfather;
};
//#define FPAGE_NULL		((struct fpage){NULL, NULL, 0, NULL, NULL, NULL, NULL})
static struct fpage	*fpage_head = NULL;
static struct fpage	*startpage = NULL;

struct spage {
	sm_page_t		*info;
	void				*base;
	int				my;
	struct spage	*left, *right, *bleft, *bright, *father, *bfather;
};
#define SPAGE_NULL		((struct spage){.info = NULL})
static struct spage	*spage_head = NULL;

/* checking for pid is not a good idea, because it creates problems with
 * multithreading and forking, therefore we do use page->my
 */
//#define ISMY(page)	((page)&&((page)->info)&&(((page)->info->cpid) == getpid ()))
//#define ISMY(page)	((page)&&((page)->my))
#define ISMY(page) 1		/* always free */

static struct spage *sm_page_struct (int id);
static struct spage *page_search_id (struct spage *, int);
static struct spage *page_search_base (struct spage *, void*);
static int page_insert (struct spage *);
static int page_insert_id (struct spage *, struct spage*);
static int page_insert_base (struct spage *, struct spage*);
static int do_del (int, struct spage*, int);
static int do_delall (struct spage*, int);
static int do_delstart (int);
static int check_pname (char*, int);
static int mk_fname (char**, const char*, int);
static int fpage_unlink2 (void*, size_t, char*, int);
static int fpage_unlink (char*, int);
static int fpage_open (char*, int, int);
static int fpage_check_destroy (struct fpage*);
static int fpage_destroy (struct fpage*);
static int fpage_dodestroy (struct fpage*);
static int fpage_rm (struct fpage*);
static int fpage_createinsert (void*, int);
static int fpage_insert (struct fpage*);
static int fpage_insert_name (struct fpage*, struct fpage*);
static int fpage_insert_base (struct fpage*, struct fpage*);
static int fpage_deinsert (struct fpage*);
static int my_getpname (char*, const char*);
static struct fpage* fpage_search_name (struct fpage*, const char*);
static struct fpage* fpage_search_base (struct fpage*, void*);
static int fpage_search_file (int*, const char*);
static int check_tfile (const char*, int);
static int check_shmfile (const char*);
#define CF_FTYPE_REG		0
#define CF_FTYPE_DIR		1
#define CF_FTYPE_ANY		2
static int check_file (const char*, int);



/***************************************
 * fpage functions
 ***************************************/



int
sm_fopen (name, size, flags)
	const char	*name;
	int			size;
	int			flags;
{
	char			pname[64];
	int			sflags;
	int			fd;
	int			psize;
	char			buf[1024];
	void			*addr;
	sm_fpage_t	spage, *ppage;
	int			i, num, num2;
	int			ret;

	CF_MAY_READ;
	ret = sm_fpage_isopen (name);
	if (RERR_ISOK(ret)) return RERR_ALREADY_OPEN;
	if (ret != RERR_NOT_FOUND) return ret;
	ret = my_getpname (pname, name);
	if (!RERR_ISOK(ret)) return ret;
	psize = size>=0 ? size : cf_atoi (cf_getarr2 ("sm_page_size",pname,"65536"));
	psize+=sizeof (sm_fpage_t);
	num = psize % page_size;
	if (num) psize += page_size-num;
	if (flags & SM_F_RECREAT) flags |= SM_F_CREAT;
	if (flags & SM_F_CREAT) {
		sflags = O_RDWR | O_CREAT;
		if (flags & SM_F_RECREAT) {
			sflags |= O_TRUNC;
		} else {
			sflags |= O_EXCL;
		}
	} else if (flags & SM_F_RDONLY) {
		sflags = O_RDONLY;
	} else {
		sflags = O_RDWR;
	}
	fd = fpage_open (pname, sflags, flags);
	if (fd==RERR_SYSTEM && !(flags & SM_F_CREAT)) {
		if (sm_max_wait < 0) {
			i=-1;
		} else if (sm_max_wait == 0) {
			i=0;
		} else if (sm_max_wait < 100) {
			i=1;
		} else {
			i=sm_max_wait/100;
		}
		while (i && fd==RERR_SYSTEM) {
			tmo_msleep (100);
			if (i>0) i--;
			fd = fpage_open (pname, sflags, flags);
		}
	}
	if (fd<0) {
		FRLOGF (LOG_ERR, "cannot open shared memory >>%s<<: %s", pname,
								rerr_getstr3 (fd));
		return fd;
	}
	if (flags & SM_F_CREAT) {
		bzero (buf, sizeof(buf));
		for (i=0; i+(ssize_t)sizeof(buf)<=psize; i+=sizeof(buf)) {
			num = write (fd, buf, sizeof(buf));
			if (num != sizeof (buf)) {
				if (num<0) {
					FRLOGF (LOG_ERR, "error writing to shared memory file >>%s<<"
										": %s", pname, rerr_getstr3(RERR_SYSTEM));
				} else {
					FRLOGF (LOG_ERR, "truncated write to shared memory file "
										">>%s<< (%d bytes written)", pname, num);
				}
				close (fd);
				fpage_unlink (pname, flags);
				return RERR_SYSTEM;
			}
		}
		num2 = psize%sizeof(buf);
		if (num2>0) {
			num = write (fd, buf, num2);
			if (num != num2) {
				if (num<0) {
					FRLOGF (LOG_ERR, "error writing to shared memory file "
										">>%s<<: %s", pname, rerr_getstr3(RERR_SYSTEM));
				} else {
					FRLOGF (LOG_ERR, "truncated write to shared memory file "
										">>%s<< (%d bytes written)", pname, num);
				}
				close (fd);
				fpage_unlink (pname, flags);
				return RERR_SYSTEM;
			}
		}
	} else {
		if (sm_max_wait < 0) {
			i=-1;
		} else if (sm_max_wait < 100) {
			i=1;
		} else {
			i=sm_max_wait/100;
		}
		while (1) {
			num = read (fd, &spage, sizeof (spage));
			lseek (fd, 0, SEEK_SET);
			if ((num == sizeof(spage)) && spage.flags.valid) break;
			if (i>0) i--;
			if (!i) break;
			tmo_msleep (100);
		}
		if (!i) {
			FRLOGF (LOG_ERR, "timed out while waiting for shared memory >>%s<<<",
									pname);
			close (fd);
			return RERR_TIMEDOUT;
		}
		if (spage.magic != SM_MAGIC_FPAGE) {
			FRLOGF (LOG_ERR, "invalid shared memory >>%s<< found", pname);
			close (fd);
			return RERR_INVALID_SHM;
		}
		psize = spage.size;
	}
	addr = mmap (NULL, psize, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	if (addr == MAP_FAILED) {
		FRLOGF (LOG_ERR, "error mapping shared memory >>%s<<: %s", pname,
								rerr_getstr3(RERR_SYSTEM));
		close (fd);
		fpage_unlink (pname, flags);
		return RERR_SYSTEM;
	}
	close (fd);
	ppage = (sm_fpage_t*)addr;
	if (flags & SM_F_CREAT) {
		ppage->flags.cp = 0;
		ppage->magic = SM_MAGIC_SPAGE;
		strncpy (ppage->name, pname, 64);
		ppage->name[63] = 0;
		ppage->size = psize - sizeof (sm_fpage_t);
		ppage->offset = sizeof (sm_fpage_t);
		ppage->cpid = getpid();
		ppage->ctime = tmo_now();
		ppage->flags.type = flags & SM_T_MASK;
	}
	/* need to be modified */
	ret = fpage_createinsert (addr, flags);
	if (ret != 0) {
		fpage_unlink2 (addr, psize, pname, flags);
		return ret;
	}
	if (flags & SM_F_CREAT) {
		ppage->flags.valid = 1;
	}
	return RERR_OK;
}



int
sm_getpname (outname, pname)
	char			**outname;
	const char	*pname;
{
	char			*pn=NULL;
	const char	*pc;
	int			ret = RERR_OK;

	if (!outname) return RERR_PARAM;
	if (!pname || *pname == '@') {
		CF_MAY_READ;
		cf_begin_read ();
		pc = cf_getarr2 ("sm_page_name", pname, pname);
		if (pn && *pn == '@') {
			pname++;
			pname = cf_getarr2 ("sm_page_name", pname, pname);
		} else {
			pname = pc;
		}
		if (pname) {
			pn = strdup (pname);
			if (!pn) ret = RERR_NOMEM;
		} else {
			pn = NULL;
		}
		cf_end_read ();
	} else {
		pn = strdup (pname);
		if (!pn) ret = RERR_NOMEM;
	}
	if (!RERR_ISOK(ret)) return ret;
	if (!pn) return RERR_INVALID_NAME;
	ret = check_pname (pn, 1);
	if (!RERR_ISOK(ret)) {
		free (pn);
		return ret;
	}
	*outname = pn;
	return RERR_OK;
}


int
sm_fclose (name, flags)
	const char	*name;
	int			flags;
{
	char				pname[64];
	int				ret;
	struct fpage	*fpage;

	CF_MAY_READ;
	ret = my_getpname (pname, name);
	if (!RERR_ISOK(ret)) return ret;
	fpage = fpage_search_name (fpage_head, pname);
	if (!fpage) return RERR_INVALID_NAME;
	if (flags & SM_F_DESTROY) {
		ret = fpage_destroy (fpage);
	} else {
		ret = fpage_rm (fpage);
	}
	if (!RERR_ISOK(ret)) return ret;
	return RERR_OK;
}

sm_fpage_t *
sm_fpage_getinfo (name)
	const char	*name;
{
	struct fpage	*fpage;
	char				pname[64];
	int				ret;

	CF_MAY_READ;
	ret = my_getpname (pname, name);
	if (!RERR_ISOK(ret)) return NULL;
	fpage = fpage_search_name (fpage_head, pname);
	if (!fpage) return NULL;
	return fpage->info;
}


void *
sm_fpage_base (name)
	const char	*name;
{
	struct fpage	*fpage;
	char				pname[64];
	int				ret;

	CF_MAY_READ;
	ret = my_getpname (pname, name);
	if (!RERR_ISOK(ret)) return NULL;
	fpage = fpage_search_name (fpage_head, pname);
	if (!fpage) return NULL;
	return fpage->base;
}


int 
sm_fpage_size (name)
	const char	*name;
{
	struct fpage	*fpage;
	char				pname[64];
	int				ret;

	CF_MAY_READ;
	ret = my_getpname (pname, name);
	if (!RERR_ISOK(ret)) return ret;
	fpage = fpage_search_name (fpage_head, pname);
	if (!fpage || !fpage->info) return RERR_NOT_FOUND;
	return fpage->info->size;
}

const char*
sm_fpage_getpage (addr)
	void	*addr;
{
	struct fpage	*fpage;

	if (!addr) return NULL;
	fpage = fpage_search_base (fpage_head, addr);
	if (!fpage || !fpage->info) return NULL;
	return fpage->info->name;
}


int
sm_fpage_isopen (name)
	const char	*name;
{
	struct fpage	*fpage;
	char				pname[64];
	int				ret;

	CF_MAY_READ;
	ret = my_getpname (pname, name);
	if (!RERR_ISOK(ret)) return ret;
	fpage = fpage_search_name (fpage_head, pname);
	if (!fpage || !fpage->info) return RERR_NOT_FOUND;
	return RERR_OK;
}


int
sm_fpage_exist (otype, name)
	int			*otype;
	const char	*name;
{
	char	pname[64];
	int	ret;

	CF_MAY_READ;
	ret = my_getpname (pname, name);
	if (!RERR_ISOK(ret)) return ret;
	return fpage_search_file (otype, pname);
}


/****************************
 * startpage functions
 ****************************/


int
sm_start (name, size, flags)
	const char	*name;
	int			size;
	int			flags;
{
	int	ret;

	ret = sm_fopen (name, size, flags | SM_F_STARTPAGE);
	if (!RERR_ISOK(ret)) {
		return ret;
	}
	return RERR_OK;
}



sm_fpage_t *
sm_startpage_getinfo ()
{
	if (!startpage) return NULL;
	return startpage->info;
}

void*
sm_startpage_base()
{
	if (!startpage) return NULL;
	return startpage->base;
}

int
sm_startpage_size()
{
	if (!startpage && !startpage->info) return RERR_INVALID_SHM;
	return startpage->info->size;
}

int
sm_delstart ()
{
	return do_delstart (0);
}

int
sm_stop ()
{
	int	ret, ret2;

	ret = do_delall (spage_head, 0);
	ret2 = do_delstart (0);
	if (ret2 != RERR_OK) ret = ret2;
	return ret;
}

int
sm_maydel ()
{
	int	ret, ret2;

	ret = do_delall (spage_head, 1);
	ret2 = do_delstart (1);
	if (ret2 != RERR_OK) ret = ret2;
	return ret;
}

/***********************************
 * page functions
 ***********************************/


int
sm_new (id, size)
	int	*id;
	int	size;
{
	int				id2, psize, num;
	struct spage	*spage;
	sm_page_t		*ppage;
	void				*addr;
	int				ret;

	if (size<=0) return RERR_PARAM;
	if (id) *id=0;
	spage = malloc (sizeof (struct spage));
	if (!spage) return RERR_NOMEM;
	*spage = SPAGE_NULL;
	psize=size + sizeof (sm_page_t);
	num = psize % page_size;
	if (num) psize += page_size-num;
	id2 = shmget (IPC_PRIVATE, psize, 0660|IPC_CREAT);
	if (id2<0) {
		FRLOGF (LOG_ERR, "error in shmget(): %s", rerr_getstr3(RERR_SYSTEM));
		free (spage);
		return RERR_SYSTEM;
	}
	addr = shmat (id2, NULL, 0);
	if (addr == (void*)-1) {
		FRLOGF (LOG_ERR, "error in shmat(): %s", rerr_getstr3(RERR_SYSTEM));
		shmctl(id2, IPC_RMID, NULL);
		free (spage);
		return RERR_SYSTEM;
	}
	spage->info = ppage = (sm_page_t*) addr;
	spage->base = (char*)addr + sizeof (sm_page_t);
	spage->my = 1;
	ppage->magic = SM_MAGIC_PAGE;
	ppage->id = id2;
	ppage->size = psize-sizeof(sm_page_t);
	ppage->offset = sizeof (sm_page_t);
	ppage->cpid = getpid();
	ppage->ctime = tmo_now();
	ppage->flags.deleted = 0;
	ret = page_insert (spage);
	if (!RERR_ISOK(ret)) {
		ppage->flags.deleted = 1;
		shmdt (addr);
		shmctl(id2, IPC_RMID, NULL);
		free (spage);
		return ret;
	}
	ppage->flags.valid = 1;
	if (id) *id = id2;
	return RERR_OK;
}


int
sm_del (id)
	int	id;
{
	return do_del (id, NULL, 0);
}


int
sm_delall ()
{
	return do_delall (spage_head, 0);
}


void*
sm_ptmap (pt)
	sm_pt	pt;
{
	void	*base;

	base = sm_page_base(SM_PT_SMS(pt));
	if (!base) return NULL;
	return (char*)base+(size_t)SM_PT_OFF(pt);
}

void*
sm_ptmap2 (pt)
	sm_pt	*pt;
{
	sm_pt	pt2;

	if (!pt) return NULL;
	sm_ptcp (&pt2, *pt);
	return sm_ptmap (pt2);
}

sm_pt
sm_ptrev (p)
	void	*p;
{
	struct spage	*page;

	if (!p) return SM_PT_NULL;
	page = page_search_base (spage_head, p);
	if (!page || !page->info || !page->base) return SM_PT_NULL;
	return SM_PT_MK(page->info->id,(ssize_t)((char*)p-(char*)page->base));
}


void
sm_ptcp (dest, src)
	sm_pt	*dest, src;
{
	if (!dest) return;
	/* on modern 64bit architectures the copy is atomic, on 32bit architectures
		we should use assembler routines here to get atomicity - to be done...
    */
	*dest = src;
}

void
sm_ptset (dest, src)
	sm_pt	*dest;
	void	*src;
{
	if (!dest) return;
	sm_ptcp (dest, sm_ptrev (src));
}



sm_page_t *
sm_page_getinfo (id)
	int	id;
{
	struct spage	*p;
	p = sm_page_struct (id);
	if (!p) return NULL;
	return p->info;
}

void*
sm_page_base(id)
	int	id;
{
	struct spage	*p;
	p = sm_page_struct (id);
	if (!p) return NULL;
	return p->base;
}

int
sm_page_size(id)
	int	id;
{
	struct spage	*p;
	p = sm_page_struct (id);
	if (!p) return -1;
	return p->info->size;
}


/*******************************
 *******************************
 ** static functions
 *******************************
 *******************************/


/*******************************
 * static page functions
 *******************************/

static
struct spage *
sm_page_struct (id)
	int	id;
{
	struct spage	*p;
	void				*addr;
	int				ret;

	if (id<0) return NULL;
	p = page_search_id (spage_head, id);
	if (p) return p;
	p = malloc (sizeof (struct spage));
	if (!p) return NULL;
	*p = SPAGE_NULL;
	addr = shmat (id, NULL, 0);
	if (addr == (void*)-1) {
		FRLOGF (LOG_ERR, "error attaching shared memory segment %d: %s", id,
								rerr_getstr3(RERR_SYSTEM));
		free (p);
		return NULL;
	}
	p->info = (sm_page_t*)addr;
	p->base = ((char*)addr)+p->info->offset;
	ret = page_insert (p);
	if (!RERR_ISOK(ret)) {
		shmdt (addr);
		free (p);
		return NULL;
	}
	return p;
}

static
struct spage *
page_search_id (head, id)
	struct spage	*head;
	int				id;
{
	if (!head) return NULL;
	if (!head->info) return NULL;
	if (head->info->id == id) return head;
	if (id<head->info->id) return page_search_id(head->left, id);
	return page_search_id (head->right, id);
}

static
struct spage *
page_search_base (head, base)
	struct spage	*head;
	void				*base;
{
	if (!head || !base) return NULL;
	if (!head->base || !head->info) return NULL;
	if (base < head->base) return page_search_base(head->left, base);
	if ((char*)base < (char*)head->base + head->info->size) return head;
	return page_search_base (head->right, base);
}


static
int
page_insert (p)
	struct spage	*p;
{
	int		ret;

	if (!p || !p->info || !p->base) return RERR_PARAM;
	p->father = p->bfather = p->left = p->right = p->bleft = p->bright = NULL;
	if (!spage_head) {
		spage_head = p;
		return RERR_OK;
	}
	ret = page_insert_id (spage_head, p);
	if (!RERR_ISOK(ret)) {
		shmdt ((void*)(p->info));
		free (p);
		return ret;
	}
	ret = page_insert_base (spage_head, p);
	if (!RERR_ISOK(ret)) {
		shmdt ((void*)(p->info));
		free (p);
		return ret;
	}
	return RERR_OK;
}

static
int
page_insert_id (head, p)
	struct spage	*p, *head;
{
	if (!head || !p) return RERR_PARAM;
	if (p->info->id <= head->info->id) {
		if (head->left) return page_insert_id (head->left, p);
		head->left = p;
		p->father = head;
		return RERR_OK;
	}
	if (head->right) return page_insert_id (head->right, p);
	head->right = p;
	p->father = head;
	return RERR_OK;
}

static
int
page_insert_base (head, p)
	struct spage	*p, *head;
{
	if (!head || !p) return RERR_PARAM;
	if (p->base <= head->base) {
		if (head->bleft) return page_insert_base (head->bleft, p);
		head->bleft = p;
		p->bfather = head;
		return RERR_OK;
	}
	if (head->bright) return page_insert_base (head->bright, p);
	head->bright = p;
	p->bfather = head;
	return RERR_OK;
}

static
int
page_unlink (p)
	struct spage	*p;
{
	struct spage	*next;
	int				ret;

	if (!p) return RERR_PARAM;
	if (p->left && p->right) {
		ret = page_insert_id (p->left, p->right);
		if (!RERR_ISOK(ret)) return ret;
		p->right = NULL;
		next = p->left;
	} else if (p->left) {
		next = p->left;
	} else {
		next = p->right;
	}
	if (p==spage_head) {
		spage_head = next;
	} else {
		if (p == p->father->left) {
			p->father->left = next;
		} else {
			p->father->right = next;
		}
	}
	if (p->bleft && p->bright) {
		ret = page_insert_base (p->bleft, p->bright);
		if (!RERR_ISOK(ret)) return ret;
		p->bright = NULL;
		next = p->bleft;
	} else if (p->bleft) {
		next = p->bleft;
	} else {
		next = p->bright;
	}
	if (p->bfather) {
		if (p == p->bfather->bleft) {
			p->bfather->bleft = next;
		} else {
			p->bfather->bright = next;
		}
	} else if (spage_head && spage_head != next) {
		if (spage_head->bfather) {
			if (spage_head->bfather->bleft == spage_head) {
				spage_head->bfather->bleft = NULL;
			} else {
				spage_head->bfather->bright = NULL;
			}
			spage_head->bfather=NULL;
		}
		ret = page_insert_base (spage_head, next);
		if (!RERR_ISOK(ret)) return ret;
	}
	p->left = p->right = p->bleft = p->bright = p->father = p->bfather = NULL;
	return RERR_OK;
}



static
int
do_del (id, p, may)
	int				id, may;
	struct spage	*p;
{
	int	ret;

	if (!p) p = page_search_id (spage_head, id);
	if (!p) return RERR_NOT_FOUND;
	if (!p->info->flags.valid) return RERR_OK;
	if (may && !p->info->flags.deleted) return RERR_OK;
	if (ISMY(p)) {
		p->info->flags.deleted=1;
	}
	ret = page_unlink (p);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error unlinking page %d: %s", id, rerr_getstr3 (ret));
		return ret;
	}
	shmdt ((void*)p->info);
	if (ISMY(p)) {
		shmctl (id, IPC_RMID, NULL);
	}
	*p = SPAGE_NULL;
	free (p);
	return RERR_OK;
}

static
int
do_delall (head, may)
	struct spage	*head;
	int				may;
{
	int	ret, ret2;

	if (!head) return RERR_OK;
	ret = do_delall (head->left, may);
	ret2 = do_delall (head->right, may);
	if (ret2 != RERR_OK) ret = ret2;
	ret2 = do_del (head->info->id, head, may);
	if (ret2 != RERR_OK) ret = ret2;
	return ret;
}

/***************************************
 * static startpage functions
 ***************************************/


static
int
do_delstart (may)
	int		may;
{
	int	ret;

	if (!startpage) return RERR_OK;
	if (may) {
		ret = fpage_check_destroy (startpage);
	} else {
		ret = fpage_destroy (startpage);
	}
	if (!RERR_ISOK(ret)) return ret;
	startpage = NULL;
	return RERR_OK;
}


/***************************************
 * static fpage functions
 ***************************************/


static
int
fpage_check_destroy (fpage)
	struct fpage	*fpage;
{
	if (!fpage->info || !fpage->info->flags.valid) return RERR_OK;
	if (!fpage->info->flags.deleted) return RERR_OK;
	return fpage_destroy (fpage);
}


static
int
fpage_destroy (fpage)
	struct fpage	*fpage;
{
	int	ret;

	ret = fpage_dodestroy (fpage);
	if (!RERR_ISOK(ret)) return ret;
	return fpage_rm (fpage);
}

static
int
fpage_dodestroy (fpage)
	struct fpage	*fpage;
{
	if (!fpage) return RERR_PARAM;

	if (!fpage->info || !fpage->info->flags.valid) return RERR_OK;
	if (ISMY(fpage)) {
		if (fpage->info->flags.deleted) return RERR_OK;
		fpage->info->flags.deleted = 1;
		fpage_unlink (fpage->info->name, fpage->info->flags.type | SM_F_CREAT);
	}
	return RERR_OK;
}

static
int
fpage_rm (fpage)
	struct fpage	*fpage;
{
	int	ret;

	if (!fpage) return RERR_PARAM;
	if (fpage->info) {
		munmap (fpage->info, fpage->info->size+fpage->info->offset);
	}
	ret = fpage_deinsert (fpage);
	if (!RERR_ISOK(ret)) return ret;
	bzero (fpage, sizeof (struct fpage));
	free (fpage);
	return RERR_OK;
}


static
int
check_pname (name, correct)
	char	*name;
	int	correct;
{
	char	*s, *s2;

	if (!name || !*name) return RERR_PARAM;
	for (s=name; *s; s++) {
		if (*s=='/' && (s==name || s[-1]=='/')) {
			if (correct) {
				for (s2=s; s2[1]; s2++) *s2=s2[1];
				*s2=0;
				if (!*s) break;
			} else {
				return RERR_INVALID_NAME;
			}
		}
		if (*s >= 'A' && *s <= 'Z') {
			if (correct) {
				*s = *s - 'A' + 'a';
			} else {
				return RERR_INVALID_NAME;
			}
		} else if (*s >= '0' && *s <= '9') {
			if (s==name) return RERR_INVALID_NAME;
		} else if (!((*s >= 'a' && *s <= 'z') || *s == '_')) {
			return RERR_INVALID_NAME;
		}
	}
	if (strlen (name) > 62) return RERR_INVALID_NAME;
	return RERR_OK;
}

#ifndef MAX_PATH
//#  define MAX_PATH	_POSIX_PATH_MAX
#  define MAX_PATH	1024
#endif

static
int
mk_fname (fname, pname, type)
	char			**fname;
	const char	*pname;
	int			type;
{
	int	ret, len, len2;
	char	*fn;

	if (!fname || !pname) return RERR_PARAM;
	*fname = NULL;
	type &= SM_T_MASK;
	switch (type) {
	case SM_T_SHMOPEN:
		len=1;
		break;
	case SM_T_PERMANENT:
		len = strlen (perm_dir) + 1;
		break;
	case SM_T_VOLATILE:
		len = strlen (vol_dir) + 1;
		break;
	case SM_T_SEARCH:
		ret = fpage_search_file (&type, pname);
		if (!RERR_ISOK(ret)) return ret;
		if (type == SM_T_SEARCH) return RERR_NOT_FOUND;		/* avoid loop */
		return mk_fname (fname, pname, type);
	default:
		return RERR_PARAM;
	}
	if (len > MAX_PATH - 64) return RERR_CONFIG;
	len2 = strlen (pname) + 1;
	fn = *fname = malloc (len + len2 + 1);
	if (!fn) return RERR_NOMEM;
	switch (type) {
	case SM_T_SHMOPEN:
		sprintf (fn, "/%s", pname);
		break;
	case SM_T_PERMANENT:
		sprintf (fn, "%s/%s", perm_dir, pname);
		break;
	case SM_T_VOLATILE:
		sprintf (fn, "%s/%s", vol_dir, pname);
		break;
	}
	fn += len;
	ret = check_pname (fn, 1);
	if (!RERR_ISOK(ret)) {
		free (*fname);
		*fname = NULL;
		return ret;
	}
	if (type == SM_T_SHMOPEN) {
		for (; *fn; fn++) if (*fn == '/') *fn='%';
	}
#if 0
	if (strlen (*fname) > MAX_PATH) {
		free (*fname);
		*fname = NULL;
		return RERR_INTERNAL;
	}
#endif

	return RERR_OK;
}


static
int
fpage_search_file (otype, pname)
	int			*otype;
	const char	*pname;
{
	int	ret, type;

	if (!pname) return RERR_PARAM;
	once {
		ret = check_tfile (pname, (type=SM_T_PERMANENT));
		if (RERR_ISOK(ret)) break;
		ret = check_tfile (pname, (type=SM_T_VOLATILE));
		if (RERR_ISOK(ret)) break;
		ret = check_tfile (pname, (type=SM_T_SHMOPEN));
	}
	if (RERR_ISOK(ret) && otype) {
		*otype = type;
	}
	return ret;
}

static
int
check_tfile (pname, type)
	const char	*pname;
	int			type;
{
	int	ret;
	char	*fname;

	if (type == SM_T_SEARCH) return RERR_PARAM;	/* avoid loops */
	if (!pname) return RERR_PARAM;
	ret = mk_fname (&fname, pname, type);
	if (!RERR_ISOK(ret)) return ret;
	if (type == SM_T_SHMOPEN) {
		ret = check_shmfile (fname);	/* yes we want fname, not pname */
	} else {
		ret = check_file (fname, CF_FTYPE_REG);
	}
	free (fname);
	return ret;
}

static
int
check_shmfile (pname)
	const char	*pname;
{
	char	*fname;
	int	ret, fsize;

	ret = check_file ("/run/shm", CF_FTYPE_DIR);
	if (RERR_ISOK(ret)) {
		fsize = strlen (pname) + strlen ("/run/shm/") + 1;
		fname = malloc (fsize);
		if (!fname) return RERR_NOMEM;
		sprintf (fname, "/run/shm/%s", pname);
		ret = check_file (fname, CF_FTYPE_REG);
		free (fname);
		return ret;
	}
	ret = check_file ("/dev/shm", CF_FTYPE_DIR);
	if (RERR_ISOK(ret)) {
		fsize = strlen (pname) + strlen ("/dev/shm/") + 1;
		fname = malloc (fsize);
		if (!fname) return RERR_NOMEM;
		sprintf (fname, "/dev/shm/%s", pname);
		ret = check_file (fname, CF_FTYPE_REG);
		free (fname);
		return ret;
	}
	ret = EXTCALL(shmopen) (pname, O_RDONLY, 0400);
	if (ret >= 0) {
		close (ret);
		return RERR_OK;
	} else {
		return RERR_NOT_FOUND;
	}
	return RERR_INTERNAL;	/* to make compiler happy */
}


static
int
check_file (fname, ftype)
	const char	*fname;
	int			ftype;
{
	int			ret;
	struct stat	sbuf;

	if (!fname || !*fname) return RERR_PARAM;
	ret = stat (fname, &sbuf);
	if (ret < 0) return RERR_NOT_FOUND;
	switch (ftype) {
	case CF_FTYPE_ANY:
		return RERR_OK;
	case CF_FTYPE_REG:
		return S_ISREG (sbuf.st_mode) ? RERR_OK : RERR_NOT_FOUND;
	case CF_FTYPE_DIR:
		return S_ISDIR (sbuf.st_mode) ? RERR_OK : RERR_NOT_FOUND;
	default:
		return RERR_PARAM;
	}
	return RERR_OK;
}



static
int
fpage_unlink (pname, flags)
	char		*pname;
	int		flags;
{
	return fpage_unlink2 (NULL, 0, pname, flags);
}

static
int
fpage_unlink2 (addr, psize, pname, flags)
	void		*addr;
	char		*pname;
	size_t	psize;
	int		flags;
{
	char	*fname;
	int	type, ret;

	if (!pname) return RERR_PARAM;
	if (addr && psize > 0) munmap (addr, psize);
	if (flags & SM_F_CREAT) {
		type = flags & SM_T_MASK;
		ret = mk_fname (&fname, pname, type);
		if (!RERR_ISOK(ret)) return ret;
		if (type == SM_T_SHMOPEN) {
			ret = EXTCALL(shmunlink) (fname);
		} else {
			ret = unlink (fname);
		}
		free (fname);
		if (ret < 0) {
			FRLOGF (LOG_ERR, "error unlinking page >>%s<<: %s", pname,
									rerr_getstr3 (RERR_SYSTEM));
			return RERR_SYSTEM;
		}
	}
	return RERR_OK;
}

static
int
fpage_open (pname, sflags, flags)
	char	*pname;
	int	flags, sflags;
{
	int	type, fd, ret;
	char	*fname;

	type = flags & SM_T_MASK;
	ret = mk_fname (&fname, pname, type);
	if (!RERR_ISOK(ret)) return ret;
	if (type == SM_T_SHMOPEN) {
		fd = EXTCALL(shmopen) (fname, sflags, 0660);
	} else {
		fd = open (fname, sflags, 0660);
	}
	free (fname);
	if (fd < 0) return RERR_SYSTEM;
	return fd;
}


static
int
fpage_createinsert (addr, flags)
	void	*addr;
	int	flags;
{
	struct fpage	*p;
	int				ret;

	if (!addr) return RERR_PARAM;
	p = malloc (sizeof (struct fpage));
	if (!p) return RERR_NOMEM;
	bzero (p, sizeof (struct fpage));
	p->info = (sm_fpage_t*) addr;
	p->base = (char*)addr + p->info->offset;
	p->my = (flags & SM_F_CREAT) ? 1 : 0;
	ret = fpage_insert (p);
	if (!RERR_ISOK(ret)) {
		bzero (p, sizeof (struct fpage));
		free (p);
		return ret;
	}
	if (flags & SM_F_STARTPAGE) startpage = p;
	return RERR_OK;
}

static
int
fpage_insert (p)
	struct fpage	*p;
{
	int		ret;

	if (!p || !p->info || !p->base) return RERR_PARAM;
	p->father = p->bfather = p->left = p->right = p->bleft = p->bright = NULL;
	if (!spage_head) {
		fpage_head = p;
		return RERR_OK;
	}
	ret = fpage_insert_name (fpage_head, p);
	if (!RERR_ISOK(ret)) {
		return ret;
	}
	ret = fpage_insert_base (fpage_head, p);
	if (!RERR_ISOK(ret)) {
		if (p->father) {
			if (p->father->left == p) {
				p->father->left = NULL;
			} else if (p->father->right == p) {
				p->father->right = NULL;
			}
		}
		return ret;
	}
	return RERR_OK;
}

static
int
fpage_insert_name (head, p)
	struct fpage	*p, *head;
{
	if (!head || !p) return RERR_PARAM;
	if (strcmp (p->info->name, head->info->name) <= 0) {
		if (head->left) return fpage_insert_name (head->left, p);
		head->left = p;
		p->father = head;
		return RERR_OK;
	}
	if (head->right) return fpage_insert_name (head->right, p);
	head->right = p;
	p->father = head;
	return RERR_OK;
}

static
int
fpage_insert_base (head, p)
	struct fpage	*p, *head;
{
	if (!head || !p) return RERR_PARAM;
	if (p->base <= head->base) {
		if (head->bleft) return fpage_insert_base (head->bleft, p);
		head->bleft = p;
		p->bfather = head;
		return RERR_OK;
	}
	if (head->bright) return fpage_insert_base (head->bright, p);
	head->bright = p;
	p->bfather = head;
	return RERR_OK;
}


static
int
fpage_deinsert (p)
	struct fpage	*p;
{
	struct fpage	*next;
	int				ret;

	if (!p) return RERR_PARAM;
	if (p->left && p->right) {
		ret = fpage_insert_name (p->left, p->right);
		if (!RERR_ISOK(ret)) return ret;
		p->right = NULL;
		next = p->left;
	} else if (p->left) {
		next = p->left;
	} else {
		next = p->right;
	}
	if (p==fpage_head) {
		fpage_head = next;
	} else {
		if (p == p->father->left) {
			p->father->left = next;
		} else {
			p->father->right = next;
		}
	}
	if (p->bleft && p->bright) {
		ret = fpage_insert_base (p->bleft, p->bright);
		if (!RERR_ISOK(ret)) return ret;
		p->bright = NULL;
		next = p->bleft;
	} else if (p->bleft) {
		next = p->bleft;
	} else {
		next = p->bright;
	}
	if (p->bfather) {
		if (p == p->bfather->bleft) {
			p->bfather->bleft = next;
		} else {
			p->bfather->bright = next;
		}
	} else if (fpage_head && fpage_head != next) {
		if (fpage_head->bfather->bleft == fpage_head) {
			fpage_head->bfather->bleft = NULL;
		} else {
			fpage_head->bfather->bright = NULL;
		}
		fpage_head->bfather=NULL;
		ret = fpage_insert_base (fpage_head, next);
		if (!RERR_ISOK(ret)) return ret;
	}
	p->left = p->right = p->bleft = p->bright = p->father = p->bfather = NULL;
	if (p == startpage) startpage = NULL;
	return RERR_OK;
}


static
struct fpage *
fpage_search_name (head, name)
	struct fpage	*head;
	const char		*name;
{
	int	ret;

	if (!head || !name) return NULL;
	if (!head->info || !head->info->name) return NULL;
	ret = strcmp (name, head->info->name);
	if (ret == 0) return head;
	if (ret < 0) return fpage_search_name(head->left, name);
	return fpage_search_name (head->right, name);
}

static
struct fpage *
fpage_search_base (head, base)
	struct fpage	*head;
	void				*base;
{
	if (!head || !base) return NULL;
	if (!head->base || !head->info) return NULL;
	if (base < head->base) return fpage_search_base(head->left, base);
	if ((char*)base < (char*)head->base + head->info->size) return head;
	return fpage_search_base (head->right, base);
}


static
int
my_getpname (outname, pname)
	char			*outname;
	const char	*pname;
{
	int	ret;
	char	*xn;

	if (!outname) return RERR_PARAM;
	ret = sm_getpname (&xn, pname);
	if (!RERR_ISOK(ret)) return ret;
	if (strlen (xn) > 63) {
		free (xn);
		return RERR_INTERNAL;
	}
	strcpy (outname, xn);
	free (xn);
	return RERR_OK;
}




/****************************
 * static functions for all
 ****************************/

static
int
read_config ()
{
	cf_begin_read ();
	sm_max_wait = cf_atoi (cf_getval2 ("sm_max_wait", "3000"));
	page_size = getpagesize();
	vol_dir = cf_getval2 ("sm_volatile_dir", "/dev/shm/");
	perm_dir = cf_getval2 ("sm_perm_dir", "/var/permshm/");
	config_read=1;
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
