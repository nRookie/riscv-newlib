/*-
 * Copyright (c) 1983, 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#if !defined(lint) && !defined(_KERNEL) && defined(LIBC_SCCS)
static char rcsid[] = "$OpenBSD: mcount.c,v 1.6 1997/07/23 21:11:27 kstailey Exp $";
#endif

/*
 * This file is taken from Cygwin distribution. Please keep it in sync.
 * The differences should be within __MINGW32__ guard.
 */

#ifndef __MINGW32__
#include <sys/param.h>
#endif
#include <sys/types.h>
#include "gmon.h"
#include "winsup.h"

/*
 * mcount is called on entry to each function compiled with the profiling
 * switch set.  _mcount(), which is declared in a machine-dependent way
 * with _MCOUNT_DECL, does the actual work and is either inlined into a
 * C routine or called by an assembly stub.  In any case, this magic is
 * taken care of by the MCOUNT definition in <machine/profile.h>.
 *
 * _mcount updates data structures that represent traversals of the
 * program's call graph edges.  frompc and selfpc are the return
 * address and function address that represents the given call graph edge.
 *
 * Note: the original BSD code used the same variable (frompcindex) for
 * both frompcindex and frompc.  Any reasonable, modern compiler will
 * perform this optimization.
 */
/* _mcount; may be static, inline, etc */
_MCOUNT_DECL (size_t, size_t);

_MCOUNT_DECL (size_t frompc, size_t selfpc)
{
	register u_short *frompcindex;
	register struct tostruct *top, *prevtop;
	register struct gmonparam *p;
	register long toindex;

	p = &_gmonparam;
	/*
	 * check that we are profiling
	 * and that we aren't recursively invoked by this thread
	 * or entered anew by any other thread.
	 */
	if (InterlockedCompareExchange (
		    &p->state, GMON_PROF_BUSY, GMON_PROF_ON) != GMON_PROF_ON)
		return;
	/*
	 * check that frompcindex is a reasonable pc value.
	 * for example:	signal catchers get called from the stack,
	 *		not from text space.  too bad.
	 */
	frompc -= p->lowpc;
	if (frompc > p->textsize)
		goto done;

#if (HASHFRACTION & (HASHFRACTION - 1)) == 0
	if (p->hashfraction == HASHFRACTION)
		frompcindex =
		    &p->froms[frompc / (HASHFRACTION * sizeof(*p->froms))];
	else
#endif
		frompcindex =
		    &p->froms[frompc / (p->hashfraction * sizeof(*p->froms))];
	toindex = *frompcindex;
	if (toindex == 0) {
		/*
		 *	first time traversing this arc
		 */
		toindex = ++p->tos[0].link;
		if (toindex >= p->tolimit)
			/* halt further profiling */
			goto overflow;

		*frompcindex = toindex;
		top = &p->tos[toindex];
		top->selfpc = selfpc;
		top->count = 1;
		top->link = 0;
		goto done;
	}
	top = &p->tos[toindex];
	if (top->selfpc == selfpc) {
		/*
		 * arc at front of chain; usual case.
		 */
		top->count++;
		goto done;
	}
	/*
	 * have to go looking down chain for it.
	 * top points to what we are looking at,
	 * prevtop points to previous top.
	 * we know it is not at the head of the chain.
	 */
	for (; /* goto done */; ) {
		if (top->link == 0) {
			/*
			 * top is end of the chain and none of the chain
			 * had top->selfpc == selfpc.
			 * so we allocate a new tostruct
			 * and link it to the head of the chain.
			 */
			toindex = ++p->tos[0].link;
			if (toindex >= p->tolimit)
				goto overflow;

			top = &p->tos[toindex];
			top->selfpc = selfpc;
			top->count = 1;
			top->link = *frompcindex;
			*frompcindex = toindex;
			goto done;
		}
		/*
		 * otherwise, check the next arc on the chain.
		 */
		prevtop = top;
		top = &p->tos[top->link];
		if (top->selfpc == selfpc) {
			/*
			 * there it is.
			 * increment its count
			 * move it to the head of the chain.
			 */
			top->count++;
			toindex = prevtop->link;
			prevtop->link = top->link;
			top->link = *frompcindex;
			*frompcindex = toindex;
			goto done;
		}
	}
done:
	InterlockedExchange (&p->state, GMON_PROF_ON);
	return;
overflow:
	InterlockedExchange (&p->state, GMON_PROF_ERROR);
	return;
}

/*
 * Actual definition of mcount function.  Defined in <machine/profile.h>,
 * which is included by <sys/gmon.h>
 */
MCOUNT

