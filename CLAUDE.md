# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

Hipe ("HTML Interface Pipe") is a stateful, local, HTML-powered **display server**, analogous to X11/Wayland
but rendering native app UIs as directly-manipulated DOM elements instead of pages. Client applications
don't send HTML or URLs: they connect over a local UNIX socket and issue a binary instruction protocol
(`HIPE_OP_*` opcodes) to build and mutate a live DOM tree that the server renders. See `ABOUT.txt` for the
rationale and `server/README.txt` for run modes.

This is one project with three parts, side by side:

- **`api/`** — `libhipe`, a small dependency-light C library (`hipe.c`/`hipe.h`, plus a thin C++ wrapper
  `hipe.hpp`) that client applications link to open a session and send/receive instructions. Sample
  applications are in `api/test/`.
- **`server/`** — `hiped`, the display server: a Qt5 application (qmake project) that embeds the display
  engine and renders frames on screen.
- **`hipecore/`** — the display engine: a cut-down fork of Qt5WebKit with no JavaScript, no networking and
  no browser features. It has its own `CLAUDE.md` with the engine's architecture and rules; read that
  before working in `hipecore/`.

The parts talk to each other only at two seams: clients and server over the socket protocol, and server
and engine through the `QWeb*` C++ API. There is no shared build system.

**Version 3.0 alpha.** The tree starts from Hipe 2.12 and hipecore 0.6 beta ("classic" Hipe), which were
separate projects. Classic Hipe could also be built against stock Qt5WebKit; 3.0 cannot. The server's
stock-Qt5WebKit code paths (the `HAVE_HIPECORE` guards and their `evaluateJavaScript` fallbacks) have been
removed, and the macro itself is gone: don't add new ones. hipecore is the only engine, and JavaScript is not available to the server.

## Licensing

Each part has its own licence; `LICENSE.md` explains them. The client library is MIT and the samples 0BSD
so that any application, open or closed, can link libhipe and copy from the samples. The server is GPLv3+.
hipecore keeps WebKit's licences. The four files shared between server and client library
(`common.{c,h}`, `hipe_instruction.{c,h}`) live in `server/src/` under MIT headers and are **symlinked**
into `api/src/`: the wire-protocol codec is shared source, not duplicated. Keep new shared code MIT, and
never move GPL server code into `api/`.

## Build

Build in this order: engine, server, client library.

**Engine** (`hipecore/`, CMake + Ninja, Qt 5.15; details and caveats in `hipecore/CLAUDE.md`):
```sh
cd hipecore && mkdir build && cd build
cmake -G Ninja -DPORT=Qt -DCMAKE_BUILD_TYPE=Release ..
ninja
sudo ninja install
```
It installs as `libHipeCore` and `libHipeCoreWidgets`, with headers in `/usr/include/HipeCore`. The server
links the *installed* copy, so a change in `hipecore/` only reaches `hiped` after `sudo ninja install`.

**Server** (`server/`, qmake + Qt 5.15; needs the Qt SVG module as well as the installed engine):
```sh
cd server
qmake -makefile ./src/hiped.pro   # or run ./configure, a 1-line wrapper for the same command
make
```
Produces `./hiped` in `server/`. There is no `make install` for the server; copy it where you like.

`hiped.pro` links the engine with `LIBS += -lHipeCore -lHipeCoreWidgets` and the source includes its headers
as `<HipeCore/QWebElement>`, `<HipeCore/QWebFrame>` and so on. If the engine was installed under a prefix
other than `/usr`, pass `HIPECORE_PREFIX=<prefix>` to qmake. The engine installs no qmake module files, so
there is no `QT += ...` for it.

**Client library** (`api/`, plain Makefile, `gcc`/`ar`):
```sh
cd api
make               # builds api/build/libhipe.a
sudo make install  # installs headers + libhipe.a to /usr/local/{include,lib}
make testing       # builds the api/test/hipe-* samples against the *installed* libhipe
sudo make install-tests  # optionally installs them to /usr/local/bin
```
The samples compile against the installed headers, so after changing `hipe.h` or
`hipe_instruction.h` (adding an opcode, for instance), reinstall the client library before `make testing`.

The C++ header `hipe.hpp` is intentionally installed as an extension-less file
(`/usr/local/include/hipe`) so C++ clients can `#include <hipe>` STL-style. This is deliberate, not a
packaging bug.

## Running and testing end to end

1. Start the server: `./hiped` (needs a running X11 or Wayland session; `--fill` for fullscreen,
   `--socket <path>` / `--keyfile <path>` to override the defaults, `--css a.css:b.css` for themes,
   `--help` for the rest).
2. Run any client, e.g. `api/test/hipe-clock` or `api/test/hipe-calc`.
3. `hiped` prints `Listening on <socket path>` on startup; clients that fail to reach it print
   `Hipe: Could not connect to socket: ...`.

To test without disturbing a running Hipe session, start a private `hiped` with its own `--socket` and
`--keyfile` (on a nested X server such as Xephyr if a display is needed) and point clients at it with the
`HIPE_SOCKET` and `HIPE_KEYFILE` environment variables.

**Important footgun**: the server hands out a single-use "top-level host key", written to a keyfile and
regenerated after every successful grant (see `requestContainerFromKey()` / `makeNewTopLevelKeyFile()` in
`server/src/main.cpp`). Several top-level clients launched back to back can read the same key before the
server rotates it; only one wins and the rest print `Hipe: Container request denied.` This is a security
property (keys can't be replayed), not a bug. Launch clients sequentially, waiting for the keyfile to be
rewritten, or route additional clients through a framing manager.

The engine has its own test suites (`ctest` in `hipecore/build`, see `hipecore/CLAUDE.md`). The server and
client library have no automated tests: verify changes with a small client against a private `hiped`.

## Architecture

### Wire protocol
Defined in `server/src/hipe_instruction.h` (shared with the API via symlink). A `hipe_instruction` is a
fixed-shape struct: an opcode byte (`HIPE_OP_*`), a `location` (an opaque handle to a DOM node), a
`requestor` id, and up to four variable-length arguments. Each opcode's arguments are documented next to
its `#define` in that header and, more fully, in the manual on the project website
(`manual.html` here is an archived copy from 2.12). The header and the manual should agree; when changing
an instruction, update both.

### Client side (`api/`)
`hipe_open_session()` connects the UNIX socket, sends `HIPE_OP_REQUEST_CONTAINER` with a host key (from
the keyfile or the environment) and blocks for `HIPE_OP_CONTAINER_GRANT`. `hipe_send` /
`hipe_send_instruction` write instructions; `hipe_next_instruction` / `hipe_await_instruction` read them.
`hipe_await_instruction` returns the next instruction with a given opcode, checking instructions already
queued first and queuing anything else that arrives meanwhile, so request/response patterns don't race
against unrelated events. The client allocates the location of each tag it creates
(`hipe_newest_location()`), so building a document needs no round trips.

### Server side (`server/`)
- One socket thread (`incomingSocketThread` in `main.cpp`) waits on every client socket, and reads and
  decodes instructions. Decoded instructions are queued and only ever *applied* (i.e. touch Qt/WebKit
  objects) on the main GUI thread, by `ConnectionManager` on a timer, since the engine is single-threaded.
  Don't add WebKit/Qt calls to the socket thread.
- `Container` (a `QObject` wrapping a `QWebFrame`) is "a place a client's DOM lives", in two forms:
  - `ContainerTopLevel` — a real top-level OS window, created when a client claims the top-level host key.
  - `ContainerFrame` — an `<iframe>` inside an existing container, created when a client presents a key
    issued by that parent. This is the "framing manager" pattern described in `ABOUT.txt`: one app acts
    as a window manager, handing out keys to host other apps inside its frames.
- `instructionhandler.cpp` has one handler per opcode, registered in `initInstructionMap()` with the
  number of arguments to pre-convert to strings.
- `KeyList` generates and consumes the single-use keys; `mKeyList` / `mActiveConnections` make it and the
  connection table thread-safe.
- `Sanitation` keeps client-supplied strings from breaking the markup hiped builds: the tag-name check,
  the stylesheet-text check (no `</`) and the text modes that escape markup. It no longer whitelists
  tags or attributes: the engine itself makes content inert (no scripts, no navigation, only `data:`
  loads), so new safety rules belong in hipecore, not in a hiped whitelist. Dedicated instructions
  (`TOGGLE_CLASS`, `SET_STYLE`, `SET_SRC`...) add to what plain attributes do; they don't gatekeep them.
  What hiped accepts is described for users on the manual's "Allowed tags, attributes and styles" page;
  keep the two in step.
- `MouseCursor` draws cursors (including Unicode-symbol cursors, `HIPE_OP_SET_CURSOR`) where a regular OS
  cursor isn't available.

### Engine (`hipecore/`)
See `hipecore/CLAUDE.md`. In short: most engine work is in `hipecore/Source/WebKit/qt/Api`
(`qwebelement.*` and the C++ event listener), exposing more of WebCore to the server through `QWebElement`
rather than through scripts. JavaScript, networking, cookies and browser history were removed on purpose
and must not come back.

## Documentation

The manual is written for people using the API: say what an instruction does, its arguments, the behaviour
they will see, limits, and where the engine matters. Leave out design rationale and implementation
detail. Prefer a short table of cases to paragraphs of explanation.
