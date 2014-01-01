AFK
===

("Automated Flight Kit" or
 "Away From Keyboard", or whatever else you prefer.)

AFK is an art project.  It's an attempt to generate pretty, diverse 3D using only code and a random number generator, with no artistic intervention on the part of a human being.  (It doubles as a learning exercise, of course.)

As of the time of writing it is in a _very raw state_, containing many bugs.  It may, for example, run out of space and crash if you approach a 3D object too closely.  I intend to keep on improving it.

I am making AFK available under the terms of the GNU General Public License (v3).  See LICENSE.txt.  I reserve the right to dual-license my original code, as permitted by the GPL; if you'd like to contribute, see CONTRIBUTIONS.txt.

Source code lives at:       https://github.com/KaiEkkrin/afk
Progress videos at:         https://www.youtube.com/user/KaiEkkrin

To build AFK you will currently need:
 - a C++11 compiler (G++ 4.8, MSVC++ 2013)
 - a modern OpenGL implementation (supporting OpenGL 4.0) and its development libraries
 - a modern OpenCL implementation (supporting OpenCL 1.1) and its development libraries
 - GLEW >=1.10.0 (get the latest from http://glew.sourceforge.net -- don't use the GLEW from the AMD APP SDK)
 - Boost >=1.5x (AFK currently links with boost_random, boost_regex,
   and maybe boost_system and boost_thread.  On Windows, see
   building-boost.txt
 - On Linux, SConstruct, the python build system, or Eclipse.
 - On Windows, Visual Studio 2013.

See COMPATIBILITY.txt.

Build and run example:

  scons -j 4
  ./build/release/afk

AFK is not currently self documenting.  Default key bindings are:
 - Middle mouse button to capture the mouse
 - Mouse axes for roll and pitch
 - Q, E to yaw
 - W, S to accelerate forwards and backwards
 - A, D to accelerate left and right
 - R, F to accelerate up and down
 - F11 toggles fullscreen.

It supports a collection of command line switches that can be identified by reading src/config.cpp.

 -- Alex Holloway, 2013.

