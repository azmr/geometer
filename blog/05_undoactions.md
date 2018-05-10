# 05\_undoactions

% Undo, redo and units of interaction

I don't think I'm doing anything revolutionary with my undo system, but hopefully making the reasoning behind it very explicit will elucidate any missing steps in the reader's or my thinking.

A large part of this is trying to define 'units of interaction' for the user and then matching that to what can be robustly determined and stored in the software.

I describe this here as a roughly linear sequence for the sake of clarity. In reality the process was more iterative, and broadly follows Casey Muratori's [Semantic Compression](https://caseymuratori.com/blog_0015) approach.

_Throughout the article, I use 'units of interaction', 'units of action', 'task units' etc interchangeably._

## Contents

* [Contents](05_undoactions.md#contents)
* [Intro](05_undoactions.md#intro)
* [Naive undo states](05_undoactions.md#naive-undo-states)
  * [Why this is not good enough](05_undoactions.md#why-this-is-not-good-enough)
* [Actions](05_undoactions.md#actions)
* [User Actions](05_undoactions.md#user-actions)
  * [Theory](05_undoactions.md#theory)
    * [Action Cycle and Principles](05_undoactions.md#action-cycle-and-principles)
    * [Error types](05_undoactions.md#error-types)
    * [Action hierarchy](05_undoactions.md#action-hierarchy)
  * [Example action set](05_undoactions.md#example-action-set)
  * [User-centred goals](05_undoactions.md#user-centred%20goals)
    * [Functionality goal - cost saving](05_undoactions.md#functionality-goal-cost-saving)
    * [Consistency](05_undoactions.md#consistency)
    * [Mapping](05_undoactions.md#mapping)
  * [Error prevention](05_undoactions.md#error-prevention)
* [Technical considerations](05_undoactions.md#technical-considerations)
  * [Objectives](05_undoactions.md#objectives)
  * [Triggers and choice of representation level](05_undoactions.md#triggers-and-choice-of-representation-level)
* [Representation](05_undoactions.md#representation)
  * [Undo](05_undoactions.md#undo)
  * [Redo](05_undoactions.md#redo)
  * [Collection data structure](05_undoactions.md#collection-data-structure)
  * [Resultant structs](05_undoactions.md#resultant-structs)
  * [Mismatch between user actions and software actions](05_undoactions.md#mismatch-between-user-actions-and-software-actions)
  * [Function Layout](05_undoactions.md#function-layout)
    * [Action Functions](05_undoactions.md#action-functions)
    * [Undo/Redo functions](05_undoactions.md#undoredo-functions)
  * [Required restructuring](05_undoactions.md#required-restructuring)
* [Future possibilities for actions](05_undoactions.md#future-possibilities-for-actions)
* [Conclusion](05_undoactions.md#conclusion)
* [Further reading](05_undoactions.md#further-reading)

## Intro

If you've used any editing software, for text, images or something else, you've almost certainly had to use the undo function. Its purpose is fairly clear. Simply put, people commit errors all the time, and so it's necessary to have a way to fix them.

On top of this, having a robust undo system allows users to have enough confidence in your software to rapidly experiment, safe in the knowledge that they can just undo if it turns out wrong.

## Naive undo states

My first approach was to have a fixed array containing snapshots of the entire state at successive points in time. Once the array was full, the new states would wrap to the beginning. Each state had a separate memory arena assigned. Undos and redos would just change the index of state looked at, and base all of the interactions, rendering etc on that.

The state would be saved at various points in the interaction processing that seemed appropriate. Because the entire state was saved this could be a bit sloppy, because all the information required was in that one state.

### Why this is not good enough

The naive approach wasn't going to work long-term, in large part because I wanted to have the option of indefinitely long undo history. It was doomed for replacement because it:

* grows exponentially - this is the main reason. Every change also stores all the previous changes.
* is awkward to access in code \(`State->Draw[State->iCurrentUndoLevel].Points[State->iActivePoint]`\),

  which would be further complicated by the addition of layers.

* is more difficult to implement than the improvements described later:
  * partly due to lessons learnt by doing this first.
  * it has general fiddly bits in terms of:
    * keeping track of the start and end of the circular buffer.
    * special-casing before the array was full.
    * memory management - making sure that the next state was large enough to contain any changes from the current state.

## Actions

The change in thinking was that rather than storing absolute states, I should store the difference between them. The things that cause the change in state are the actions by the user. Broadly speaking, each time the user does an action, I should record what it is, then be able to undo/redo it at will.

But what counts as a user action?

## User Actions

### Theory

It's worth taking a slight detour into UX/ergonomics, as we're stepping into their wheelhouse. Not all of this is applied directly in this article, but I think that it provides background/context and informs intuitions in a useful way.

That said, this section isn't necessary to understand the rest of the article, so feel free to skip it if you want a shorter read.

#### Action Cycle and Principles

Don Norman's 7 step model of action/action cycle provides a breakdown of interaction from the user's perspective, which allows us to better target corrections.

> **Goal formation stage**
>
> 1. Goal formation.
>
> **Execution stage**
>
> 1. Translation of goals into a set of unordered tasks required to achieve goals.
> 2. Sequencing the tasks to create the action sequence.
> 3. Executing the action sequence.
>
> **Evaluation stage**
>
> 1. Perceiving the results after having executed the action sequence.
> 2. Interpreting the actual outcomes based on the expected outcomes.
> 3. Comparing what happened with what the user wished to happen.
>
> These lead to 4 principles of good \(UI\) design: 1. Visible system state 2. Consistent conceptual model 3. 'Intuitive' mappings between action and consequence 4. Continuous feedback of past and potential actions
>
> -- [Don Norman \(Action Cycle - Wikipedia\)](https://en.wikipedia.org/wiki/Human_action_cycle)

#### Error types

Undoing is typically for the purpose of correcting \(potential\) errors. I won't go too far into this, but it's useful to have some vocabulary and context for thinking about errors. Different error types may require different prevention and recovery methods

* Mistakes - conscious decision to do something that turns out to be wrong i.e. forming the wrong goal.
* Slips - unconscious automatic behaviour resulting in unintended consequences.
  * Capture error - a common script overriding the intended action \(e.g. driving to work instead of to the shops\).
  * Description error - do the right thing with the wrong object.
  * Data-driven error - information from the world intruding into an action.
  * Associative activation error - Freudian slips \(roughly\).
  * Loss-of-activation error - forgetting.
  * Mode error - do the otherwise right thing in a mode that interprets it differently.

Errors can also be categorised as:

* Commission - did something incorrectly.
* Omission - didn't do something that should have been done.

\[Don Norman \(Design of Everyday Things\)\]

#### Action hierarchy

A common start point for human factors/ergonomics analysis is a Hierarchical Task Analysis. This involves recursively breaking down the component tasks required to achieve a larger task/goal based on observations of people performing the task. The end result is similar to a hierarchically-organized graph of functions that call a number of other functions, which each call a number of other functions... The main takeaway is that there are multiple possible levels of detail that could be appropriate to consider 'an action', as you'll see in the upcoming example.

### Example action set

It's often easiest to design a user interface if you can reference a concrete example of something a user might do. This is the UI/UX equivalent of

> _**Casey Muratori**_
>
> Always write the usage code first \(broadly talking about API design\).

I'll be running with this example for the rest of the article. It's not comprehensive, but other actions follow a similar line of reasoning. As already mentioned, tasks can be broken down in granularity into a hierarchy. Here's a slice of an example goal/task hierarchy for a drawing in Geometer:

```text
0.  Draw a cathedral in profile view                 /* Entire drawing */
    ...
    4.  Draw the east wing                           /* Section of drawing */
        ...
        4.6.  Draw an arch                           /* Architectural component */
            4.6.1.  Draw the left-hand arc           /* Discrete shape */
                4.6.1.1.  Add a focus point          /* Shape component */
                    4.6.1.1.1.  Move the cursor to the bottom-right point  /* Direct input */
                        4.6.1.1.1.1.  Grip mouse     /* Joint movement */
                        4.6.1.1.1.2.  Retract shoulder girdle
                        4.6.1.1.1.3.  Extend, laterally rotate and adduct shoulder
                        4.6.1.1.1.4.  Allow passive elbow flexion
                        4.6.1.1.1.5.  Ulnar deviate wrist

                    4.6.1.1.2.  Check that the intended point is snapped to
                    4.6.1.1.3.  Click and release the left mouse button at that point

                4.6.1.2.  Start the arc
                    4.6.1.2.1.  Move the cursor to the bottom-left point
                    4.6.1.2.2.  Check that the intended point is snapped to
                    4.6.1.2.3.  Click and hold the left mouse button at that point

                4.6.1.3.  Finish the arc
                    4.6.1.3.1.  Move the cursor to the top-centre point
                    4.6.1.3.2.  Check that the intended point is snapped to
                    4.6.1.3.3.  Release the left mouse button at that point

            4.6.2.  Draw the right-hand arc
                ...
```

Note that:

* If asked "what are you doing?", you could accurately answer at any level;

  there are multiple viable levels of analysis - you can be as detailed or abstract as you like.

* The action cycle occurs at each level from direct input up
* Errors could occur at any level

In magical UI land, the unit of interaction should be at whatever level in the hierarchy the user thinks it is at the time. I'm not sure to what extent you can implement variable hierarchical levels of undo, let alone communicate it in a way that makes sense to the user. As such, we'll start with a single level-of-detail action unit that is good enough for most purposes. This may be extensible to get closer to magical UI land, as hinted at in [Future possibilities for actions](05_undoactions.md#future-possibilities-for-actions).

It's also worth noting that other interface features and external programs can fill the role of dealing with high-level units of interaction, particularly saves, backups and version control.

### User-centred goals

As already mentioned, the primary purpose of `undo` is to correct mistakes. This is not the only purpose, however - it's also useful for:

* reminding users of the sequence of actions they've just taken as a prompt for what is needed next.
* repeatedly returning to a common point after temporarily exploring multiple elaborations from it.

There are a few axes on which to evaluate an undo/redo system.

* Functionality goal - cost-saving
* Consistency goals:
  * Self-consistency
  * Consistency with other programs \(less important, but better if possible\)
* Mapping

#### Functionality goal - cost saving

An action should represent some notable cost to the user: in precision or time or effort.

If it's just as easy to actually redo the action as to use the `redo` input, there's not that much point in having the control. Too small action sizes are similarly irritating for undo as well: consider a text editor that removes one letter per undo. With this in mind, we'd like to define task units higher up the hierarchy to save as much effort as possible per undo/redo. We don't, however, want them to be too high up, otherwise we'll unavoidably skip back past too many valid actions, which will then require manual redoing.

One way to think about this is, "if the user realises they made an error here, how many times will they have to undo, and how much will they have to manually redo in order to return to the state immediately before the error".

The aim here is to minimize estimated total effort in error recovery

An empirical question that would be useful to know the answer to is "At what level do most errors occur?". Without an answer, it seems better to lean too fine-grained than too coarse-grained - it's much harder to increase resolution than decrease it.

#### Consistency

Self-consistency:

* Undo and redo should have the same unit of action.
* Units of action should be the same size.

External consistency:

* I'll be using Ctrl+Z for undo, Ctrl+Y/Ctrl+Sh+Z for redo, as with other programs.
* Units of interaction will represent a similar time investment.
* The user should know in advance what will disappear when they undo.
  * This minimizes time spent in the evaluation stage of the action cycle.
  * Actions should be demarcated in some way, e.g. with distinct start and end points.

#### Mapping

Quality of mapping can be evaluated of in terms of how well the application matches what the user expects from the first time they undo. Consistency plays a large part of expectations in future interactions How well does it fit their initial mental model?

In order to make a clear mapping, actions should correspond fairly directly to a pattern of inputs rather than some downstream changes.

### Error prevention

I won't go into this too much here as it's not really in the intended scope of this article, but helping to minimise stupid errors is important. Errors from bad design:

* waste your user's time
* make the user frustrated with themself and your product
* undeservedly diminish the user's sense of competence/mastery
* reduce the chance of continued use of your product

_Note the qualifier 'from bad design'. Some increased likelihood of error occurs as a side-effect of flexibility and other positive qualities._

Mode errors are among the most insidious. Vim is a classic example for this: users try to type but the editor seems to go haywire because it is not in 'input' mode. It is not immediately apparent which mode vanilla Vim is in, which worsens the problem.

I try to avoid this in Geometer by making changes in mode obvious, discrete and brief. I still have more work to do to improve this.

## Technical considerations

I have not yet talked about computer actions or representation. Without a good idea of what we're aiming for, it's easy to prematurely limit the technical representation to the first/easiest thing we think of. We should be aware of technical limitations, but we want a user-focused goal to shoot for, even if we fall short.

### Objectives

Nothing novel here, but some things to keep in mind:

* Quick to read/write
* Not memory intensive
* Deterministic in terms of:
  * Interpreting a pattern of input as 'an action'
  * Recreating that action
* Able to traverse forwards and backwards
* Changes are self- and externally-consistent, e.g.
  * Multiple undos followed by an equal number of redos returns to the same place
  * A redo makes the same state changes that doing the action manually would \(excluding any changes to the action array\)

### Triggers and choice of representation level

Working up from the lowest level \(which incidentally tends to be how humans search for the cause of errors\), let's consider how well the task levels fit the objective of determinism:

* Joint movement  - no way to directly interpret
* Direct input - difficult to tell how much movement to classify as an action; state doesn't necessarily change
* Shape component - needed to reconstruct shapes, a single function is executed for each
* Discrete shape - this is demarcated by leaving the normal state, going through the drawing process, then returning to the normal state again

  \(see the explanation of state-based input-handling in my [1 year-in article](https://geometer.handmade.network/blogs/p/3048-1_year_of_geometer_-_lessons_learnt)\).

* Architectural component - this is drawing-specific, and without additional user tagging would need some fancy heuristics.

  This \(and higher levels in the hierarchy\) are thus ruled out as non-deterministic.

So we're left with shape components and discrete shapes as viable levels for units of interaction. The shape components need to be stored, but the input demarcation of discrete shapes seems more appropriate for a user's mental model. Additionally, returning to part-way through a shape would leave the program in a non-normal state, encouraging mode errors.

For Geometer, shape components have the useful property of being the same as other discrete-shape-level actions: adding points. That is to say that shape components are a subset of the discrete shapes. 'Hierarchy levels' is a [leaky abstraction](https://en.wikipedia.org/wiki/Leaky_abstraction)! This allows me to somewhat cheat here: I can store both.

This should allow for something that broadly matches user-based objectives.

Now we have an idea of what level we want to capture, how best to represent this?

## Representation

The action representation must provide sufficient information for traversal in either direction.

### Undo

Some changes are directly invertible, i.e. the way to reverse can be found from how they work normally. A change of basis, for instance, is primarily a matrix multiplication. This can be reversed by multiplying by the inverse matrix. Other changes need additional information: points only need a position to be added, but its index in the `Points` array is needed to reverse this. The inverse is true for deleted points.

### Redo

This mirrors adding/removing shapes, and is perhaps even easier. This should just do the same thing that the user did, so the initial handling and the redoing can be collapsed into one function.

Not all actions have the same 'remove' equivalent, so `Remove___` actions are treated as separate entities rather than inversions of the original action \(in the enum\).

### Collection data structure

I like arrays:

* They're easy to write bug-minimal code for \(particularly with a `foreach` construct\).
* They are fast to traverse:
  * They're cache-friendly \(all the data is in contiguous memory\).
  * You often get an efficiency/speed bonus from [cache prefetching](https://en.wikipedia.org/wiki/Cache_prefetching).
* Elements at arbitrary indices can be accessed in O\(1\) time.
* Bounds checking can be added and toggled pretty easily.
* They're already serialized for saving to disk.

Unless there's a good reason otherwise \(see [Future possibilities](05_undoactions.md#future-possibilities-for-actions) I will stick with them.

The data access pattern here is normally going to be linearly progressing either forwards or backwards with the occasional random access, which is a good fit for arrays.

They'll need to be resized as more and more actions are added, so it'll have to be a dynamic array \(allocated and reallocated on the heap\) rather than simple fixed-size static arrays. See [Sean Barrett's stretchy buffer code](https://github.com/nothings/stb/blob/master/stretchy_buffer.h) and [Per Vognsen explanation of dynamic arrays as part of Bitwise](https://bitwise.handmade.network/episode/bitwise/bitwise002#762).

### Resultant structs

The data needed for different actions are of comparable size, but not all exactly the same in contents. This is a textbook case for using [discriminated unions](https://en.wikipedia.org/wiki/Tagged_union). That is, indicating the kind of action with an enum/integer ID, then interpreting the following data based on that kind.

My current representation of shapes is as a union, but it needn't necessarily continue like that. As a result I have to discriminate between shape types at the action level. It would be redundant to also have the shape `Kind` stored in both the shape and the action. I have to have the action `Kind`, so I only include the union for shapes.

```c
typedef enum action_types {
    ACTION_Reset   = 0,
    ACTION_Line    = SHAPE_Line,    /* 1 */
    ACTION_Ray     = SHAPE_Ray,     /* 2 */
    ACTION_Segment = SHAPE_Segment, /* 3 */
    ACTION_Circle  = SHAPE_Circle,  /* 4 */
    ACTION_Arc     = SHAPE_Arc,     /* 5 */
    ACTION_Point,
    ACTION_RemovePt,
    ACTION_RemoveShape, /* This will probably turn out to be insufficient */
    ACTION_Basis,
    ACTION_Move,

    ACTION_Count,
    ACTION_SHAPE_START = ACTION_Line,
    ACTION_SHAPE_END   = ACTION_Arc
} action_types;

typedef struct action_v2 {
    i32 Kind;   /* 4 bytes  */
    union {     /* 20 bytes */
        struct reset_action {
            u32 iAction;    /* where I'm resetting back to */
            u32 cPoints;    /* maybe needed for future stuff */
            u32 cShapes;    /* maybe needed for future stuff */
            f32 Empty_Space_To_Fill_1;
            f32 Empty_Space_To_Fill_2;
        } Reset;

        struct shape_action {
            u32 iShape;
            u32 iLayer;
            shape_union;    /* Contains point indices for line/circle/arc */
        } Shape;

        /* struct move_action Move; - temporarily retired while I figure some stuff out */

        struct pt_action {
            u32 iPoint;
            u32 iLayer;
            v2 po;
            u32 Empty_Space_To_Fill;
        } Point;

        basis_v2 Basis;
    };
} action_v2;
```

* \*If you're wondering what the `v2` is about, please see my previous article on

  [Designing, saving and loading a forward-compatible binary file format](https://geometer.handmade.network/blogs/p/3077-03._designing,_saving_and_loading_a_forward-compatible_binary_file_format).\*

* _The actiontypes enum is actually implemented as an X-macro, as described in \[my year in review article\]\(_[https://geometer.handmade.network/blogs/p/3048-1\_year\_of\_geometer](https://geometer.handmade.network/blogs/p/3048-1_year_of_geometer)_-\_lessons\_learnt\), under 'Macros'_

### Mismatch between user actions and software actions

Given what I've said so far, an example of a user action could be adding an arc. The process of drawing the arc also adds up to 3 new points \(if they weren't there already\): the focus, the start of the arc and the end of the arc. These need to be added as actions so that the software knows what to undo/redo, but as I've already said, the user action is adding an arc - I don't want the user to have to undo 4 times \(3 points + 1 shape\) for 1 conceptual action.

My solution to this is pretty simple: I negate the `Kind` value for all but the last action. The last action is thus conceptually the 'user action' and I have to make sure the actions are ordered accordingly.

I end up with shape components as the software actions and discrete shapes as user actions.

There's an additional wrinkle that the components may already exist. Ways to deal with this are:

* Not add them \(my current strategy\) - saves memory and processing, but may make some things harder.
* Add them and notate them in some way \(e.g. high bit in enum\)
* Add them without notation, leave ignoring them to my idempotent `AddPoint` function, which 

### Function Layout

#### Action Functions

As alluded to earlier, more rigour is required for action/delta-based rather than state-based undos because you need to be careful that all the necessary changes are captured. I do this by attaching the action-adding code to the code that makes the relevant change.

Each software action has 2 related functions:

1. Making whatever changes are necessary without pushing a new action.

   This is used in the undo/redo code, to avoid corruptions of the action array.

2. A wrapper function that calls \(1\) then adds an action if necessary.

   This is used in the normal input/interaction handling code.

Below is the basic structure of this for shapes:

```c
// returns true if shape is new (i.e. action to be added)
b32 AddShapeNoAction(state *State, shape Shape, uint *iShapeOut) {
    Assert(ShapeIsValid(Shape));
    b32 ExistingShape = 0;
    uint iShape;

    // check if shape exists already
    for(iShape = 1; iShape <= State->iLastShape; ++iShape) {
        if(ShapeEq(Shape, State->Shapes[iShape])) {
            ExistingShape = 1;
            break;
        }
    }

    // add new shape if needed
    if(! ExistingShape) {
        Push(&State->maShapes, Shape);
        iShape = ++State->iLastShape;
        AddAllShapeIntersects(State, iShape);
    }

    *iShapeOut = iShape;
    return ! ExistingShape;
}

// returns position in Shapes array
uint AddShape(state *State, shape Shape) {
    uint iShape = 0;
    if(AddShapeNoAction(State, Shape, &iShape)) {
        action Action = ActionShape(iShape, Shape);
        AddAction(State, Action);
    }
    return iShape;
}
```

Non-user actions come before the user action. Here's how I make sure that happens for adding a line segment:

```c
// returns position in Shapes array
inline uint AddSegment(state *State, uint P1, uint P2) {
    shape Shape;
    Shape.Kind = SHAPE_Segment;
    Shape.Line.P1 = P1;
    Shape.Line.P2 = P2;
    return AddShape(State, Shape);
}

inline uint AddSegmentAtPoints(state *State, v2 P1, v2 P2) {
    uint iP1 = AddPoint(State, P1, -ACTION_Point);   /* these will add new actions if the points are new */
    uint iP2 = AddPoint(State, P2, -ACTION_Point);   /* negative -> non-user action */
    return AddSegment(State, iP1, iP2);
}
```

#### Undo/Redo functions

The undo/redo functions are divided into a function for software actions and a loop repeats these until a user action is reached.

The simple form of undo/redo works at the component level by switching on the type, then doing the action/its reverse as needed. Here's an abridged version:

```c
void SimpleUndo(state *State) {
    action Action = State->Actions[State->iCurrentAction];
    switch(AbsInt(Action.Kind)) {    /* whether or not it is user-level is dealt with elsewhere */
        /* ... */

        case ACTION_RemoveShape:
        {
            shape *Shape = ShapeFromAction(State, Action);
            uint iShape  = 0;
            AddShapeNoAction(State, Shape, &iShape);
        } break;

        case ACTION_Segment:
        case ACTION_Circle:
        case ACTION_Arc:
        {
            RemoveShape(State, Action);
        } break;

        /* ... */

        default: { Assert(!"Unknown/invalid action type"); }
    }

    --State->iCurrentAction;
}


void SimpleRedo(state *State) {
    action Action = State->Actions[++State->iCurrentAction];
    int UserActionKind = AbsInt(Action.Kind);
    switch(UserActionKind) {
        /* ... */

        case ACTION_RemoveShape:
        {
            RemoveShape(State, Action);
        } break;

        case ACTION_Segment:
        case ACTION_Circle:
        case ACTION_Arc:
        {
            shape *Shape = ShapeFromAction(State, Action);
            uint iShape  = 0;
            AddShapeNoAction(State, Shape, &iShape);
        } break;

        /* ... */
    }
}
```

The user-level undo/redo then works as follows:

```c
b32 Ctrl_Z = Keyboard.Ctrl.EndedDown && Press(Keyboard.Z);
b32 Ctrl_Y = Keyboard.Ctrl.EndedDown && Press(Keyboard.Y);
b32 ShiftHeld = Keyboard.Shift.EndedDown;

if(Ctrl_Z && ! ShiftHeld &&
   State->iCurrentAction > 0)
{ // UNDO
    do { SimpleUndo(State); }
    while( ! IsUserAction(State->Actions[State->iCurrentAction]));
}

if((Ctrl_Y || Ctrl_Z && ShiftHeld) &&
    State->iCurrentAction < State->iLastAction)
{ // REDO
    do { SimpleRedo(State); }
    while( ! IsUserAction(State->Actions[State->iCurrentAction]));
}
```

### Required restructuring

I didn't initially distinguish between user-added points and intersections. Adding intersections into the action array would have added a lot of confusion once deleting shapes was working. There would be a non-deterministic number of unnecessary additions for every shape added.

Intersections now only become user points once they have been drawn with. They are calculated based on shape positions and stored in their own dynamic array. After an undo/redo that involves a shape, they are recalculated.

## Future possibilities for actions

* One of the things I like about the action paradigm closely following the user-input model is that it seems like it would lend itself well to macro/function creation.
* The changes at this level can be fairly easily visualised in a history thumbnail.
* Although unwise to do automatically, the user might find it helpful to manually group actions together into what would become a hierarchy.
* Actions could be laid out as a tree, so rather than new actions after undos overwriting all potential redos, they're just written on an new branch.
* Have variable-sized action data structures. This would save a small amount of memory but would be more error-prone and could make some operations slower.

## Conclusion

In this article we:

* Looked at human factors theory for backing in terms of action cycles and error types.
* Considered the factors that would make a unit of interaction appropriate for users.
* Determined some technical objectives for an action representation.
* Came up with a high-level description of what would fit technical and user objectives.
* Determined appropriate data structures for actions.
* Ensured that actions would be captured at the right times.
* Created a simple and user-level undo/redo.
* Speculated on potential future developments.

I think this is quite thorough from a theoretical perspective, but would ideally have had some more end-user involvement. Based on use by me and my single test-user \(my girlfriend\), and given the similarity of my solution to other undo systems, I'm happy with the results so far.

Writing this has helped me clarify some of my own thoughts on the topic and simplify some of the code. Hopefully this has provided you some food for thought in terms of undo systems and interaction design more generally.

Please let me know if any sections were particularly helpful, or if they need any clarification.

## Further reading

_N.B. I get a kickback if you buy directly through one of these links \(on Amazon UK\). If you'd rather not do that for whatever reason, they're easy to find online._

* ['Design of Everyday Things' - Don Norman](https://amzn.to/2r7DPLZ). This article was heavily influenced by the ideas in this book.

  I highly recommend it for anyone making anything that people will interact with \(yes, that's intentionally broad!\).

* ['Human Error' - James Reason](https://amzn.to/2FmvqsZ). A classic work for a deeper dive on error than DOET.

