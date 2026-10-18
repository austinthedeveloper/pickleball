# Contributing

Before calling a change done, run the checks in
[CLAUDE.md](CLAUDE.md#before-calling-something-done): `make check`,
`make perf`, `make golden-compare` against the base commit, `make stats`
when AI, rules, positions, or opponent styles change, and a screenshot of
anything visible. Keep generated files out of commits and use the milestone
task ID at the start of commit messages, for example
`M4-SAVE-03: initials entry`.

Warnings are defects. A milestone checkbox should only be checked in the same
commit that contains its implementation, checks, and documentation.
