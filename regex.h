#pragma src "/usr/ben/src/libregex"
#pragma lib "libregex.a"
enum
{
	OANY = 0,
	OBOL,
	OCLASS,
	OEOL,
	OJMP,
	ONOTNL,
	ORUNE,
	ORUNEM,
	OSAVE,
	OSPLIT,
	OSPLITSUB,
	OUNSAVE,

	NSUBEXP = 32
};

typedef struct Resub Resub;
typedef struct Reinst Reinst;
typedef struct Reprog Reprog;

/*
 * Sub expression matches
 */
struct Resub
{
	union
	{
		char *sp;
		Rune *rsp;
	};
	union
	{
		char *ep;
		Rune *rep;
	};
};

/*
 * Machine instructions
 */
struct Reinst
{
	char op;
	Reinst *a;
	union
	{
		Rune r;
		int sub;
	};
	union
	{
		Rune r1;
		Reinst *b;
	};
};

/*
 * Reprogram definition
 */
struct Reprog
{
	Reinst *startinst; /* start pc */
	char *regstr;
	int len;
};

Reprog *regcomp(char*);
Reprog *regcomplit(char*);
Reprog *regcompnl(char*);
void regerror(char*);
int regexec(Reprog*, char*, Resub*, int);
void regsub(char*, char*, int, Resub*, int);
int rregexec(Reprog*, Rune*, Resub*, int);
void rregsub(Rune*, Rune*, int, Resub*, int);
