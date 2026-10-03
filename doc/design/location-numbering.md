# Location numbering (design D, v2)

Status: v2, 2026-10-04 (with the Appscape session's review). Step 1 built: hipecore a26aa39, hipe 9e1286d. Supersedes the v1 phase plan. Phase 1 of v1 is built (hipecore 94ab5ac, hipe 0e4d831,
f3d664b, facae70); v2 reworks its internals around what building it taught us. The protocol clients see is
unchanged apart from the behaviour change in section 5.

## What phase 1 taught us

1. The engine is the natural owner of locations, not just the place to store a number. Keeping ownership in hiped
   produced two server-side tables kept in step by hand, strings handed back to hiped, and post-hoc DOM scans.
2. Engine-side work is cheap; hiped-side work after the fact costs per instruction (the first scan cost Appscape's
   benchmark 9-12%; doing it on the parsed fragment cost nothing measurable).
3. Undo re-inserts the same node object, so a number's lifetime follows the node's identity, which only the engine
   sees.
4. The `hipe-loc` attribute is only a way to carry a number in. The registry is the truth; anything that shows numbers
   (read-back) comes from the registry, never from a stored attribute.
5. Client allocation holds up: reservation separate from sending, out-of-order numbers, per-session pools.

## Principles

- **The client owns every number.** The server never makes one up. Each number names at most one element, and each
  element has at most one number, so comparing numbers compares elements.
- **The client retires its numbers.** Only the client knows which copies of a number it still holds, so a number
  becomes reusable only through `HIPE_OP_FREE_LOCATION`, never automatically.
- **The engine owns the mapping**; hiped does protocol and policy only.
- **Dedicated instructions manipulate the DOM directly**; markup goes through the parser.
- **No round trips** to number or use an element.

## Two records, different facts

| Where | Records | Authority for |
|---|---|---|
| Client (libhipe pool per session, C++ `hipe::loc` counts) | which numbers are allocated, and who still holds them | "is this number free to give out?" |
| Engine registry (per document) | which element each number names, and back | "which element is this?" |

They are kept in step by the protocol: a number comes into use on the server only through APPEND_TAG/INSERT_TAG
or a listed markup number, and leaves only through FREE_LOCATION. A new number already in use is fatal ("the client
and server disagree"). A reserved number never sent exists only in the client's pool; freeing it sends a
FREE_LOCATION the server ignores. A number bound to no element counts as in use on both sides until freed.

---

# Step 1 (next to build)

## 1. Number space (unchanged from v1)

64-bit numbers; `0` is the body; per container; top bit reserved; per-session client pools under the session lock,
lowest free first; cap of 2^22 numbers in use per container, fatal when exceeded.

## 2. Engine registry

One registry per document, in the QWebElement/frame layer:

- `bind(number, element or none)`, `free(number)`, `elementFor(number)`, `numberOf(element)`, and (for later steps)
  `nearestNumbered(node)`.
- One structure for both directions: dense for numbers up to about twice the count in use, sparse beyond, so memory
  follows the count in use.
- **Weak on elements.** A number stays reserved until freed, but does not keep its element alive. A removed element
  that nothing else references (not the document, not the undo history) is destroyed normally, and its number then
  names nothing. Undo still works: the undo history keeps the node alive, so it returns with its number. (v1 kept
  removed subtrees in memory until their numbers were freed.)
- hiped's own location table goes. hiped keeps only per-number side state (GET_CONTENT mode 4 snapshots, unfinished
  uploads, the selected canvas), dropped on free.

## 3. One binding path

- **Markup** (SET_TEXT / APPEND_TEXT mode 3): hiped decodes `arg[2]` and passes it to the engine's insertion call.
  The engine parses, takes `hipe-loc` off the fragment, binds the listed numbers it finds, and reports only the
  exceptions: listed numbers on no element (bound to none), duplicates (first wins), numbers already in use. hiped
  turns these into fatal reasons or notices (rules table as v1).
- **APPEND_TAG / INSERT_TAG**: the same registry `bind()`, on an element created directly (section 5).
- **`hipe-loc` never stays in the document**: removed on every engine insertion path (setInnerXml, appendInside,
  prependOutside, setOuterXml, prependInside, appendOutside, the enclose functions), refused by SET_ATTRIBUTE. Paste
  and drop: step 2.

## 4. Lists and numbers

- `arg[2]` and FREE_LOCATION lists are decoded as **ranges**, never expanded into one entry per number. Checks (well
  formed, no overlaps, within the cap, each number free) work on sorted intervals.
- `hipe-loc` values are **decimal digits only**, as in lists (no spaces, signs or other forms).

## 5. APPEND_TAG and INSERT_TAG create elements directly

The engine creates the element with the DOM (no markup string, no parser):

- **Namespace from the parent**: an element appended inside SVG is an SVG element, inside MathML a MathML element,
  with the parser's exceptions (HTML inside `foreignObject` and the other integration points). SVG tag names get the
  parser's case table (`lineargradient` becomes `linearGradient`).
- **Name check by the engine**: an invalid name is refused (its number is bound to none, as now).
- **id, classes and text** (`arg[1..3]`) set directly. The text is data, and behaves exactly like SET_TEXT mode 0 on
  the same element: it reads back as sent, except that `\r\n` and `\r` become `\n` (as in mode 0). A newline at the
  start is kept, also in a `pre`, `textarea` or `listing` (the parser only drops one that follows a `<pre>` tag in
  markup). Void elements ignore text.
- **Behaviour change**: the element asked for is created exactly where asked. No wrappers are added (`tr` appended to
  a `table` goes in directly, without a `tbody`; `col` without a `colgroup`), and nothing is dropped (`td` appended to
  a `div` is inserted). Correctly placed elements look the same as before, and even a `tr` directly in a `table`
  renders the same; only the structure differs. The location returned always names the element asked for. Known
  clients already append a `tbody` first; the manual's advice to do so stays. The "Tags, attributes and styles" page's
  rule that a tag isn't created where HTML doesn't allow it becomes a rule for markup only.

## 6. Events report the element's current number

A listener reports the number its element has **when the event fires**, not the one captured when it was requested.
After FREE_LOCATION the element has no number, so its own events stop; a reused number only ever reports events for
its own element. (In v1 a freed number kept receiving its old element's events, and could be confused with the
number's new element.) The Locations page states both consequences: event requests belong to the element, not the
number, so a reused number never inherits listeners; and freeing a location you still want events from silences it.

## 7. Elements destroyed while numbered

- An `iframe` with a connected child client is torn down when it leaves the document, as DELETE does today: the
  child is disconnected and the parent gets its frame event, reported with the iframe's number while that number is
  still bound. Weak holding changes nothing here, because a frame is detached on removal, not on destruction. Deleting
  an iframe and freeing its number, in either order, works.
- hiped's per-number side state (mode 4 snapshot, unfinished upload) is kept until the number is freed, even if its
  element has been destroyed; instructions to the number do nothing meanwhile.

## 8. FREE_LOCATION

The one way a number becomes reusable. Single or listed. Clears the registry entry and hiped's side state. Leaves a
still-present element in the document, unnumbered. Freeing a number not in use does nothing.

## 9. Unchanged from v1

The protocol (`arg[2]` lists, ranged FREE_LOCATION, SERVER_DENIED reasons, SERVER_NOTICE, protocol version check),
libhipe (`hipe_reserve_location(s)`, `hipe_send_markup`, `hipe_last_error`, per-thread `hipe_newest_location()`),
and the manual's Locations page (updated for sections 2, 5 and 6).

## 10. Hardening (part of step 1)

- **Tests in the repo**, runnable with one command against a private hiped: the phase 1 suite (binding table, fatal
  cases, out-of-order numbers, threads, sessions, free/reuse/undo, memory over repeated rebuilds, old clients
  refused), plus: numbered markup inside a framed app; mode 3 as the first instruction on an uninitialised body;
  INSERT_TAG; multi-megabyte markup; APPEND_TAG text into a `pre` starting with newlines reads back exactly, as
  with SET_TEXT mode 0; an iframe with a child client deleted and its number freed, in both orders; malformed and fuzzed lists; section 5's namespace, case and text cases; event
  silencing after free; a weakly held element destroyed after removal and its number naming nothing.
- **libhipe**: checked allocation and size arithmetic in `hipe_send_markup`; thread-safe `hipe_last_error(0)`.
- **Performance**: Appscape's `bench.sh 20000 50000` and the APPEND_TAG/mode 3 microbenchmark must not regress
  against the pre-phase-1 build.

---

# Later steps (each gets a short spec)

Each is a small extension of the registry:

- **Read-back with numbers**: GET_CONTENT mode 1 with `"loc"`; the serializer writes `hipe-loc` from the registry.
  Round trip gives the same DOM (not the same string). Strip `hipe-loc` from pasted and dropped HTML.
  **Keeping numbers through a re-render**: the engine knows which nodes a replacement removes, so a listed number
  whose element this same instruction removes moves to the new element; anywhere else "in use" stays fatal.
  **Replacing an element itself** (like `outerHTML`) keeping its number: SET_TEXT's spare `arg[3]`.
- **Positions**: events report the nearest numbered element at or above the target plus an offset (caret boundary
  nearest the pointer); caret/selection as `"number:offset"`. Open question: offset units (DOM text vs layout text).
- **Removal and return notices**: registry hooks on node removal and insertion; coalesced per editing command,
  delivered before that command's `input` event; reports returns (undo) as well as removals.
- **Editor features**: range highlights on WebCore's document markers; input events carrying their delta and a
  pushed selection-change; cancellable editing intents with undoable application; copy/cut interception;
  formatted-run read-back; observers, IME, a fallback focus target, hit testing, accessibility; a C++ markup builder.

## Not in this design

Server-assigned numbers, for a client that acts on the structure of HTML it didn't write. If added: from the
reserved top-bit range, explicitly weak.

## Decided along the way

1. Read-back with numbers is GET_CONTENT mode 1 with `"loc"`; plain mode 1 stays clean.
2. Removal notices: per numbered subtree, editing-caused only unless `"all"`.
3. Key events carry no offset.
4. No automatic freeing on removal or replacement: the client retires its numbers.
5. Cap 2^22 numbers in use per container.
6. (v2) The engine owns the registry, held weakly; hiped's table is removed.
7. (v2) APPEND_TAG/INSERT_TAG create elements directly; no parser wrappers or dropping.
8. (v2) Events report the element's current number.
