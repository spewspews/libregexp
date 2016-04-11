</$objtype/mkfile

LIB=/$objtype/lib/newlibregexp.a
OFILES=\
	regcomp.$O\
	regerror.$O\
	regexec.$O\
	regsub.$O\
	rregexec.$O\
	rregsub.$O\
	regprint.$O\

HFILES=\
	regex.h\

UPDATE=\
	mkfile\
	$HFILES\
	${OFILES:%.$O=%.c}\
	${LIB:/$objtype/%=/386/%}\

</sys/src/cmd/mksyslib

$O.regextest: tests/regextest.$O $LIB
	$LD -o $target regextest.$O

$O.sysregextest: tests/sysregextest.$O
	$LD -o $target sysregextest.$O
