01: Overview
============

This post is meant to provide a general overview of the code structure to contextualize later information.

Platform
--------
As with Handmade Hero, I'm using a win32 layer that opens a dll with the platform-independent interaction and rendering code. The dll is watched and reloaded when it gets recompiled.

Points
------
All the drawing is based on a large array of points, which shapes index into for their constituent components. This allows for easily moving points around both individually by the user and as a group when rotating/panning/zooming the canvas.
These are currently alongside a parallel array of bytes that comprise bit flags for the status of the points, e.g. whether it's an intersection and/or the centre of a circle.

Shapes
------
The main shapes are line segments, circles and arcs. Rays and lines (infinitely long in one and two directions, respectively) might be added later. They are numeric indices into the points array.
- Lines are simply 2 points that are drawn between.
- Circles are also 2 points: one represents the centre/focus point, the other a point on the circumference. The distance between these points represents the radius. 
- Arcs are 3 points: a focus, a point on the circumference that indicates both radius and the start of the arc, and a point indicating the end of the arc.

Software Rendering
------------------
Everything is currently drawn with a software renderer with functions I wrote myself. This was partly just for the challenge, partly to learn better how it all works, and partly because I haven't yet learnt how to use OpenGL.

Debug Systems
-------------
I currently use 4 different methods for providing debug info:
1. A print function at the end of my main loop for providing info on things that don't need a high resolution - mostly looking at mouse position, basis vectors, that sort of thing.
2. A global debug text buffer that can be written to and cleared from anywhere in the code - this has been most useful for tracking things at a higher resolution than once per frame, often once per loop.
3. A simple text logger - I put this in place after the program was crashing in a way that for some reason I couldn't debug in the VS debugger (I forget the details now). I clear it at the beginning of the frame, then use it to log the function, line, file and an additional string at various points. It meant I could check the log afterwards to see what section it was crashing in.
4. Printing a list of all (well... the first 32 or so) points along with the shape points and point statuses.


Next post will get a bit more detailed, looking at how I draw lines.
