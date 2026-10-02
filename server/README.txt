Source code for Hipe server process.

DEPENDENCIES:

You must have the development files for Qt 5.15 installed on the system
(earlier Qt 5 releases are not supported, and Qt 6 is not supported yet).
This includes the QtSvg module (Debian/Ubuntu: libqt5svg5-dev; Fedora:
qt5-qtsvg-devel; Arch: qt5-svg) - hiped uses QSvgGenerator for the "svg"
HIPE_OP_TAKE_SNAPSHOT format. Missing it shows up as
"Project ERROR: Unknown module(s) in QT: svg" at qmake time.
You must also have hipecore, Hipe's display engine, installed on the system:
build and install it from ../hipecore first.
Qt must be built with STL support included.

BUILD INSTRUCTIONS:

****
****PLEASE REFER TO THE INSTALLATION SECTION OF manual.html IN THE TOP DIRECTORY
****(ALSO ONLINE AT http://hipe.generaldevelopment.net) FOR FULLER INSTRUCTIONS.
****(It also explains how to set up environment variables for non-default settings.)


You can either build the Qt project using Qt Creator, or alternatively from the terminal as follows:

Run the following commands from this directory.

$ qmake -makefile ./src/hiped.pro
$ make

Then clean up the object files with:

$ make clean

Now the hiped executable is ready to run.

INSTALLING

Once the build is complete, simply copy the hiped executable file into /usr/local/bin/, or an alternative directory 
of your choice.


RUNNING THE SERVER:

The default build of hipecore requires that hiped be run within an X or Wayland environment.
hipecore can also be built and configured for EGLFS, which uses Linux's direct rendering
infrastructure without the need for another display server.

You can either run ./hiped directly from within your favourite desktop environment 
(rootless mode), or you can deploy the Hipe display server on its own (e.g. EGLFS through
Linux's Direct Rendering Infrastructure), or within a bare-bones X session.

If running on top of an existing desktop environment (e.g. Ubuntu) it is recommended to
set up hiped to run in the background when the user logs into a graphical desktop session,
by adding the following command to your configuration:

hiped &

The command should be executed within the environment of the user's desktop session, so
that hiped can connect to the display server.



For a dedicated Hipe interface (e.g. embedded devices)...

One example for a dedicated Hipe desktop on top of X is given in a rudimentary script 
provided: run ./start_hipe_standalone_x11
*from the current directory* while in a text-only console virtual terminal (this script 
assumes that the second virtual graphics terminal (often the one accessible with Ctrl+Alt+F8) 
is free for use).


When the hiped server process starts, there is no graphical interface shown on the screen,
since no Hipe applications are running within the display server yet.

The user can use a separate terminal instance to start a hipe application, which will then
connect to the server process and be displayed.

Run `hiped --help` for more command-line options.
