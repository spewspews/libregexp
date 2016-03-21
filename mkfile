</$objtype/mkfile

LIB=/$objtype/lib/libregex.a
OFILES=\
	regcomp.$O\
	regerror.$O\
	regexec.$O\
	regsub.$O\
	rregexec.$O\
	rregsub.$O\

HFILES=\
	regex.h\

UPDATE=\
	mkfile\
	$HFILES\
	${OFILES:%.$O=%.c}\
	${LIB:/$objtype/%=/386/%}\

</sys/src/cmd/mksyslib

$O.regextest: regextest.$O $LIB
	$LD -o $target $prereq

$O.sysregextest: sysregextest.$O
	$LD -o $target $prereq
