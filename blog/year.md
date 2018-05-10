# year

## 1 year of Geometer - Lessons learnt

It's now a little over one year since the first commit on Geometer. The process has taught me a lot, so I thought I'd give a quick overview of some of the things I've learnt, as well as hint at some future blog posts. Feel free to let me know which you'd like to see first.

### Contents

* [Project strategy](year.md#project-strategy)
* [Version Control](year.md#version-Control)
* [Process of writing code](year.md#process-of-writing-code)
* [Invalid array indexes](year.md#invalid-array-indexes)
* [Enums](year.md#enums)
* [Macros](year.md#macros)
* [Saving and loading binary files](year.md#saving-and-loading-binary-files)
* [Linear memory undo](year.md#linear-memory-undo)
* [Geometry/maths](year.md#geometrymaths)
* [Complex state-based user input](year.md#complex-state-based-user-input)
* [UI Feedback](year.md#ui-feedback)
* [Multiple monitors](year.md#multiple-monitors)
* [Resources](year.md#resources)
* [Common/notable bugs](year.md#commonnotable-bugs)

### Project strategy

* It's difficult to find time to work when moving to a new area and starting a new full-time job
  * Weekends always seem like they'll have loads of time, but social/admin obligations often seem to pop up.
  * The most productive time for me seems to be in the mornings, before work and after a cold shower, when I'm freshest.
  * Don't prioritize a few more minutes of programming over exercising/eating/sleeping.

    You will be less productive long-term if you're not healthy.

  * I chose to prioritise writing code over blog posts when I had time \(for better or worse\).
  * Deadlines help focus - I'll now be posting on the blog each Monday for the rest of April.
* It takes me a few minutes to work out where I am and what needs doing from looking at the code cold,

  but whenever I finish, I almost always have an idea of where to go next.

  I've started to adopt Hemingway's unfinished sentence idea for code:

  I leave a statement that will fail to compile, either by missing out a semicolon,

  or just giving myself a few words to describe the next step that aren't commented out.

  I then just compile straight after firing up Vim to see where I'm at.

* Write the simplest thing that will compile in a way you can 'test'.

  It's normally better to take a few testable zigs and zags rather than try to go linearly to the endpoint.

  When \(not if\) that fails, there's much more code that you need to go through to find the bugs.

* Try and test the program on one other machine before releasing, or failing that, with the executable in a directory by itself.

  It's not a great look to have the first comment on a release you're excited about to be "It crashes on opening"

  \(ask me how I know\).

### Version Control

* Use it. Commit regularly.
* Each commit on the master branch should run properly \(and pass tests _ahem, something I need to do more_\), partial checkpoints should go in branches
* It's useful to know what version of Geometer people are using \(particularly when there are bugs\).

  I want this to be both human-readable \(i.e. not a SHA\) and directly identify a commit for me to investigate.

  `git describe --tags` is an easy way to do this.

  * I wrote a post-commit script to add a version tag for each commit \(which I cancel with `ctrl-c` if unnecessary\):

    ```bash
    #!/bin/sh
    # TODO: check that we're on master
    echo
    git log --oneline --decorate --reverse -4
    read -p "Tag name: v0." tagname </dev/tty
    git tag v0.$tagname
    echo
    ```

  * This is added \(somewhat awkwardly\) as a define in the build script:

    ```text
    for /f "tokens=* USEBACKQ" %%f in (`git describe --tags`) do (
          set VersionNum=%%f)
    cl win32_geometer.c ... -DVERSION_NUMBER="%VersionNum%" ...
    ```

  * I then just include this into my assert messages: `"Version number: " VERSION_NUMBER "\n\n"`

### Process of writing code

* Whenever I feel the need to comment, I try and instead add better-named intermediate variables. This normally obviates the need for comments, and won't get stale.
* Just about everything is written inline until it is repeated at least once \(see [John Carmack's comments on long/inline code](http://number-none.com/blow/john_carmack_on_inlined_code.html)\).
  * Logical blocks are scoped with curly braces to minimize accidentally making separate bits of code inter-dependent.
  * This approach also makes it really easy to extract blocks into functions when they're used multiple times.
  * Scopes have a comment describing what they do in a single sentence at the first brace, so that you can fold blocks and see what it happening at an appropriate level of abstraction
* I experimented a bit with Hungarian notation. Not worth adopting wholesale, but I did find it useful to prefix some common constructs, e.g. `i____` for array indexes, `d____` for differences, `c____` for counts, `t____` for time values. I've also used it to distinguish concepts that I'm too lazy to differentiate with proper types, e.g. Points and Vectors - see Joel on Software's post on [Making Wrong Code Look Wrong](https://www.joelonsoftware.com/2005/05/11/making-wrong-code-look-wrong/) for some other examples.

### Invalid array indexes

I was doing some queries into point arrays and started using 0 as an invalid index. This meant that I could do standard boolean conditionals such as `if(SnapPointIndex) { DoThingWithSnappedPoint(SnapPointIndex); }`. It also meant that I had to keep the first point free. I now have a mix of 0-index and 1-index arrays, which has lead to a few off-by-one errors, and just adds unnecessary cognitive overhead. In future I'll be using `enum { INVALID_INDEX = 0xFFFFFFFF };`, which requires a little more typing, but simplifies everything else...

### Enums

* Have a simple incrementing version: this is useful as an index for strings etc, and easy to get a count \(see below\).

  If you're using flags, make a simple incrementing version as well \(see below\).

* Consider having 0 as an invalid type, so you can use `if(Thing.EnumKind){...}` constructs
* You can make groups with outer bounds \(see below\), and check for membership with `(SHAPE_BEGIN_Linear <= Kind && Kind <= SHAPE_END_Linear)`.
* Bitflags and string labels can be made easily \(see X-macros below\)
* Negatives \(this obviously does not apply to 0\) are useful for holding an extra bit of information \(for normal enums\) in a way that's easy to read and write.

  e.g. negative shapes are not shown to the user.

### Macros

* Used judiciously, they can help readability and maintainability.
* X-macros are particularly useful, as long as you can group multiple 'statements' together in your code.

  \`\`\` c

  **define SHAPE\_TYPES           \**

    SHAPE\_TYPE\(SHAPE\_Invalid\) \

    SHAPE\_TYPE\(SHAPE\_Line\)    \

    SHAPE\_TYPE\(SHAPE\_Ray\)     \

    SHAPE\_TYPE\(SHAPE\_Segment\) \

    SHAPE\_TYPE\(SHAPE\_Circle\)  \

    SHAPE\_TYPE\(SHAPE\_Arc\)     \

## define SHAPE\_TYPE\(name\) name,

typedef enum { SHAPE\_TYPES SHAPE\_COUNT, /_ useful if you need to loop over them _/ /_ Groups: _/ SHAPE\_BEGIN\_Linear = SHAPE\_Line, SHAPE\_END\_Linear = SHAPE\_Segment, } shape\_types;

## undef SHAPE\_TYPE

/_ If these weren't sequential, you could still get a count: _/

## define SHAPE\_TYPE\(name\) +1

enum { SHAPE\_ALT\_COUNT = SHAPE\_TYPES };

## undef SHAPE\_TYPE

/_ This assumes that they are sequential _/

## define SHAPE\_TYPE\(name\) \#name,

char \*ShapeTypesStrings\[\] = { SHAPE\_TYPES };

## undef SHAPE\_TYPE

/_ Used as: printf\(ShapeTypesStrings\[Shape.Kind\]\); --&gt; "SHAPE\_Arc" _/

/_ I don't need flags for my shapes, but if I did...: _/

## define SHAPE\_TYPE\(name\) name \#\# \_FLAG = 1 &lt;&lt; name,

typedef enum { SHAPE\_TYPES } shape\_type\_flags;

## undef SHAPE\_TYPE

## undef SHAPE\_TYPES

```text
- Debug - a number of debug features can be added by appending information into static buffers, as in [`live_edit`](https://github.com/azmr/live_edit).
- Common accesses that change approach often: I've changed the way that I access points a few times now: basic array access, through an additional layer of undo state, through a dynamic array, and then with checked bounds access. At some point I realised that it was easier to just use `POINTS(PointIndex)` and redefine that as needed. I'm not particularly consistent with this though...
- Adding 'features' to C:
    - Dynamic array access/creation etc. See [Sean Barrett's stretchy buffer code](https://github.com/nothings/stb/blob/master/stretchy_buffer.h), and [Per Vognsen explanation as part of Bitwise](https://bitwise.handmade.network/episode/bitwise/bitwise002#762)
    - a foreach construct (see below)
    - compile-time 'functions' (given that C doesn't have constexpr). e.g. `#define FLAG(x) (1 << x)`

## Saving and loading binary files
(I'll be doing a blog post just about this, particularly on issues around versioning)
- The process of doing this helped me better think about data as just a series of bytes in memory - a useful perspective to be able to take.
- Enum labels for file sections etc must be unique and unchanging after a version goes public - make them append-only in a separate file
- The file format, particularly the header should let you:
    - Determine that it's your filetype
    - Tell what version it is so that you can read it the right way
    - Error check the file (for corruption)
    - Read a useful amount of data into memory in one go (most likely the whole file)

## Linear memory undo
- My initial approach to memory was to just keep a rolling buffer of states.
This is safe and easy, but is both O(n^2) and makes accessing memory verbose.
- I had a rethink in terms of user/program actions (I'll write more about this at some point).
    - I save just enough to reconstruct the state at each action going either forwards or backwards in history.
    - This granularity doesn't match a user's mental model, so I tag some of the actions as 'user actions'
    (whether the enum `Kind` for the action is positive/negative) and undo/redo to these.

## Geometry/maths
- Software rendering shapes has some interesting challenges (see [my thought process for drawing lines](https://geometer.handmade.network/blogs/p/2603-02._line_drawing))
- Finding intersections was harder than I expected.
A couple of game maths books and some online searches did me fine though (I'll write some examples of the harder parts).
- My intuition for linear algebra has improved a lot by working with it.
- Working out the maths for bases by hand both helped my understanding,
and allowed me some optimisations from not needing the most general case.

## Complex state-based user input
The first pass on UI involved switching on the input used and dealing with ad-hoc state within these levels.
This became complex and order-dependent as the number of states and input grew.
Switching on more reified levels of state and then dealing with actual input simplified this a lot.
I deal with a couple of inputs up-front that relate to all states - notably escaping from drawing states and panning around. 
The basic structure is roughly as follows:
``` c
if(IsPressed(C_Cancel))
{
    ReturnStuffToNormal(State);
    State->InputMode = MODE_Normal;
}

else if(IsPanning)
{
    /* Pan */
}

else switch(State->InputMode)
{
    case MODE_Normal:
    {
        if(IsPressed(C_StartLine))
        {
            if(C_PointsOnlyMod.EndedDown)
            { State->InputMode = MODE_DrawPoint; }
            else
            {
                State->LineStartPoint = CanvasMousePos;
                State->InputMode = MODE_DrawLine;
                goto input_mode_drawline;
            }
        }
        /* ... */
    } break;

    case MODE_DrawLine:
input_mode_drawline:   /* calculation for the preview is done here. This is an easy way to avoid  */
    {                  /* the preview being drawn with junk data without adding extra calculation */
        /* ... */
    } break;

    /* ... */

    default:           /* always have a default case for switches, even if it should never be hit */
    { Assert(! "Unknown input mode"); }    /* the ! makes the assertion fail */
}
```

### UI Feedback

\(I'll be writing more about this\)

* Make it obvious what the current state of the system is.
* Sudden jumps by continuous variables can be confusing.
* Lerp \(interpolate\) between states to make clear both what changed and how it did so.
  * This can be very fast and still be effective.
  * Use a float between 0.0f and 1.0f to track animation state. Set this to 0 on triggering the animation.
  * You probably want to clamp to these values and/or assert if it's outside. I've had a couple of bugs from very large t-values making floats get so big that `x+1.f == x`, and so some loops never finish.
  * 2 easy uses for lerps:

      1\) the linear interpolation: fix the start and target value when triggering the event.

      2\) the decaying interpolation: do the same, but each frame reset the start value to be the current position.

  * There are a bunch of fancier motion curves that you can use.
    * A simple ease-in and ease-out curve that looks good and is really quick to compute is SmoothStep:

      ```c
      float SmoothStep(float t) { return 3*t*t - 2*t*t*t; }    /* 3t^2 - 2t^3 */
      ```

    * See [http://easings.net/](http://easings.net/) for other examples.

      There is C code for these here: [https://github.com/warrenm/AHEasing/blob/master/AHEasing/easing.c](https://github.com/warrenm/AHEasing/blob/master/AHEasing/easing.c)
  * See [Casey's video on interpolation](https://www.youtube.com/watch?v=S2fz4BS2J3Y).

### Multiple monitors

They're a thing - don't forget to account for them.

### Resources

* On Windows, use .rc files for icons
* Use xxd for everything else.
  * xxd can output an array of bytes in a C header file along with the length of the array.
  * Use this to easily include any arbitrary resource.

### Common/notable bugs

* Arena out of bounds - caught by my runtime bounds checking.
  * Occasionally an arena problem, e.g. the length hasn't been set.
  * Normally something else has gone wrong in the algorithm, and this is the first thing that triggers.
* Float precision
  * Very small differences in value from float imprecision meant that I was unintentionally adding multiple points to almost exactly the same place.

    To fix it, I had to check that float values for point locations are checked within a given epsilon \(small error range\).

    Moving to fixed point might be a better solution, but I haven't tried it yet.

  * As mentioned above, floats getting so large \(normally from another bug\) that `x+1 == x`.

    Some of my drawing loops are based on adding 1 to a vector dimension, and so never finish if the values get too big.

    In some cases integer types might be more appropriate, otherwise you can just assert for this.
* Not keeping cached versions of information in sync with the canonical version.

  Avoid storing a copy of data or trivially-derived data \(e.g. LengthInMm, LengthInM\) unless you have a good reason.

* The debugging process itself\(!\)
  * I sometimes forget about the first thing I've tried when figuring out why something isn't working, so after hardcoding a test value, I try some other stuff and take a while to realise why it's not varying as expected.

    I've already gone through things in the debugger and feel like I've built a representative mental model, but this doesn't include my ad-hoc tests.

    These are probably the least satisfying bugs to find - you've just been wasting your own time.
* Copy-pasting code
  * In particular copying for loops on arrays with similar names and contents, then not changing all of the names.
    * I fixed this with some `foreach` macro constructs, which also make the code more readable. The difference is more pronounced when they're nested with similar names.

      ```c
      /* before: */
      for(uint iTestPoint = 0; iTestPoint < Len(State->SelectedPoints); ++iTestPoint)
      {
        uint TestPoint = Pull(State->SelectedPoints, iTestPoint);
        if(PointInAABB(POINTS(TestPoint), SelectionAABB))
        { Remove(&State->SelectedPoints, iTestPoint--); }
      }

      /* after (gives the index with an i prefix): */
      foreach(uint, TestPoint, State->SelectedPoints)
      {
        if(PointInAABB(POINTS(TestPoint), SelectionAABB))
        { Remove(&State->SelectedPoints, iTestPoint--); }
      }
      ```
* Using `typeof(type)` rather than the longer but less brittle `typeof(Struct->Member.SubMember)`.

  The former can fail in non-obvious ways if you change the variable's type.

* The bug that took me the longest to find showed up as text in the memory for my input handling code.

  I found a few things that needed fixing in the process of hunting this down.

  The main problem was that I was reallocating a void-pointered in an arena, and hadn't updated a matching typed pointer that I was using.

  A lovely combination of cache invalidation, improper memory access, and a non-obvious trigger \(enough intersections to cause reallocation\).

  I now used typed arenas \(which I make with a macro\) that union a void pointer, a u8 pointer and the type I want:

  ```c
  /* ... */
  union {
    void *Base;
    u8 *Bytes;
    user_type *Items;
  }
  /* ... */
  ```

  I should probably transition this to dynamic arrays/stretchy buffers \(as described above\), as that is how I'm using the arenas.

### Conclusion

Ok, I'm over 2500 words now... this is probably getting long enough. Please let me know if any of these are useful to you or you'd like me to write more about them.

