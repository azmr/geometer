Project background
==================
Learning some woodworking a few months ago, I got into some design using a compass and straightedge.
The tools required for this are very simple, and as such have been around for thousands of years, used for physical design from fine cabinet-making to large-scale architecture.
Despite their simplicity, combinations of arcs and straight lines allow for a very powerful method for design and encourage in the designer an understanding of proportion and rhythms.
I was impressed when I came across https://sciencevsmagic.net/geo/ a little while ago, and after gaining a better understanding of the fundamentals of compass/straightedge construction with *By Hound and Eye* (Geo Walker and Jim Tolpin), I decided I should try to make a more complete desktop application to emulate this geometrical method of construction.

Goals
=====
This is the first 'proper' desktop application I have attempted to build, so I'm likely to have overlooked some key/time-consuming features.
With that said, these are the main goals for the project:

Functional goals:
-----------------
- UI that emphasises drawing ease and speed with minimal input (primarily mouse-based)
- Draw points, circles, arcs and lines
- Drawing between intersections (and along lines) maintained as a central concept as with manual construction
- Scalable, movable, rotatable canvas
- Multiple layers
- Export drawings in useful formats (.svg, .png...)

Technical goals:
----------------
- Ability to create 'large' (more precise size TBD) drawings without slowdown
- Programmed in C with minimal library usage (e.g. drawing functions all created 'by hand')
- Support Windows from the start, probably support Linux as well

Project goals:
--------------
- Write blog entries describing/explaining the design and implementation of major features (being fairly new to this, these will probably focus on overcoming the various problems I encounter along the way)
- Provide access to most, if not all, source code (all the project-specific stuff is currently at https://github.com/azmr/geometer)
- Provide at least the basic program free. I haven't decided whether a paid, 'advanced' version would be appropriate
- Target user personas: woodworker designing furniture, mathematician demonstrating various geometric constructions

There are also a number of other ideas that I've been playing around with, but haven't yet decided upon whether they are worth including in the project. I'm sure these will be the topic of future blog posts.

Roadmap
=======
In roughly implementation order:
DONE:
- *Draw lines*
- *Draw circles*
- *Index circles & lines on points*
- *Snap to points (optional but on by default)*
- *Line-line intersection*
- *Line-circle intersection*
- *Circle-circle intersection*
- *Delete points*
- *Move points*
- *Undo function*
- *Finish adding arcs*
- *Panning*
- *Zooming and rotation*
- *Animation between rotations*
- *Unlimited points/shapes*

TO DO:
- File saves/loading
- Basic .png exports
- Faster way to implement multiple arcs/circles of the same radius
- Add text labels for points
- Add layers and decide on how they can interact
- Improved exports
- Improve graphical representation (anti-aliasing, experiment with sweeps when moving the mouse quickly...)
- Consider hardware rasterizer
- Decide on and use a spatial partitioning method
- More advanced undo system?
- Rays and/or lines
- New and exciting features... (animation between states?! Who knows? Only time will tell...)
