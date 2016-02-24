Another Regular Expression Library
-

This is a regular expression implementation. It draws on
RSC's regular expression [exposition](https://swtch.com/~rsc/regexp/)
and on libregexp and Ken Thompson's grep from plan 9.

It implements the regular expressions documented in plan 9's regexp(6)
by executing bytecode on a virtual machine. It also has the advantage of
being thread-safe unlike plan 9's libregexp.