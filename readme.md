Libegexp
=

This is a regular expression implementation. It draws on
RSC's regular expression [exposition](https://swtch.com/~rsc/regexp/)
and on libregexp and Ken Thompson's grep from plan 9.

It implements the regular expression as bytecode executed
on a vm. This implementation follows the grammar in plan 9's regexp(6)
with the following clarifications:

* `.` matches any character including newline.
* `^` matches only the beginning of a string (not the character after newline)
* `$` matches only the end of a string (not a newline)
* a negated character class does not match newline (as specified in regexp(6))

This implementation has the advantage of being thread-safe unlike libregexp
in plan 9.