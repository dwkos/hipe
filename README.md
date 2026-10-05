Hipe: HTML Interface Pipe
=========================

A display server for native applications, from desktops to embedded devices.

Hipe is a display server whose native language is HTML. Applications don't draw pixels or
send pages: they connect over a local socket and build their interface as a live document of
HTML elements and CSS styles, which the display server lays out and renders. See `ABOUT.txt`
for the idea behind it.

Version 3.0 alpha.

What is here
------------

| Directory | Contents |
|---|---|
| `api/` | libhipe, the small client library that applications link (C, with a C++ header), and sample applications in `api/test/` |
| `server/` | hiped, the display server |
| `hipecore/` | The display engine that hiped is built on: a cut-down WebKit with no JavaScript and no network access |

Getting started
---------------

`INSTALL.txt` gives the build steps in order: hipecore, then the server, then the client
library and samples.

The manual, including the reference for every instruction an application can send, is at
http://hipe.generaldevelopment.net. `manual.html` in this directory is an archived copy from
version 2.12, for reading without a network connection.

Status of this version
----------------------

3.0 is the first version in which the client library, the server and the display engine are
one project. It starts from Hipe 2.12 and hipecore 0.6 beta, which were published separately
and remain available from the website's Download page as "classic" Hipe.

Classic Hipe could also be built against stock Qt5WebKit. Version 3.0 cannot: hipecore is
Hipe's display engine, and the server needs it.

3.0 uses version 3 of the protocol, and the server refuses clients built with the 2.x client
library. Rebuild applications against the 3.0 library.

Licensing
---------

Each part has its own licence: the client library is MIT, the samples 0BSD, the server GPL
version 3 or later, and hipecore keeps the licences inherited from WebKit. See `LICENSE.md`.
