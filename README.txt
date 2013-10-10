AFK
===

("Automated Flight Kit" or
 "Away From Keyboard", or whatever else you prefer.)

AFK is an art project.  It's an attempt to generate pretty, diverse 3D using only code and a random number generator, with no artistic intervention on the part of a human being.

As of the time of writing it is in a _very raw state_, containing many bugs.  It will, for example, run out of space and crash if you approach a 3D object too closely.  I intend to keep developing it.  Some day it may acquire functionality like that of a game.

To build AFK you will currently need:
 - a GNU/Linux distribution
 - GNU G++ (recent enough to support C++11)
 - a modern OpenGL implementation (supporting OpenGL 4.0) and its development libraries
 - a modern OpenCL implementation (supporting OpenCL 1.1) and its development libraries
 - GLX
 - GLEW
 - Boost (AFK currently links with boost_chrono, boost_random, boost_system and boost_thread.  I used version 1.53)
 - SConstruct, the python build system.

AFK has been tested and is known to work on Nvidia Fermi based graphics cards using the binary "nvidia" driver, and AMD VLIW5 graphics cards using the binary "fglrx" driver.  It is known not to work on Intel integrated graphics at the current time.  Some day I hope to make it run on a broader range of hardware and operating systems.

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


