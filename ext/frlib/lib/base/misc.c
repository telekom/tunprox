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
#include <errno.h>
#include <sys/types.h>
#include <stdint.h>

#include "misc.h"


int
numbits32 (a)
	uint32_t	a;
{
	uint32_t	b;
	int			sum, i, j;

	for (sum=0, i=0; i<4; i++) {
		b = ((unsigned char *)(void*)&a)[i];
		if (b==0) continue;
		for (j=0; j<8; j++) {
			if (b & (1 << j)) sum++;
		}
	}
	return sum;
}





int
numbits64 (a)
	uint64_t	a;
{
	uint64_t	b;
	int		sum, i, j;

	for (sum=0, i=0; i<8; i++) {
		b = ((unsigned char *)(void*)&a)[i];
		if (b==0) continue;
		for (j=0; j<8; j++) {
			if (b & (1 << j)) sum++;
		}
	}
	return sum;
}



uint64_t
distributebits64 (a, m)
	uint64_t	a, m;
{
	int		i,j;
	uint64_t	b = 0;

	for (i=j=0; i<64; i++) {
		if (m&(1<<i)) {
			b |= ((a&(1<<j))>>j)<<i;
			j++;
		}
	}
	return b;
}


uint32_t
distributebits32 (a, m)
	uint32_t	a, m;
{
	int		i,j;
	uint32_t	b = 0;

	for (i=j=0; i<32; i++) {
		if (m&(1<<i)) {
			b |= ((a&(1<<j))>>j)<<i;
			j++;
		}
	}
	return b;
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
