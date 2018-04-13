- if it's working with x=0.5 but not other values of x, try x'=1-x
- for white backgrounds, memset to 255
- if constants should be defined but aren't found, check if they're excluded by `WIN32_LEAN_AND_MEAN`
- idempotent functions (e.g. AddPoint) are very useful
- take a trick from Hemingway's book - leave a line without a semicolon to come back to
- union static array with struct containing array - can assign
- always have a default case in switches - Assert if nothing is more appropriate
- better to take the path with lots of smaller testable zigzags than one long linear jump that's untestable
- keeping things in sync is bug prone - avoid wherever possible
- on build, define useful information into a header file/with compiler flags (e.g. git tag, build date...)
- beware infinite loops based on float precision. Guard against with if(X == X + 1.f || Y == Y + 1.f) { break; }
- use long functions but keep features in small local scopes. this helps prevent unknowingly making spread out bits of code interdependent and is easy to later split into functions if used a lot
- in 98% of cases when you feel the need to comment something, instead add some well-named intermediate variables. These will most likely be optimized out, and won't get stale, as they'll change if functionality does.
- the following appears to be both a valid C comment and valid Markdown:
- be careful of assuming a pure function changes state
/******
```
******/
# heading
- a list of things
- that are interesting
- about the programe
/******
``` */

