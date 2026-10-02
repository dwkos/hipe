# CLAUDE.md (hipecore)

Guidance for working in `hipecore/`, Hipe's display engine. The project as a whole (client library,
server, how the parts fit and are built together) is described in `../CLAUDE.md`; read that first.

## What this is

hipecore is a stripped-down fork of Qt5WebKit (based on qtwebkit-1.212.0-alpha4, itself derived from
upstream WebKit/WebCore/JavaScriptCore/WTF). It is **not** a general-purpose browser engine: it is the
rendering back end for `hiped`, a stateful, synchronous, local display server. The server manipulates the
DOM directly through the C++ `QWebElement` API rather than through JavaScript, so most work here is:

1. Adding or fixing `QWebElement` (and occasionally `QWebPage` / `QWebFrame`) methods for direct DOM
   manipulation, selection and caret handling, text measurement and editing
   (`Source/WebKit/qt/Api/qwebelement.cpp` / `.h`, `Source/WebKit/qt/WidgetApi/`).
2. Removing web/browser features that make no sense for a local, single-document, non-networked display
   server. The big ones are already gone: the whole JavaScript engine (`Source/JavaScriptCore` deleted, no
   JS bindings in WebCore), all networking and URL navigation, cookies, history, the Web Inspector,
   storage, device APIs. `README.md` lists them.
3. Maintaining a C++ callback-based event system (`hipecoreeventlistener.h/.cpp` in the same `Api`
   directory) as the non-JS replacement for inline event handlers.

The standing policy is to delete residual dead Inspector / networking / history code when it is
encountered rather than preserve it "just in case". Don't reintroduce JavaScript execution, cookies,
browser history, or network/URL-navigation logic: these were eliminated deliberately, for security and
simplicity, not by oversight.

hipecore compiles and installs under the same names as Qt5WebKit, so it conflicts with an existing
QtWebKit installation. This is expected (see `README.md`, "Notes").

## Build

CMake + Ninja, from this directory:

```sh
mkdir build && cd build
cmake -G Ninja -DPORT=Qt -DCMAKE_BUILD_TYPE=Release ..
ninja
sudo ninja install
```

- `PORT=Qt` is required: `CMakeLists.txt` fails fast if no valid port is given, and only the Qt port is
  supported in practice.
- `hiped` links the *installed* libraries, so `sudo ninja install` is part of testing any change against
  the server. Run `ninja` as yourself first; `sudo ninja install` builds whatever is out of date as root.
- Every `ninja` run relinks `libQt5WebKit` and the test programs even when nothing changed: the link step
  regenerates `QtWebKit.version`, which ninja then sees as a newer input. It is harmless.
- On a small machine, the large "AllInOne" translation units can exhaust memory at high parallelism:
  build with `-j2` until they are through.
- A clean build (delete `build/` and reconfigure) is the only reliable check after changing generated
  code or removing files: an incremental build can pass while a clean one fails.
- On Ubuntu/Debian, `sudo apt-get build-dep libqt5webkit5` pulls most dependencies (do **not** install
  `libqt5webkit5` itself: it conflicts). `README.md` has the package list and the dependencies that have
  been dropped (ruby-dev, SQLite, Qt5Sensors, Qt5Network, libhyphen, qtpositioning, qtwebchannel).
- The Perl build wrapper at `Tools/Scripts/build-webkit`, inherited from upstream, is **non-functional**:
  `Tools/Scripts/webkitdirs.pm` has a syntax error (dangling `elsif` blocks left from a removed `isEfl()`
  branch). Use the CMake invocation above.
- `BuildrootIntegration/` contains experimental Buildroot packaging (`hipecore.mk`, `Config.in`) for
  embedded targets. It is marked EXPERIMENTAL in `BuildrootIntegration/README.txt`.

## Tests

The suites began as stock QtWebKit suites and are lightly maintained. They are not a gate, but everything
that tested removed features has been pruned, so a *new* failure is worth a look.

- Enable with `-DENABLE_API_TESTS=ON` at configure time (only for `PORT=Qt`), then run `ctest` from the
  build directory. Per-test `TIMEOUT` is 240s.
- Qt API tests are under `tests/webkitwidgets/` (one subdirectory per class): `qwebelement`, `qwebframe`,
  `qwebpage`, `qwebview`, `qgraphicswebview`. `Tools/TestWebKitAPI/` adds `TestWTF` and `TestWebCore`.
- All seven pass. `qwebview` needs a **window manager** on the display: its palette tests wait for a
  window to become active, and fail on a bare X server.
- Run one Qt Test binary directly with e.g. `./tests/tst_qwebelement someTestFunction`.
- Selection, caret and input-method tests are driven through the C++ API (`QWebPage` actions,
  `inputMethodQuery(Qt::Im*)`, `QWebElement::select()` / `setSelectionRange()`), not JS.
- `QWebFrame::setHtml()` completes synchronously, so `waitForSignal(view, SIGNAL(loadFinished(bool)))`
  after it hangs for its full timeout: create a `QSignalSpy` before the call, or don't wait.
- `ENABLE_TEST_SUPPORT` (the old `DumpRenderTreeSupportQt` path) is dead; don't turn it on.

## Architecture

Standard WebKit layering, most of which changes should not need to touch:

- `Source/WTF` — low-level utility library (threading, containers, text). Also holds the pure-C++
  typed-array headers WebCore still needs (`wtf/typedarrays/`), moved here when JavaScriptCore was deleted.
- `Source/WebCore` — the DOM/CSS/rendering engine (page model, DOM tree, layout, painting, editing). This
  is what `QWebElement` wraps. It has no JS bindings.
- `Source/WebKit/qt/` — the embedding API, where almost all Hipe-specific work happens:
  - `Api/` — `qwebelement.*`, `qwebsettings.*`, `qwebsecurityorigin.*`, `qwebpluginfactory.*`, and
    `hipecoreeventlistener.*` (a `WebCore::EventListener` subclass that dispatches DOM events to a
    `std::function<void(Event*)>`).
  - `WidgetApi/` and `WidgetSupport/` — `QWebPage`, `QWebFrame`, `QWebView`, `QGraphicsWebView`.
  - `WebCoreSupport/` — Qt implementations of WebCore's client interfaces (chrome client, editor client,
    `QWebPageAdapter`): the seam where WebCore calls out into Qt and the server.
  - `declarative/`, `Plugins/`, `examples/`, `docs/` — inherited from upstream; low priority.
- `Source/bmalloc` — allocator, built conditionally.
- `Source/cmake/` — `Find*.cmake` modules and shared CMake logic (`OptionsQt.cmake` has the `ENABLE_*`
  flags and their Qt-port defaults).
- `Tools/` — leftover upstream tooling: `TestWebKitAPI`, `Scripts/`, `qt/`.

### Key mental model

WebCore is the engine and is largely upstream code, left alone except where features are being stripped
or a small, well-contained change is needed. `Source/WebKit/qt/Api` is the thin surface the server calls.
When a `QWebElement` method needs new behaviour from the engine, first check whether WebCore already
exposes it through `Node` / `Element` / `Frame` / `Editor`, and prefer exposing existing WebCore
behaviour over adding new WebCore code. Never solve a problem by reintroducing script-based bindings.

Files with non-trivial Hipe changes carry a `Copyright (C) 2025-2026 General Development Systems` line
below the existing upstream lines; add one when making such a change to a file that lacks it.
