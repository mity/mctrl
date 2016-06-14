
# Contributing to mCtrl

We love to receive patches improving mCtrl.

We are still relatively small project and there are actually no formal
processes when accepting patches. The crucial question is always "Does the
change make mCtrl better?"

The prefered way how to contribute code is making [pull requests][1] for
out [Github repository][2] but it is not strong condition.


## Reporting Bugs

Even if you are unable to fix a bug on our own, reporting it is very 
benefitial to us. Unheard bugs cannot get fixed. You may submit the bug 
as a [Github issue][3].

Please provide the following information with the bug report:

* mCtrl version you are using.
* Whether you use 32-bit or 64-bit build of mCtrl.
* OS version where you reproduce the issue.
* As explicit description of the issue as possible, i.e. what behavior
  you expect and what behavior you see.
  (Reports of the kind "it does not work." do not help).
* If relevant, consider attaching a screenshot or some code reproducing
  the issue.


## Trivial Changes

Bug fixes, documentation fixes and all those little improvements which do
our life brighter are always welcome.


## Bigger Changes

More complex features are welcome, too. But to keep some consistency, new
features should fulfill some expectations.

Remember mCtrl is about reusable controls. If the feature is generally useful
it is welcome. 

If your new control or feature is very specific to your particular application,
you may rather keep the code outside of mCtrl code tree: You may derive your
controls from mCtrl ones via a technique of [control customization][4]. (If
needed, changes improving customizability of mCtrl controls **are** welcome.)

Furthermore, keep in mind mCtrl supports Windows 2000 and anything newer.
The new features may not be available on such old Windows, but mCtrl must not 
crash and the lack of the feature must not render mCtrl or the control unusable
on Windows 2000.

This especially means depending on any system DLL which was only added later,
e.g. in Windows Vista, has to be resolved in run-time via `LoadLibrary()`
and `GetProcAddress()`, with some reasonable fallback strategy for older
Windows versions.


## API Changes

Changes to public API require special attention. Designing bad API may lead
to big burdern in the future.

Any changes to public API (whatever is in the `include` directory) have to
be backward compatible, unless there is extremely strong reason to do
otherwise. (You know, even the API itself may be broken but we do our best
to avoid this kind of problems.)

Both, the source code as well as binary level compatibility matters.

Therefore, whenever adding new API, expect it shall be reviewed very closely 
before we accept it into the upstream. Especially these aspects of the new API
are scrutinized:

* Is the new API easy to use? (It should be descriptive, it should not allow
  to get the control into invalid state, it should not expose peculiarities
  of inner implementation.)

* Does it follow mCtrl naming scheme? (Examine all public headers to get an
  idea.)

* Is it well documented? (It has to be clear how it is supposed to be used
  from the documentation in public headers, which can be retrieved out into
  the reference manual with Doxygen.)

* Is the new API designed with respect to backward and forward compatibility?
  Can it be extended in the future if such need arises?

* Is there already an analogy in API of another control, either directly in
  mCtrl or in system DLL (usually, `USER32.DLL` or `COMCTL32.DLL`)? If yes,
  does the API follow the analogy?


## Final Word

Last but not least, do not be afraid to ask or to make any mistake. We do not 
bite and we believe in iterative process and discussion rather then in getting 
perfect solution right away.


[1]: https://help.github.com/articles/using-pull-requests/
[2]: http://github.com/mity/mctrl/
[3]: http://github.com/mity/mctrl/issues/
[4]: http://www.codeproject.com/Articles/646482/Custom-Controls-in-Win-API-Control-Customization/
