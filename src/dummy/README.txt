
This directory contains files which are a moral replacement for some standard
Win32API headers which, for reasons beyond me, Microsoft created incompatible
with plain C for no apparent reason.

Note that the headers are very incomplete: They only contain stuff needed
for successful building of mCtrl.

Also, to avoid clash with e.g. forward declarations in various standard
headers, we use the prefix "dummy_" for all globally visible symbols.
