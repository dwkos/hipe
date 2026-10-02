hipecore
========

hipecore is Hipe's display engine: the part of the project that turns a document of HTML
elements and CSS styles into pixels. The display server, hiped (in `../server`), is built on
it.

It is a cut-down fork of the Qt5WebKit library. It keeps WebKit's HTML/CSS/DOM rendering
engine but strips out the browser: no JavaScript, no network, no plugins, no persistent
storage. hiped drives the DOM directly through an extended `QWebElement` C++ API rather than
through scripts, so the parts of a web engine that exist to support stateless, asynchronous,
networked pages are dead weight here.

Forked from Qt5WebKit 5.212.0-alpha4 (`qtwebkit-1.212.0-alpha4.tar.xz`, from
https://github.com/qtwebkit/qtwebkit/releases/). The newer community fork was not used:
different build requirements, it breaks iframes in Hipe, and its aims (WebKit2, better
JavaScript, browser features) are the opposite of this project's.


What changed vs. QtWebKit
-------------------------

### Added

- `QWebElement` methods for working on the DOM directly: reading and writing content,
  selection and caret handling, finding text, and measuring text.
- A C++ callback event system (`hipecoreeventlistener`): DOM events go straight to a
  `std::function`, and an event's default action can be cancelled by rule.
- `QWebPage::insertText()`, which inserts text as typing would, as one undoable step.
- Canvas 2D drawing through `QWebElement`, with no JavaScript involved.
- Image, audio and video data given to an element directly as bytes, whole or in chunks.
- Embedding an X11 window in an `<object>` element.
- Frame snapshots that render exactly what is on screen
  (`QWebFrame::renderContentsForSnapshot()`).

### Removed

- **JavaScript.** JavaScriptCore, the JS bindings in WebCore, the JIT and the Web
  Inspector are all gone, along with the Streams API and Web Timing, which existed only
  for scripts. A `<script>` element never executes; the parser skips its contents. Only
  the element classes the parser needs for that remain.
- **Networking.** hipecore has no network access, and Qt5Network is not a dependency. Only
  `data:` and `about:` URLs can be constructed at all. HTTP, FTP, WebSockets, `fetch()`
  and the offline application cache are removed. Links don't navigate: a client that
  shows hypertext replaces the content itself. The browser-history API is removed.
- **Storage.** Web SQL, IndexedDB, Web Storage (`localStorage` / `sessionStorage`), the
  Quota API, the cookie store, and the SQLite dependency.
- **Device and hardware APIs.** Geolocation, DeviceOrientation / DeviceMotion (and the
  Qt5Sensors dependency), Proximity, Vibration, Battery Status, WebRTC / MediaStream and
  AirPlay.
- **Other.** WebKit2, NPAPI plugins, WebAssembly, the website icon database, printing,
  and the layout-test tools (`DumpRenderTree`, `ImageDiff`, `WebKitTestRunner`).
  `QWebPluginFactory` (Qt widget embedding, unrelated to NPAPI) is kept.


Aims
----

- Optimise WebCore as the rendering back end for a synchronous local display server.
- Fix bugs in the DOM C++ interface.
- Keep the attack surface small: no scripting, no network, no arbitrary code execution.
- Cut secondary build dependencies and shrink the source distribution.


Tests
-----

`tests/` and `Tools/TestWebKitAPI` carry what remains of the stock QtWebKit test suites.
They are lightly maintained. `-DENABLE_API_TESTS=ON` builds the Qt API tests under
`tests/webkitwidgets/` (`qwebelement`, `qwebframe`, `qwebpage`, `qwebview`,
`qgraphicswebview`) plus `TestWTF` / `TestWebCore`; run them with `ctest`. Cases that
exercised removed features (JavaScript, URL navigation, resource loading, plugins, WebGL)
have been taken out.


Notes
-----

hipecore installs as two libraries, `libHipeCore` and `libHipeCoreWidgets`, with the headers
of both in `<prefix>/include/HipeCore` (included as `<HipeCore/QWebElement>` and so on). The
classes keep their `QWeb*` names. Because the library and header names are hipecore's own, it
can be installed on a system that also has QtWebKit. It also installs pkg-config files and
CMake packages named `HipeCore` and `HipeCoreWidgets`; it does not install qmake module files.


Licence and origin
------------------

hipecore is derived from Qt5WebKit 5.212.0-alpha4 and has been modified by
General Development Systems since February 2025. Files with non-trivial
changes carry a General Development Systems copyright line in their header.

Licensing is inherited from the original code and is stated in each file's own
header, mostly the GNU LGPL 2.1 (`LICENSE.LGPLv21`) and Apple's BSD-style
licence (`Source/WebCore/LICENSE-APPLE`), with bundled third-party code under
`Source/ThirdParty/`. Files added by hipecore carry their own licence headers.


Build
-----

    mkdir build && cd build
    cmake -G Ninja -DPORT=Qt -DCMAKE_BUILD_TYPE=Release ..
    ninja
    sudo ninja install

`PORT=Qt` is required. On the Raspberry Pi, build `-j2` until the AllInOne translation
units clear, then `-j4` — a blanket `-j4` can exhaust memory.

### Dependencies (Ubuntu/Debian)

    ninja-build bison gperf libjpeg-dev libpng-dev libicu-dev libxml2-dev
    libxslt1-dev qtbase5-private-dev libxcomposite-dev
    libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev

`sudo apt-get build-dep libqt5webkit5` pulls most of these. A failing configure/build names whatever is still missing.

Dependencies dropped relative to stock Qt5WebKit: `ruby-dev` and `ruby` (were only for
JavaScriptCore's Ruby assembler), `flex` (no lexer inputs remain — only bison grammars),
`libsqlite3-dev`, `libqt5sensors5-dev`, `libhyphen-dev`, `qtpositioning`,
`libqt5webchannel5-dev`, Qt5Network. The CMake configure no longer looks for any of these.

On Buildroot, the stock QtWebKit package's patches are already applied to this tree.


---

*Upstream QtWebKit was an open-source browser engine — WebKit's HTML/JS code began as a
branch of KDE's KHTML and KJS, made toolkit-independent by Apple, then ported back to Qt.
Upstream docs: https://github.com/annulen/webkit/wiki*
