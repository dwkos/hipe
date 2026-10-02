# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

hipecore is a stripped-down fork of Qt5WebKit (based on qtwebkit-1.212.0-alpha4, itself derived from
upstream WebKit/WebCore/JavaScriptCore/WTF). It is **not** a general-purpose browser engine — it's the
rendering backend for "Hipe", a stateful, synchronous, local display server. Hipe manipulates the DOM
directly through the C++ `QWebElement` API rather than through JavaScript, so most of the ongoing work in
this repo is:

1. Adding/fixing `QWebElement` methods for direct DOM manipulation, selection, and caret handling
   (`Source/WebKit/qt/Api/qwebelement.cpp` / `.h`).
2. Removing web/browser features that make no sense for a local, single-document, non-networked display
   server. The big ones are already gone — the whole JavaScript engine (`Source/JavaScriptCore` deleted,
   no JS bindings in WebCore), all networking/URL navigation, cookies, history, the Web Inspector,
   storage, device APIs — see README.md for the full list.
3. Maintaining a custom C++ callback-based event system (`hipecoreeventlistener.h/.cpp` in the same `Api`
   directory) as a faster, non-JS alternative to inline JS event bindings.

When touching code in `Source/WebKit/qt/Api`, check README.md's "Aims" / "Todo" sections first — the
standing policy is to delete residual dead Inspector / networking / history code whenever it's
encountered rather than preserve it "just in case". Don't reintroduce JavaScript execution, cookies,
browser history, or network/URL-navigation logic — these are deliberately eliminated for security and
simplicity, not by oversight.

Because hipecore compiles and installs under the same names as Qt5WebKit, it will conflict with an
existing QtWebKit installation — this is expected/by design (see README.md "Notes").

## Build

CMake + Ninja, same as upstream QtWebKit:

```sh
mkdir build && cd build
cmake -G Ninja -DPORT=Qt -DCMAKE_BUILD_TYPE=Release ..
ninja
sudo ninja install
```

- `PORT=Qt` is required — `CMakeLists.txt` fails fast if no valid port (`AppleWin`, `GTK`, `Mac`, `WinCairo`,
  `Qt`) is given. hipecore only supports the Qt port in practice.
- On Ubuntu/Debian, `sudo apt-get build-dep libqt5webkit5` pulls most dependencies (do **not** actually
  install `libqt5webkit5` itself — it conflicts). See README.md's "Build" section for the explicit package
  list and for dependencies that have been intentionally dropped (ruby-dev, SQLite, Qt5Sensors, Qt5Network,
  libhyphen, qtpositioning, qtwebchannel).
- There is also a Perl-based build wrapper at `Tools/Scripts/build-webkit` (`--qt`, `--debug`/`--release`,
  `--clean`, etc.) inherited from upstream WebKit; the CMake invocation above is the primary path used for
  this fork. **This wrapper is currently non-functional**: `Tools/Scripts/webkitdirs.pm` has a syntax
  error (`getJhbuildPath()` / `wrapperPrefixIfNeeded()` have dangling `elsif` blocks left over from an
  `isEfl()` branch removed in a pre-hipecore commit — `perl -c` fails at the first `} elsif`). This
  predates hipecore's own history; don't assume `build-webkit` works without fixing this first.
- `BuildrootIntegration/` contains experimental Buildroot packaging (`hipecore.mk`, `Config.in`) for
  embedded targets (Raspberry Pi, X11/EGLFS). It's explicitly marked EXPERIMENTAL in
  `BuildrootIntegration/README.txt`.

## Tests

**These started as stock QtWebKit suites and are only lightly maintained** — they are not a gate for
hipecore work, and `ctest` results are a weak signal, not a trusted one. Don't treat extending or
perfecting these suites as expected work unless explicitly asked. That said, the API suites have been
pruned of everything that tested removed features (JavaScript, URL navigation, resource loading, plugins,
WebGL, page cache), so a *new* failure in them is now worth a look.

API tests live under `tests/webkitwidgets/` (Qt Test framework, one subdir per `QWeb*` class): the wired
suites are `qwebelement`, `qwebframe`, `qwebpage`, `qwebview`, `qgraphicswebview`, built via
`tests/CMakeLists.txt` -> `tests/webkitwidgets/CMakeLists.txt`. `Tools/TestWebKitAPI/` adds `TestWTF`
(WTF containers/strings/threading) and `TestWebCore` (a handful of `WebCore/` unit tests). Everything
else that used to be here — `qwebhistory`, `qwebsecurityorigin`, `hybridPixmap`, the `benchmarks/`,
`MIMESniffing`, `keyed{en,de}coderqt`, `DumpRenderTree`, `ImageDiff` — has been deleted.

- Enable with `-DENABLE_API_TESTS=ON` at CMake configure time; this calls `enable_testing()` and adds the
  `tests/` subdirectory (only for `PORT=Qt`). `ENABLE_TEST_SUPPORT` (the old `DumpRenderTreeSupportQt` /
  `WebCoreTestSupport` `window.internals` path) is effectively dead — don't turn it on.
- Run the full suite with `ctest` from the build directory after building. Per-test `TIMEOUT` is 240s
  because the Raspberry Pi is slow.
- Run a single Qt Test binary directly, e.g. `./tst_qwebelement someTestFunction`.
- All wired suites pass (`tests/webkitwidgets/qwebpage/BLACKLIST` still skips `cursorMovements` on
  Windows only). Selection/caret/IM tests are driven through the C++ API — `QWebPage` actions,
  `inputMethodQuery(Qt::Im*)`, `QWebElement::select()` / `setSelectionRange()` — not JS.
- Watch out: `QWebFrame::setHtml()` now completes synchronously, so
  `waitForSignal(view, SIGNAL(loadFinished(bool)))` after it will hang for its full timeout — use a
  `QSignalSpy` created before the call, or just don't wait.

## Architecture

Standard WebKit layering, most of which future changes should NOT need to touch directly:

- `Source/WTF` — low-level platform/utility library (threading, containers, text). Also now holds the
  ~20 pure-C++ typed-array headers WebCore still needs (`wtf/typedarrays/`), relocated when
  `Source/JavaScriptCore` was deleted.
- `Source/WebCore` — the actual DOM/CSS/rendering engine (page model, DOM tree, layout, painting). This is
  what QWebElement wraps. No JS bindings anywhere in it any more.
- `Source/WebKit` — the public embedding API. **`Source/WebKit/qt/` is where almost all hipecore-specific
  work happens.**
  - `Source/WebKit/qt/Api/` — the public `QWeb*` C++ API surface (`qwebelement.*`, `qwebsettings.*`,
    `qwebsecurityorigin.*`, `qwebpluginfactory.*`, etc.) plus hipecore's own additions:
    `hipecoreeventlistener.h/.cpp` (a `WebCore::EventListener` subclass that dispatches DOM events to a
    `std::function<void(Event*)>` callback instead of requiring JS bindings).
  - `Source/WebKit/qt/WidgetApi/` and `WidgetSupport/` — `QWebView`/`QGraphicsWebView` widget integration.
  - `Source/WebKit/qt/WebCoreSupport/` — Qt-specific implementations of WebCore's platform abstraction
    interfaces (chrome client, editor client, etc.) — this is the seam where WebCore calls out into Qt/hipe
    behavior, including the `ContextMenuCallback` mechanism that replaced Qt's native context menu.
  - `Source/WebKit/qt/declarative/`, `Plugins/`, `examples/`, `docs/` — QML integration, plugin loading,
    examples/docs inherited from upstream; lower priority for hipecore-specific work.
- `Source/bmalloc` — custom allocator, built conditionally.
- `Source/cmake/` — all `Find*.cmake` modules and shared CMake logic (`OptionsQt.cmake` has the full list of
  `ENABLE_*` feature flags and their Qt-port defaults; `WebKitFeatures.cmake` defines the flags themselves).
- `Tools/` — leftover upstream WebKit tooling: `TestWebKitAPI` (C-level `TestWTF` / `TestWebCore`, distinct
  from the Qt-level tests in `tests/`), `Scripts/` (Perl build wrappers), `qt/`, `qmake/`, `jhbuild/`. The
  layout-test harness (`DumpRenderTree`, `ImageDiff`, `WebKitTestRunner`) has been deleted.

### Key mental model

WebCore is the actual browser engine (DOM, layout, rendering) and is largely upstream code left alone
except where features are being actively stripped. `Source/WebKit/qt/Api` is the thin, hipecore-owned
surface that Hipe actually calls into — most feature work (new `QWebElement` methods, selection/caret
behavior, the C++ event listener system) happens there, not in WebCore itself. When a `QWebElement` method
needs new behavior from the engine, check whether WebCore already exposes it via `Node`/`Element`/`Frame`
before adding new WebCore code — the strong preference throughout this fork's history is to expose more of
WebCore through `QWebElement` rather than reintroduce JS-based bindings.
