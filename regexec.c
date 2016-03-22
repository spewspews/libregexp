#include <u.h>
#include <libc.h>
#include "regex.h"

typedef struct Thread Thread;
typedef struct Threadlist Threadlist;
typedef struct Submatch Submatch;
typedef struct Submatchlist Submatchlist;

struct Thread
{
	Reinst *pc;
	Submatch *submatch;
};
struct Threadlist
{
	Thread *threads;
	Thread *next;
};
struct Submatch
{
	long ref;
	Resub *sem;
};
struct Submatchlist
{
	Submatch **subs;
	Submatch **next;
};

static void
incref(Submatch *sm)
{
	sm->ref++;
}

static long
decref(Submatch *sm)
{
	if(sm->ref == 0)
		return 0;
	return --sm->ref;
}

static void
freesubmatch(Submatch *submatch)
{
	free(submatch->sem);
	free(submatch);
}

static void
pushsubmatch(Submatchlist *list, Submatch *sub)
{
//	print("Pushing %p\n", sub);
	*list->next++ = sub;
}

static Submatch*
popsubmatch(Submatchlist *list, int msize)
{
	Submatch *sub;

	if(list->next == list->subs) {
		sub = malloc(sizeof(*sub));
		if(sub == nil)
			goto Fail;
		sub->sem = calloc(sizeof(*sub->sem), msize);
		if(sub->sem == nil)
			goto Fail;
	} else
		sub = *(--list->next);
	sub->ref = 0;
//	print("Popping %p\n", sub);
	return sub;
Fail:
	regerror("Out of memory");
	return nil;
}

void
savesub(Thread *t, Reinst *curinst, char *sp, Submatchlist *sublist, int msize, int unsave)
{
	Submatch *submatch;

	if(curinst->sub < msize) {
		if(t->submatch->ref > 1) {
			submatch = popsubmatch(sublist, msize);
			memcpy(submatch->sem, t->submatch->sem, sizeof(*submatch->sem)*msize);
			incref(submatch);
			decref(t->submatch);
			t->submatch = submatch;
		}
		if(unsave)
			t->submatch->sem[curinst->sub].ep = sp;
		else
			t->submatch->sem[curinst->sub].sp = sp;
	}
}

int
regexec(Reprog *prog, char *str, Resub *sem, int msize)
{
	Threadlist lists[2], *clist, *nlist, *tmp;
	Thread *t, *s;
	Reinst *curinst;
	Submatchlist sublist;
	Submatch **matchp, *startmatch;
	char *sp;
	Rune r;
	int i, match, firstmatch, first, gen;

	if(prog->startinst->gen != 0)
	for(curinst = prog->startinst; curinst < prog->startinst + prog->len; curinst++)
		curinst->gen = 0;
	for(clist = lists; clist < lists + 2; clist++) {
		clist->threads = calloc(sizeof(*clist->threads), prog->len+1);
		if(clist->threads == nil) {
			regerror("Out of memory");
			return 0;
		}
		clist->next = clist->threads;
	}
	if(msize) {
		sublist.subs = calloc(sizeof(*sublist.subs), utflen(str)+1);
		if(sublist.subs == nil) {
			regerror("Out of memory");
			return 0;
		}
		sublist.next = sublist.subs;
	}
	clist = lists;
	nlist = lists+1;

	gen = 0;
	match = 0;
	if(msize)
		startmatch = popsubmatch(&sublist, msize);
	else
		startmatch = nil;
	r = Runemax + 1;
	for(sp = str; r != 0; sp += i) {
		gen++;
		i = chartorune(&r, sp);
		firstmatch = first = 1;
		for(t = clist->threads; t < clist->next; t++) {
			curinst = t->pc;
Again:
//			print("thread: %p, %p, %d\n", t, curinst, curinst->gen);
			switch(curinst->op) {
			case ORUNE:
				if(r != curinst->r)
					goto Threaddone;
			case OANY: /* fallthrough */
			Any:
				if(curinst[1].gen == gen + 1)
					goto Threaddone;
				curinst[1].gen = gen + 1;
				nlist->next->pc = curinst + 1;
				nlist->next->submatch = t->submatch;
				nlist->next++;
				break;
			case OCLASS:
			Class:
				if(curinst->a->gen == gen + 1 || r < curinst->r)
					goto Threaddone;
				if(r > curinst->r1) {
					curinst++;
					goto Class;
				}
				curinst->a->gen = gen + 1;
				nlist->next->pc = curinst->a;
				nlist->next->submatch = t->submatch;
				nlist->next++;
				break;
			case ONOTNL:
				if(r != L'\n') {
					curinst++;
					goto Again;
				}
				goto Threaddone;
			case OBOL:
				if(sp == str || sp[-1] == '\n') {
					curinst++;
					goto Again;
				}
				goto Threaddone;
			case OEOL:
				if(r == 0) {
					curinst++;
					goto Again;
				}
				if(r == '\n')
					goto Any;
				goto Threaddone;
			case OJMP:
				curinst = curinst->a;
				goto Again;
			case OSPLIT:
				if(curinst->b->gen != gen) {
					clist->next->pc = curinst->b;
					if(msize) {
						clist->next->submatch = t->submatch;
						incref(t->submatch);
					}
					clist->next++;
				} else if(msize) {
					for(s = t + 1; s < clist->next; s++) {
						if(s->pc == curinst->b)
							break;
					}
					incref(t->submatch);
					if(decref(s->submatch) == 0 && s->submatch != startmatch)
						pushsubmatch(&sublist, s->submatch);
					s->submatch = t->submatch;
				}
				curinst = curinst->a;
				goto Again;
			case OSAVE:
				savesub(t, curinst, sp, &sublist, msize, 0);
				curinst++;
				goto Again;
			case OUNSAVE:
				/* First match is the left-most longest. */
				if(curinst->sub == 0) {
					if (!firstmatch)
						goto Threaddone;
					firstmatch = 0;
					match = 1;
					if(msize) {
						memcpy(sem, t->submatch->sem, sizeof(*sem)*msize);
						sem[curinst->sub].ep = sp;
					}
					goto Threaddone;
				}
				savesub(t, curinst, sp, &sublist, msize, 1);
				curinst++;
				goto Again;
			Threaddone:
				if(msize == 0)
					break;
				if(decref(t->submatch) == 0 && t->submatch != startmatch)
					pushsubmatch(&sublist, t->submatch);
				break;
			}
		}
//		print("Clist threads: %lld\n", clist->next - clist->threads);
//		for(t = clist->threads; t < clist->next; t++)
//			print("thread instruction: %p %d\n", t->pc, t->pc->gen);
		/* Start again once if we haven't found anything. */
		if(first == 1 && match == 0) {
			first = 0;
			clist->next = clist->threads;
			t = clist->next++;
			if(msize) {
				t->submatch = startmatch;
				incref(t->submatch);
			}
//			print("Start again\n");
//			t->pc = prog->startinst;
			curinst = prog->startinst;
			goto Again;
		}
		/* If we have a match and no extant threads, we are done. */
		if(match == 1 && nlist->next == nlist->threads)
			break;
		tmp = clist;
		clist = nlist;
		nlist = tmp;
		nlist->next = nlist->threads;
	}
	prog->startinst->gen = gen;

	for(clist = lists; clist < lists + 2; clist++)
		free(clist->threads);
	if(msize) {
//		print("submatches allocated: %lld\n", sublist.next - sublist.subs + 1);
		for(matchp = sublist.subs; matchp < sublist.next; matchp++)
			freesubmatch(*matchp);
		freesubmatch(startmatch);
		free(sublist.subs);
	}

	return match;
}
