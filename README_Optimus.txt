AFK and Nvidia Optimus systems
==============================

This *ought* to "just work" ("optirun afk" or "primusrun afk"), but frequently doesn't.

On Ubuntu 13.10, here's what I did:
 - Installed primusrun from ppa:bumblebee/stable .  The version in Ubuntu 13.10 main is outdated and doesn't support glXCreateContextAttribsARB (!!)
 - Installed nvidia-319 nvidia-settings-319 nvidia-319-dev.
 - Updated /etc/bumblebee/bumblebee.conf with Driver=nvidia, and swap all references to `nvidia-current' with `nvidia-319'
 - Updated /etc/OpenCL/vendors/nvidia.icd to contain the string "/usr/lib/nvidia-319/libnvidia-opencl.so.319.32".

Right now the render is showing significant flicker that doesn't appear on non-Optimus systems, I'm not sure what the cause is.

AFK *ought* to also work on Optimus systems on the integrated GPU, so long as it has the right OpenGL and OpenCL version support (i.e. Ivy Bridge or later, not sure about the AMD requirements).  But I don't have such a system, so I can't test it.

 -- Alex Holloway, 2013.

