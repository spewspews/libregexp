Another Regular Expression Library
-

This is a regular expression implementation. It draws on
RSC's regular expression [exposition](https://swtch.com/~rsc/regexp/)
Ken Thompson's grep from Plan 9.

It implements the regular expressions documented in plan 9's regexp(6)
by executing bytecode on a virtual machine. It is a drop-in replacement
for Plan 9's libregexp with the following key differences:

(1) It is faster.
  For typical regular expressions, this implementation is
  typically 2.6 to 3 times faster than the system
  implementation.  For abnormal regular expressions--something
  like `a?a?a?a?a?a?a?`--I have observed this implementation to
  be 55 times faster.
(2) It is correct.
  The system implementation will fail to capture sub-matches for
  certain regular expressions--typically abnormal ones.  The
  rune implementation fails faster than the character based one.
  This implementation will always capture sub-matches.
(3) It is a smaller implementation.
  The system implementation's wc is:
    1395    3919   27249 total
  This implementation's wc is:
    1269    3052   21597 total
(4) It is thread-safe/re-entrant.
  The system implementation references external variables during
  compilation (the execution system is thread-safe).  This
  implementation is fully thread-safe at all times.

I have not checked memory usage but I believe that it is more
efficient for simple regular expressions and possibly less efficient
for abnormal expressions that cause the system implementation to fail.

If you are interested in performing
your own comparison, the tests directory contains two programs to
compare this implementation to the system's implementation.  Usage of
those is:

regextest -lnp -r reps -c creps -m nsubm regex matchstr
usage:
  reps is the number of times to run regex against the matchstr (default 1).
  creps is the number of times to compile the regex (default 1).
  nsubm is the number of submatches to capture (default 10).
  -l is to compile a literal regular expression.
  -n is to compile a regular expression where . does not match '\n'.
  -p is to print the compiled regular expression.

sysregextest uses the system implementation.

This implementation also includes a fmt routine to print compiled regular
expressions.  You can install it with e.g. `fmtinstall('R', reprogfmt);`
The regular expressions 'foo|bar' and 'a+b*c?(foo)+|bar'
are printed below as an example.

```
402070 OSAVE 0
402090 OSPLIT 4020b0 402130
4020b0 ORUNE f
4020d0 ORUNE o
4020f0 ORUNE o
402110 OJMP 402190
402130 ORUNE b
402150 ORUNE a
402170 ORUNE r
402190 OUNSAVE 0

4023f0 OSAVE 0
402410 OSPLIT 402430 402630
402430 ORUNE a
402450 OSPLIT 402430 402470
402470 OSAVE 1
402490 OSPLIT 4024b0 4024f0
4024b0 ORUNE b
4024d0 OJMP 402490
4024f0 OUNSAVE 1
402510 OSPLIT 402530 402550
402530 ORUNE c
402550 OSAVE 2
402570 ORUNE f
402590 ORUNE o
4025b0 ORUNE o
4025d0 OUNSAVE 2
4025f0 OSPLIT 402550 402610
402610 OJMP 402690
402630 ORUNE b
402650 ORUNE a
402670 ORUNE r
402690 OUNSAVE 0
```
