#include <u.h>
#include <libc.h>
#include "regex.h"
#include "regcomp.h"

static int yylex(Parselex*);
static void getnextr(Parselex*);
static void getnextrlit(Parselex*);
static void getclass(Parselex*);
static Renode *e0(Parselex*);
static Renode *e1(Parselex*);
static Renode *e2(Parselex*);
static Renode *e3(Parselex*);
static Renode *buildclass(Parselex*);
static Renode *buildclassn(Parselex*);
static int pcmp(void*, void*);
Reprog *regcomp1(char*, int, int);
static Reinst *compile(Renode*, Reprog*);
static Reinst *compile1(Renode*, Reinst*, int*);
static void prtree(Renode*, int, int);
static void prprog(Reprog*);
//static void prinst(Reinst*);

static Renode*
node(Parselex *plex, int op, Renode *l, Renode *r)
{
	Renode *n;

	n = plex->freep++;
	n->op = op;
	n->left = l;
	n->right = r;
	return n;
}

static Renode*
e3(Parselex *plex)
{
	Renode *n;
	int sym;

	switch(sym = yylex(plex)) {
	case LANY:
		return node(plex, TANY, nil, nil);
	case LBOL:
		return node(plex, TBOL, nil, nil);
	case LEOL:
		return node(plex, TEOL, nil, nil);
	case LRUNE:
		n = node(plex, TRUNE, nil, nil);
		n->r = plex->yyrune;
		return n;
	case LCLASS:
		if(plex->nc)
			return buildclassn(plex);
		return buildclass(plex);
	case LLPAR:
		n = e0(plex);
		n = node(plex, TSUB, n, nil);
		if(yylex(plex) != LRPAR)
			regerror("no matching parenthesis");
		return n;
	default:
		print("%d\n", sym); 
		regerror("nope");
		break;
	}
	return nil;
}

static Renode*
e2(Parselex *plex)
{
	Renode *n;

	n = e3(plex);
	if(yylex(plex) == LREP) {
		switch(plex->yyrune) {
		case L'*':
			return node(plex, TSTAR, n, nil);
		case L'+':
			return node(plex, TPLUS, n, nil);
		case L'?':
			return node(plex, TQUES, n, nil);
		}
	}
	plex->yypeek = plex->yyrune;
	return n;
}

static Renode*
e1(Parselex *plex)
{
	Renode *n, *n1;
	int sym;

	n = e2(plex);
	for(;;) {
		sym = yylex(plex);
		if(sym == LEND || sym == LOR || sym == LRPAR)
			break;
		plex->yypeek = plex->yyrune;
		n1 = e2(plex);
		n = node(plex, TCAT, n, n1);
	}
	plex->yypeek = plex->yyrune;
	return n;
}

static Renode*
e0(Parselex *plex)
{
	Renode *n, *n1;

	n = e1(plex);
	for(;;) {
		if(yylex(plex) != LOR)
			break;
		n1 = e1(plex);
		n = node(plex, TOR, n, n1);
	}
	plex->yypeek = plex->yyrune;
	return n;
}

Reprog*
regcomp1(char *regstr, int nl, int lit)
{
	Reprog *reprog;
	Parselex plex;
	Renode *parsetr;
	int regstrlen;

	regstrlen = utflen(regstr);
	reprog = malloc(sizeof(*reprog) + sizeof(Reinst)*(regstrlen*2 + 5));
	plex.nodes = calloc(sizeof(*plex.nodes), regstrlen*2 + 10);
	if(reprog == nil || plex.nodes == nil)
		return nil;
	plex.freep = plex.nodes;
	plex.rawexp = regstr;
	plex.sub = 0;
	plex.getnextr = lit ? getnextrlit : getnextr;
	parsetr = node(&plex, TSUB, e0(&plex), nil);
//	prtree(parsetr, 0, 0);
	reprog->startinst = compile(parsetr, reprog);
	free(plex.nodes);
//	prprog(reprog);
	return reprog;
}

Reprog*
regcomp(char *str)
{
	return regcomp1(str, 0, 0);
}

Reprog*
regcomplit(char *str)
{
	return regcomp1(str, 0, 1);
}

static Reinst*
compile1(Renode *renode, Reinst *reinst, int *sub)
{
	Reinst *i;
	int s;

	switch(renode->op) {
	case TRUNE:
		reinst->op = ORUNE;
		reinst->r = renode->r;
		return reinst + 1;
	case TCLASS:
		reinst->op = OCLASS;
		reinst->r = renode->r;
		reinst->r1 = renode->r1;
		return reinst + 1;
	case TCAT:
		reinst = compile1(renode->left, reinst, sub);
		return compile1(renode->right, reinst, sub);
	case TOR:
		reinst->op = OSPLIT;
		reinst->a = reinst + 1;
		i = compile1(renode->left, reinst->a, sub);
		reinst->b = i + 1;
		i->op = OJMP;
		i->a = compile1(renode->right, reinst->b, sub);
		return i->a;
	case TSTAR:
		reinst->op = OSPLIT;
		reinst->a = reinst + 1;
		i = compile1(renode->left, reinst->a, sub);
		reinst->b = i + 1;
		i->op = OJMP;
		i->a = reinst;
		return reinst->b;
	case TPLUS:
		i = reinst;
		reinst = compile1(renode->left, reinst, sub);
		reinst->op = OSPLIT;
		reinst->a = i;
		reinst->b = reinst + 1;
		return reinst->b;
	case TQUES:
		reinst->op = OSPLIT;
		reinst->a = reinst + 1;
		reinst->b = compile1(renode->left, reinst->a, sub);
		return reinst->b;
	case TSUB:
		reinst->op = OSAVE;
		reinst->sub = s = (*sub)++;
		reinst = compile1(renode->left, reinst+1, sub);
		reinst->op = OUNSAVE;
		reinst->sub = s;
		return reinst + 1;
	case TANY:
//		reinst++->op = ONOTNL;
		reinst->op = OANY;
		return reinst + 1;
	case TNOTNL:
		reinst->op = ONOTNL;
		return reinst + 1;
	case TEOL:
		reinst->op = OEOL;
		return reinst + 1;
	case TBOL:
		reinst->op = OBOL;
		return reinst + 1;
	}
	return nil;
}

static Reinst*
compile(Renode *parsetr, Reprog *reprog)
{
	Reinst *reinst, *end;
	int sub;

	reinst = (Reinst*)(reprog+1);
	end = compile1(parsetr, reinst, &sub);
	reprog->len = end - reinst;
	return reinst;
}

static void
getnextr(Parselex *l)
{
	l->literal = 0;
	if(l->done) {
		l->yyrune = 0;
		return;
	}
	l->rawexp += chartorune(&l->yyrune, l->rawexp);
	if(*l->rawexp == L'\\') {
		l->rawexp += chartorune(&l->yyrune, l->rawexp);
		l->literal = 1;
	}
	if(*l->rawexp == 0)
		l->done = 1;
	return;
}

static void
getnextrlit(Parselex *l)
{
	l->literal = 1;
	if(l->done) {
		l->literal = 0;
		l->yyrune = 0;
		return;
	}
	l->rawexp += chartorune(&l->yyrune, l->rawexp);
	if(*l->rawexp == 0)
		l->done = 1;
	return;
}

static int
yylex(Parselex *l)
{
	if(l->yypeek) {
		l->yyrune = l->yypeek;
		l->yypeek = 0;
		return l->peek;
	}
	l->getnextr(l);
	if(l->literal)
		return l->peek = LRUNE;
	switch(l->yyrune){
	case 0:
		return l->peek = LEND;
	case L'*':
	case L'?':
	case L'+':
		return l->peek = LREP;
	case L'|':
		return l->peek = LOR;
	case L'.':
		return l->peek = LANY;
	case L'(':
		return l->peek = LLPAR;
	case L')':
		return l->peek = LRPAR;
	case L'^':
		return l->peek = LBOL;
	case L'$':
		return l->peek = LEOL;
	case L'[':
		getclass(l);
		return l->peek = LCLASS;
	}
	return l->peek = LRUNE;
}

static int
pcmp(void *va, void *vb)
{
	vlong n;
	Rune *a, *b;

	a = va;
	b = vb;

	n = (vlong)a[0] - (vlong)b[0];
	if(n)
		return n;
	return (vlong)a[1] - (vlong)b[1];
}

static void
getclass(Parselex *l)
{
	Rune *p, *q, t;

	l->nc = 0;
	l->getnextr(l);
	if(l->yyrune == L'^') {
		l->nc = 1;
		l->getnextr(l);
	}
	p = l->cpairs;
	p[0] = l->yyrune;
	for(;;) {
		if(l->yyrune == '\\') {
			l->getnextr(l);
			p[0] = l->yyrune;
		}
		if(l->yyrune == L']')
			break;
		p[1] = l->yyrune;
		p += 2;
		if(p >= l->cpairs + nelem(l->cpairs) - 2)
			regerror("class too big");
		l->getnextr(l);
		if(l->yyrune != L'-') {
			p[0] = l->yyrune;
			continue;
		}
		l->getnextr(l);
		if(l->yyrune == '\\')
			l->getnextr(l);
		if(l->yyrune == L']')
			break;
		p[-1] = l->yyrune;
		if(p[-2] > p[-1]) {
			t = p[-2];
			p[-2] = p[-1];
			p[-1] = t;
		}
		l->getnextr(l);
		p[0] = l->yyrune;
	}
	*p = 0;
	qsort(l->cpairs, (p - l->cpairs)/2, 2*sizeof(*l->cpairs), pcmp);
	q = l->cpairs;
	for(p = l->cpairs+2; *p != 0; p += 2) {
		if(p[0] > q[1] + 1) {
			q[2] = p[0];
			q[3] = p[1];
			q += 2;
			continue;
		}
		q[1] = p[1];
	}
	q[2] = 0;
}

static Renode*
buildclassn(Parselex *l)
{
	Renode *n, *n1;
	Rune *p;

	p = l->cpairs;
	n = node(l, TCLASS, nil, nil);
	n->r = 0;
	n->r1 = p[0] - 1;

	for(p += 2; *p != 0; p += 2) {
		n1 = node(l, TCLASS, nil, nil);
		n1->r = p[-1] + 1;
		n1->r1 = p[0] - 1;
		n = node(l, TOR, n, n1);
	}
	n1 = node(l, TCLASS, nil, nil);
	n1->r = p[-1] + 1;
	n1->r1 = Runemax;
	n = node(l, TOR, n, n1);

	n1 = node(l, TNOTNL, nil, nil);
	n1->r = L'\n';
	n = node(l, TCAT, n1, n);
	return n;
}

static Renode*
buildclass(Parselex *l)
{
	Renode *n, *n1;
	Rune *p;

	p = l->cpairs;
	if(p[0] != p[1]) {
		n = node(l, TCLASS, nil, nil);
		n->r = p[0];
		n->r1 = p[1];
	} else {
		n = node(l, TRUNE, nil, nil);
		n->r = p[0];
	}
	for(p += 2; *p != 0; p += 2) {
		if(p[0] != p[1]) {
			n1 = node(l, TCLASS, nil, nil);
			n1->r = p[0];
			n1->r1 = p[1];
			n = node(l, TOR, n, n1);
		} else {
			n1 = node(l, TRUNE, nil, nil);
			n1->r = p[0];
			n = node(l, TOR, n, n1);
		}
	}
	return n;
}
	
static void
prtree(Renode *tree, int d, int f)
{
	int i;

	if(tree == nil)
		return;
	if(f)
	for(i = 0; i < d; i++)
		print("\t");
	switch(tree->op) {
	case TCAT:
		prtree(tree->left, d, 0);
		prtree(tree->right, d, 1);
		break;
	case TOR:
		print("TOR\n");
		prtree(tree->left, d+1, 1);
		for(i = 0; i < d; i++)
			print("\t");
		print("|\n");
		prtree(tree->right, d+1, 1);
		break;
	case TSTAR:
		print("*\n");
		prtree(tree->left, d+1, 1);
		break;
	case TPLUS:
		print("+\n");
		prtree(tree->left, d+1, 1);
		break;
	case TQUES:
		print("?\n");
		prtree(tree->left, d+1, 1);
		break;
	case TANY:
		print(".\n");
		prtree(tree->left, d+1, 1);
		break;
	case TBOL:
		print("^\n");
		break;
	case TEOL:
		print("$\n");
		break;
	case TSUB:
		print("SUB\n");
		prtree(tree->left, d+1, 1);
		break;
	case TRUNE:
		print("RUNE: %C\n", tree->r);
		break;
	case TNOTNL:
		print("NOTNL: \\n\n");
		break;
	case TCLASS:
		print("CLASS: %C-%C\n", tree->r, tree->r1);
		break;
	}
}

static void
prprog(Reprog *reprog)
{
	Reinst *inst;

	print("Printing prog of len %d\n", reprog->len);
	for(inst = reprog->startinst; inst < reprog->startinst + reprog->len; inst++)
		prinst(inst);
}

void
prinst(Reinst *inst)
{
	print("%p ", inst);
	switch(inst->op) {
	case ORUNE:
		print("ORUNE\t%C\n", inst->r);
		break;
	case ONOTNL:
		print("ONOTNL\n");
		break;
	case OCLASS:
		print("OCLASS\t%C-%C\n", inst->r, inst->r1);
		break;
	case OSPLIT:
		print("OSPLIT\t%p %p\n", inst->a, inst->b);
		break;
	case OJMP:
		print("OJMP \t%p\n", inst->a);
		break;
	case OSAVE:
		print("OSAVE\t%d\n", inst->sub);
		break;
	case OUNSAVE:
		print("OUNSAVE\t%d\n", inst->sub);
		break;
	case OANY:
		print("OANY \t.\n");
		break;
	case OEOL:
		print("OEOL \t$\n");
		break;
	case OBOL:
		print("OBOL \t^\n");
		break;
	}
}
