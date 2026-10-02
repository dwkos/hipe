This directory contains files needed to build hipecore as a package in an embedded Buildroot distribution.

EGLFS and X11 are recommended backend options for embedded systems.
On Raspberry Pi, X11 still delivers superior performance to EGLFS.

EXPERIMENTAL; not ready or tested yet.

Because the dependencies are imperfectly specified and dependent on multiple possible driver and backend
configurations, it can help to set up a Buildroot distro without Hipecore first and then add Hipecore
back in when all dependencies are satisfied.


Instructions [some experimentation needed]
------------

1. Place this hipecore distribution in its own directory outside your buildroot directory.
2. Copy the hipecore subdirectory into your buildroot/package/ directory.
3. In the resulting buildroot/package/hipecore/ directory, modify the hipecore.mk file 
so the path in the "HIPECORE_SITE" line points to your hipecore directory.
(There are multiple hipecore.mk-.... versions, rename preferred one to hipecore.mk so
that file will be used.)
4. Modify the hipecore.mk file as needed so it builds for the desired backend configuration. 
5. To add hipecore to the buildroot config menu, edit buildroot/package/Config.in file
as follows:

comment "Other GUIs"
    source "package/qt5/Config.in"
    source "package/hipecore/Config.in"  #<------ Add this line


