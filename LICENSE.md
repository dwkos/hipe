Licensing
=========

Hipe is not under a single licence. Each part of the project has its own, chosen for what
that part is, and the licence that applies to a file is the one in its own directory and
its own header.

| Part | Licence | Where it is stated |
|---|---|---|
| `api/src/` — libhipe, the client library | MIT | `api/LICENSE.txt` |
| `api/test/` — the sample applications | 0BSD | `api/test/LICENSE.txt` |
| `server/` — hiped, the display server | GNU GPL version 3 or later, with an attribution requirement | `server/LICENSE.txt` |
| `hipecore/` — the display engine | Inherited from WebKit and QtWebKit: mostly the GNU LGPL 2.1 and Apple's BSD-style licence | `hipecore/LICENSE.LGPLv21`, `hipecore/Source/WebCore/LICENSE-APPLE`, and each file's header |

Why they differ
---------------

- **The client library is MIT, and the samples 0BSD, so that any application can use Hipe.**
  An application talks to the display server only through libhipe, over a socket. It may link
  libhipe statically and copy from the samples whatever its own licence is, open or closed.
- **The server is GPL because of what it is built on.** hiped links the display engine, which
  is derived from WebKit and carries WebKit's copyleft licences; the GPL is compatible with
  them and keeps the display server as a whole free software.
- **hipecore keeps the licences of the code it came from.** Bundled third-party code under
  `hipecore/Source/ThirdParty/` has its own licence in its own directory.

A few files are shared between the server and the client library: `common.c`, `common.h`,
`hipe_instruction.c` and `hipe_instruction.h` in `server/src/`, which appear in `api/src/` as
links. They are under the MIT licence, as their headers say, so that the client library stays
free of the server's licence.

The manual (`manual.html`) and the other documentation in this directory are
Copyright (c) General Development Systems and Daniel Kos.

"Hipe" is a trademark of General Development Systems. See `server/LICENSE.txt` for the
attribution requirement that applies to products containing the display server.
