#include <u.h>
#include <libc.h>
#include <thread.h>
#include "regex.h"

typedef struct Thread Thread;
typedef struct Threadlist Threadlist;
typedef struct Resubref Resubref;
typedef struct Resubreflist Resubreflist;

struct Thread
{
	Reinst *pc;
	Resubref *submatch;
};
struct Threadlist
{
	Thread *threads;
	Thread *next;
};
struct Resubref
{
	Ref;
	Resub *sem;
};
struct Resubreflist
{
	Resubref **subs;
	Resubref **next;
};

static void
freesubref(Resubref *subref)
{
	free(subref->sem);
	free(subref);
}

static void
pushsubref(Resubreflist *list, Resubref *sub)
{
	*list->next++ = sub;
}

static Resubref*
popsubref(Resubreflist *list, int msize)
{
	Resubref *sub;
	static int count;

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
	return sub;
Fail:
	regerror("Out of memory");
	return nil;
}

int
rregexec(Reprog *prog, Rune *str, Resub *sem, int msize)
{
	Threadlist lists[2], *clist, *nlist, *tmp;
	Thread *t;
	Reinst *curinst;
	Resubreflist sublist;
	Resubref *submatch, **matchp;
	Rune *rp, last;
	int match, firstmatch, first, gen;
	long rstrlen;

	if(prog->startinst->gen != 0)
	for(curinst = prog->startinst; curinst < prog->startinst + prog->len; curinst++)
		curinst->gen = 0;
	rstrlen = runestrlen(str);
	for(clist = lists; clist < lists + 2; clist++) {
		clist->threads = calloc(sizeof(*clist->threads), prog->len+rstrlen);
		if(clist->threads == nil) {
			regerror("Out of memory");
			return 0;
		}
		clist->next = clist->threads;
	}
	sublist.subs = calloc(sizeof(*sublist.subs), utflen(prog->regstr));
	if(sublist.subs == nil) {
		regerror("Out of memory");
		return 0;
	}
	sublist.next = sublist.subs;
	clist = lists;
	nlist = lists+1;

	gen = 0;
	match = 0;
	last = 1;
	for(rp = str; last != L'\0'; rp++) {
		gen++;
		last = *rp;
		firstmatch = first = 1;
		for(t = clist->threads; t < clist->next; t++) {
			curinst = t->pc;
Again:
			switch(curinst->op) {
			case ORUNE:
				if(*rp != curinst->r)
					goto Threaddone;
			case OANY: /* fallthrough */
				(curinst + 1)->gen = gen + 1;
				nlist->next->pc = curinst + 1;
				nlist->next->submatch = t->submatch;
				nlist->next++;
				break;
			case OCLASS:
				if(*rp < curinst->r)
					goto Threaddone;
				if(*rp > curinst->r1) {
					curinst++;
					goto Again;
				}
				curinst->a->gen = gen + 1;
				nlist->next->pc = curinst->a;
				nlist->next->submatch = t->submatch;
				nlist->next++;
				break;
			case ONOTNL:
				if(*rp != L'\n') {
					curinst++;
					goto Again;
				}
				goto Threaddone;
			case OBOL:
				if(rp == str || *(rp-1) == L'\n') {
					curinst++;
					goto Again;
				}
				goto Threaddone;
			case OEOL:
				if(*rp == 0) {
					curinst++;
					goto Again;
				}
				if(*rp == '\n') {
					(curinst + 1)->gen = gen + 1;
					nlist->next->pc = curinst + 1;
					nlist->next->submatch = t->submatch;
					nlist->next++;
				}
				goto Threaddone;
			case OJMP:
				curinst = curinst->a;
				goto Again;
			case OSPLIT:
				if (curinst->b->gen != gen) {
					curinst->b->gen = gen;
					clist->next->pc = curinst->b;
					if(msize) {
						clist->next->submatch = t->submatch;
						incref(t->submatch);
					}
					clist->next++;
				}
				curinst = curinst->a;
				goto Again;
			case OSAVE:
				if(curinst->sub < msize)
					t->submatch->sem[curinst->sub].rsp = rp;
				curinst++;
				goto Again;
			case OUNSAVE:
				if(curinst->sub < msize)
					t->submatch->sem[curinst->sub].rep = rp;
				/* First match is the left-most longest. */
				if(curinst->sub == 0 && firstmatch) {
					firstmatch = 0;
					match = 1;
					if(msize)
						memcpy(sem, t->submatch->sem, sizeof(sem)*msize);
					goto Threaddone;
				}
				curinst++;
				goto Again;
			Threaddone:
				if(msize == 0)
					break;
				if(decref(t->submatch) == 0)
					pushsubref(&sublist, t->submatch);
				break;
			}
		}
		/* Start again once if we haven't found anything. */
		if(first == 1 && match == 0) {
			first = 0;
			clist->next = clist->threads;
			t = clist->next++;
			if(msize) {
				t->submatch = popsubref(&sublist, msize);
				incref(t->submatch);
			}
			curinst = prog->startinst;
			curinst->gen = gen;
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

	for(clist = lists; clist < lists + 2; clist++)
		free(clist->threads);
	for(matchp = sublist.subs; matchp < sublist.next; matchp++)
		freesubref(*matchp);
	free(sublist.subs);
	return match;
}
