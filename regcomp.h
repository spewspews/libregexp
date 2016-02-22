enum
{
	LANY = 0,
	LBOL,
	LCLASS,
	LEND,
	LEOL,
	LLPAR,
	LOR,
	LREP,
	LRPAR,
	LRUNE,

	TANY = 0,
	TBOL,
	TCAT,
	TCLASS,
	TEOL,
	TNOTNL,
	TOR,
	TPLUS,
	TQUES,
	TRUNE,
	TSTAR,
	TSUB
};

typedef struct Parselex Parselex;
typedef struct Renode Renode;

struct Parselex
{
	/* Parse */
	Renode *freep;
	Renode *nodes;
	int sub;
	/* Lex */
	char *rawexp;
	Rune yyrune;
	Rune yypeek;
	int peek;
	int done;
	int literal;
	Rune cpairs[200+2];
	int nc;
};
struct Renode
{
	int op;
	union
	{
		Rune r;
		Renode *left;
	};
	union
	{
		Rune r1;
		int sub;
		Renode *right;
	};
};
