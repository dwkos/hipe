# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

Hipe ("Hypertext Pipe") turns a WebKit engine into a stateful, local HTML-powered **display server**,
analogous to X11/Wayland but rendering native app UIs as directly-manipulated DOM elements instead of
pages. Client applications don't send HTML/URLs — they connect over a local UNIX socket and issue a
binary instruction protocol (`HIPE_OP_*` opcodes) to build and mutate a live DOM tree that the server
renders. See `ABOUT.txt` for the full rationale and `server/README.txt` for run modes.

The repo has two independently-built components that talk to each other only via that socket protocol —
there is no shared build system, header, or library between them:

- **`server/`** — `hiped`, a Qt5 Widgets/GUI application (qmake project) that embeds the WebKit engine
  (either stock Qt5WebKit or, more commonly for this project, **hipecore** — a stripped-down WebKit fork
  built specifically for Hipe) and renders frames on screen.
- **`api/`** — `libhipe`, a small dependency-light C library (`hipe.c`/`hipe.h`, plus a thin C++ wrapper
  `hipe.hpp`) that client applications link against to open a session and send/receive instructions. MIT
  licensed — deliberately separate from the GPL server so that closed-source apps can statically link it
  without coming under the GPL. (The server links hipecore/WebKit and may eventually be bundled with it,
  hence its GPL-compatible licence.) The sample programs in `api/test/` are 0BSD (`api/test/LICENSE.txt`) so they
  can be copied into any app freely; each file's header still records the authorship.

A third-party build should treat these as two separate build steps; nothing here assumes a shared parent
build directory.

## Build

**Server** (`server/`, qmake + Qt5, requires Qt5 dev headers plus either hipecore or Qt5WebKit dev
headers/libs installed on the system):
```sh
cd server
qmake -makefile ./src/hiped.pro   # or run ./configure, a 1-line wrapper for the same command
make
```
Produces `./hiped` in `server/`. Install by copying it wherever you like (e.g. `/usr/local/bin/`) — there's
no `make install` target for the server.

Note: `hiped.pro` deliberately does **not** use `QT += webkit webkitwidgets`; it links manually via
`LIBS += -lQt5WebKit -lQt5WebKitWidgets`. This sidesteps depending on the qmake `.pri` module files for
the WebKit modules (which may not be present, e.g. on a fresh hipecore install that hasn't shipped them),
at the cost of qmake not auto-adding WebKit's include paths — the source works around this by using fully
qualified includes (`<QtWebKit/QWebElement>`, `<QtWebKitWidgets/QWebFrame>`) rather than bare
(`<QWebElement>`) includes, relying on the base Qt5 include dir already being on the path via `QT += core
gui ...`.

**API** (`api/`, plain Makefile, `gcc`/`ar`, only needs `hipe.h`'s C `sys/types.h`-level dependencies):
```sh
cd api
make               # builds api/build/libhipe.a
sudo make install  # installs headers + libhipe.a to /usr/local/{include,lib} (edit the Makefile's
                    # cp targets if your distro doesn't use /usr/local)
make testing        # builds api/test/hipe-* demo/test binaries against the installed libhipe
sudo make install-tests  # optionally installs them to /usr/local/bin as hipe-clock, hipe-calc, etc.
```
The C++ header `hipe.hpp` is intentionally installed as an extension-less file
(`/usr/local/include/hipe`, not `hipe.hpp`) so C++ clients can `#include <hipe>` STL-style — see
"C++ extensions" in `manual.html`. This is deliberate, not a packaging bug.

`api/src/common.{c,h}` and `hipe_instruction.{c,h}` are **symlinks** into `server/src/`: the wire-protocol
codec is shared source between the client library and the server, not duplicated.

## Running / testing end-to-end

1. Start the server: `./hiped` (needs a running X11 or Wayland session; add `--fill` for fullscreen,
   `--socket <path>` / `--keyfile <path>` to override the defaults, `--help` for the rest).
2. Run any client, e.g. `./hipe-clock`, `./hipe-calc` (built via `make testing` above).
3. `hiped` prints `Listening on <socket path>` on startup; clients that fail to reach it print
   `Hipe: Could not connect to socket: ...`.

**Important footgun**: the server hands out a single-use, non-repeating "top-level host key" (written to
a keyfile on disk, regenerated immediately after every successful grant — see `requestContainerFromKey()`
/ `makeNewTopLevelKeyFile()` in `server/src/main.cpp`). If you launch multiple standalone top-level client
apps back-to-back with no delay, several of them can read the *same* not-yet-rotated key from the keyfile
before the server processes the first request — only one wins, and the rest print
`Hipe: Container request denied.` This is a real security property (keys can't be replayed), not a bug.
When scripting multi-client tests, launch clients sequentially with a short delay between each, or route
additional clients through a framing manager (see Architecture below) instead of requesting the top level
directly.

**Known limitation**: per `README.md`'s version history, canvas support is not yet implemented when Hipe
is linked against hipecore (only against stock Qt5WebKit). This isn't a crash — `hipe-canvas` connects
fine — the server's `handle_USE_CANVAS` / `handle_CANVAS_ACTION` / `handle_CANVAS_SET_PROPERTY` handlers
in `instructionhandler.cpp` are currently empty stubs in that configuration, so canvas instructions are
silently no-ops.

## Architecture

### Wire protocol
Defined in `server/src/hipe_instruction.h` (shared with the API via symlink). A `hipe_instruction` is a
fixed-shape struct: an opcode byte (`HIPE_OP_*`, e.g. `APPEND_TAG`, `SET_TEXT`, `EVENT_REQUEST`), a
`location` (an opaque handle to a DOM node), a `requestor` id, and up to a few variable-length string
args. Every opcode's argument meaning is documented inline next to its `#define` in that header — treat
it as the protocol spec. `manual.html` (a copy of the website's documentation) documents the
client-facing C/C++ call surface and every instruction.

### Client side (`api/`)
`hipe_open_session()` connects the UNIX socket, sends `HIPE_OP_REQUEST_CONTAINER` with a host key (read
from the keyfile) and blocks for `HIPE_OP_CONTAINER_GRANT`. Once granted, `hipe_send`/`hipe_send_instruction`
write instructions out, and `hipe_next_instruction`/`hipe_await_instruction` read them back — the latter
pulls a specific opcode out-of-order while queuing anything else that arrives in the meantime, so
request/response call patterns (e.g. "append a tag, then await its ATTRIBUTE_RETURN") don't race against
unrelated incoming events.

### Server side (`server/`)
- `ConnectionManager` owns one `Connection` per connected client socket. Per `connectionmanager.cpp`,
  each `Connection` has its own thread that reads and decodes raw instructions off the socket; decoded
  instructions are queued and only ever *applied* (i.e. touch Qt/WebKit objects) on the main/GUI thread,
  since Qt widgets aren't thread-safe. Don't add WebKit/Qt calls to the socket-reading thread.
- `Container` (a `QObject` wrapping a `QWebFrame`) is the base abstraction for "a place a client's DOM
  lives." It has two concrete forms:
  - `ContainerTopLevel` — a real top-level OS window (`QMainWindow`/`QGraphicsView`/`QGraphicsWebView`),
    created when a client successfully claims the single-use top-level host key.
  - `ContainerFrame` — an `<iframe>` inside an *existing* container, created when a client presents a
    valid key issued by that parent container rather than the top-level key. This is the "framing
    manager" pattern described in `ABOUT.txt`: one app can act as a window manager, handing out its own
    keys to host other apps inside its frames, without needing top-level server involvement.
- `KeyList` (`keylist.cpp`/`.h`) generates and claims (consumes) the single-use security keys described
  above; `mKeyList`/`mActiveConnections` mutexes make it and the connection table thread-safe.
- `Sanitation` (`sanitation.cpp`/`.h`) statically sanitizes user-supplied content before it reaches
  WebKit/the DOM — notably canvas-related instruction arguments, called out in the version history as a
  deliberate defense against arbitrary code execution via crafted instructions. Treat this as a security
  boundary: new instruction handlers that accept client-controlled strings should go through it, not
  around it.
- `MouseCursor` implements custom (including Unicode-symbol, via `HIPE_OP_SET_CURSOR`) cursor rendering
  for platforms where a regular OS cursor isn't available (e.g. bare EGLFS/DRM setups).

### Standards mismatch to be aware of
`hiped.pro` builds the server with `-std=c++11`. If it's linked against a hipecore build compiled at a
newer C++ standard (hipecore itself now requires C++17 on modern toolchains — see hipecore's own
CLAUDE.md), that's fine across the shared-library boundary in practice, but don't assume the two projects
build under the same language standard.
