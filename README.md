HIPE: Hypertext Pipe
====================

* For full documentation, open manual.html in this directory, or visit the project homepage at
  http://hipe.generaldevelopment.net
* See INSTALL.txt for a summary of the installation steps.

VERSION INFORMATION
-------------------

2.12 -- Oct 2026

This is the planned final version of Hipe that supports stock Qt5WebKit as its display engine.
Later versions will require hipecore.

Requires hipecore v0.6 beta or later, or Qt5WebKit. Some instructions need hipecore; the manual
says which.

2.11 beta -- Sep 2026

Requires hipecore v0.5 alpha or later.

Canvas drawing works with hipecore (HIPE_OP_USE_CANVAS, HIPE_OP_CANVAS_ACTION,
HIPE_OP_CANVAS_SET_PROPERTY, HIPE_OP_CANVAS_QUERY), and image or canvas data can be read back
with HIPE_OP_GET_SRC. HIPE_OP_SET_SRC can send audio and video in chunks, as well as images.
Events that arrive in bursts (mousemove, resize, scroll) are combined before they are sent.
New samples: hipe-canvas and hipe-video.

2.10 beta -- Sep 2026

hiped no longer loads the host desktop's Qt platform theme, saving about 11 MB of memory.
Native dialogs are drawn in Qt's built-in Fusion style.

2.09 beta -- Sep 2026

HIPE_OP_TAKE_SNAPSHOT gains an "svg" format. Building hiped now needs the Qt SVG module
(libqt5svg5-dev on Debian and Ubuntu). With hipecore, SVG drawing tags and their attributes
can be created inside an <svg> tag.

2.08 beta -- Sep 2026

Requires hipecore v0.4 alpha or later.

HIPE_OP_TAKE_SNAPSHOT captures the frame exactly as it appears on screen, and can produce a
PNG as well as a PDF.

2.07 beta -- Aug 2026

Updated to work with hipecore v0.2.

2.06 beta -- Aug 2026

Builds against hipecore now that it has no network backend.

2.05 beta -- Aug 2026

New HIPE_OP_GET_X11_XID and HIPE_OP_X11_XID_RETURN instructions for embedding an X11 window
in an <object> tag (hipecore only). hipe_open_session() retries for a short time if its key
has just been claimed by another client starting at the same moment.

2.04 beta -- Aug 2026

The server is now compiled as C++17. The C API (libhipe) remains plain C.

2.03 beta -- Jul 2025

Addition of HIPE_OP_SET_CURSOR instruction which allows unicode symbols to be used as mouse
cursors.

2.02 beta -- Feb 2025

Various bug fixes and optimisations. Addition of hipecore-specific optimisations.
Hipe can now be linked against hipecore instead of qtwebkit however the canvas element is
not yet supported.

2.01 beta -- 8 Feb 2023

NEW VERSION. 
NOTE!!! Existing hipe client applications need to be recompiled to work with this version due
to a change to the protocol. !!!!
This version features vastly improved perfmance in constructing and rendering user interfaces
due to a change in how resources are allocated.


1.10 beta -- 28 Feb 2020

Attempted to commit some fixes that were not in the tree.
Replaced the old HIPE_OP_SET_BACKGROUND_SRC operation with a more general HIPE_OP_SET_STYLE_SRC
operation (can be used with the "background-image" style attribute for the same functionality.
Since the old instruction has been removed, existing code that used this operation should be modified.

1.09 beta -- 9 Jul 2019

Numerous improvements. An instruction framework for modal dialog box implementation is now provided.

1.08 beta -- 14 Apr 2019

Numerous bug fixes. Added carat manipulation instructions for accessing and manipulating selections in text input elements.

1.07  beta -- 24 Jan 2019

Multiple new instructions, sanitisation of canvas instructions to prevent arbitrary
code execution.

v1.06 beta -- 21 July 2018

Important bug fix release.
Test programs now conveniently installable to help verify a working Hipe environment.

v1.05 beta -- 4 July 2018

New instructions and instruction arguments added.

v1.04 beta -- 15 May 2018

Documentation cleanup and various bug-fixes.

v1.03 beta -- 24 Apr 2018

Sending instructions to the server is now an atomic operation.

v1.02 beta -- 19 Apr 2018

Fixed a longstanding bug in await_instruction that caused the calling client
to hang at random.

v1.01 beta -- 7 Jan 2018

- HIPE_OP_GET_SCROLL_GEOMETRY instruction has been added.
- HIPE_OP_GET_GEOMETRY instruction has changed. Server now replies with a HIPE_OP_GEOMETRY_RETURN
  instruction, and the old two-step reply instructions have been removed.
- Socket handling in the hiped server process is now handled with multithreading. Instructions are
  collected and decoded in a separate thread.

From 1.0:
- The hipe_send() function now supports a variadic syntax. Existing programs will require modification.
- Hipe now uses a slightly different message-passing format between client and server, enabling between
  zero and four arguments per instruction. (Existing programs require recompilation.)
- From 0.28: The argument ordering for the following  instructions has been altered: HIPE_OP_SET_ICON,
  HIPE_OP_SET_BACKGROUND_SRC, HIPE_OP_SET_SRC. Applications that make use of these will need minor modification to
  work with the new version. This change will ease the transition to a variadic instruction set.
- C++ <hipe> library extension now has a new argument format for the hipe::send() function. Arguments should now
  be enclosed in initialiser list braces { }, and a variable number of arguments may be provided.

- From 0.27: A workaround has been added to allow HIPE_OP_ADD_STYLE_RULE instructions to be respected after initial
  body content has been added to the screen. Not all style rules added in this scenario appear to be respected
  by the webkit backend, however.


TODO:

- More robust memory management
- A transparent PING instruction to allow the hipe display server to identify nonresponsive applications based on when the application last checked for new instructions. Such information could then be passed to (or managed entirely by) the framing manager.
- More flexible DOM manipulation operations, e.g. compliment append with prepend operations.


GETTING THE LATEST VERSION
--------------------------

Releases of Hipe and of hipecore are published as separate source archives on the Download page of
the project homepage, http://hipe.generaldevelopment.net
