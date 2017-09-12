00. Greetings
==============

Hi all, thanks for checking out my project. The software I'm making, 'Geometer', is a desktop application for drawing in a way that emulates traditional construction with compass and straightedge. There's more info on the impetus behind it on the main project page, so I'll try not to repeat myself too much here.

This is just to provide a brief introduction to me, the project, and what I'll be doing with this blog.


Me
--

I'll get the self-aggrandizing part out of the way so we can focus on the important stuff.

I'm Andrew, I've just graduated with a BSc in Ergonomics (Human Factors Design) from Loughborough University. I started learning to program a few years ago, when I asked a comp-sci student I knew how I could get into making games. This was initially using [Processing](https://processing.org/), which has the advantage that you can get into graphical content and interaction straight away. School, mediocre self-organisation, and making things in other mediums meant that I went for large stretches without writing any code. I've dabbled in blacksmithing, electronics, sewing and woodwork, and set up a small leatherwork business in my gap year. You can check out my [portfolio](http://andrewreece.co.uk/) if you're interested. When I was programming, I finished a few small projects while flitting between languages, looking for the Right One. 

Coming back to the allure of the glowing screen, I remembered my brother tipping me off to Casey streaming the process of making a game from scratch. I was familiar with Casey from a few episodes of the Jeff and Casey Show, and immediately got absorbed in watching the 'Intro to C' videos and Handmade Hero proper. After briefly considering trying to follow along in Rust, I decided to just use C. I followed along directly to begin with, then branched out to rasterizing basic shapes and that sort of thing. Once I had just enough of an engine to make something, I set myself the challenge of creating a game in a day. I ended up with the 2-player version of [Twinstick Sumo](https://github.com/azmr/twinstick-sumo). The next day I made one important tweak and added some simple randomized AI behaviours, so you can play by yourself.


The Project
-----------

Motivated by successfully completed a basic (but surprisingly fun!) game, I set my sights a little higher... a little too high. I had some cool ideas for a more complicated game. I made a good start with it, but fairly soon I realised that I was going to have to implement a host of things that I hadn't learned about so far. It made sense to put it on the backburner and refocus on smaller projects that would build my skillset and hopefully be useful/fun in their own right. This is one of those projects.

I got into using a physical compass and straightedge for some woodwork, and tools for them as a design paradigm (not just maths) don't really exist digitally. I've enjoyed playing around with https://sciencevsmagic.net/geo/, which nails the line-circle intersection drawing, but there are key considerations for design that it ignores. For one thing, you can't draw arcs. For another, there is no way to draw anywhere but intersections. This is not to fault it at all, it obviously has a different objective from my project. I think that Geometer can provide 2 main benefits compared to your favourite fancy editing software:

 1) a restriction in tools that encourages you to learn about visual rhythms, flow, and balance of design.
 2) an efficient, uncluttered UI that can make using the few tools you do have very fast.

I'm coding the whole project in C (not a minimal C++ subset) for a similar reasons: the small language forces me to consider concepts rather than waste time obsessing over which language features to use (which I would be prone to do).


The Blog
--------

My main plan for this blog is to explain:
- my user-facing design choices (and hopefully elicit some useful feedback)
- problems I've encountered (and how I've solved them)
- details of my approach to some of the more technical parts (rasterizing circles, arc-line intersection...)

There'll probably be some more general project-related stuff as well, and possibly some comments on geometric construction.

I've got a list of blog titles in mind, but I think I'll be starting with a general overview of how I've laid out the program.

Until next time, if you're interested in learning about design with a compass and straightedge (and sometimes a sector), [By Hound and Eye](https://lostartpress.com/collections/books/products/by-hound-eye) (a play on 'By Hand and Eye', another book by the authors) is a straightforward, practical, and lighthearted introduction to the topic.
