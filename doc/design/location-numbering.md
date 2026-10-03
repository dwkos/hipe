# Location numbering for markup (design D)

Status: draft 5, 2026-10-03; phase 1 implemented (hipecore 94ab5ac, hipe 0e4d831). Design D approved by the user. Phase 1 below is the spec to build first; later
phases are outlined with their open questions. No code yet.

## Goal

Elements created by markup (`HIPE_OP_SET_TEXT` / `HIPE_OP_APPEND_TEXT` mode 3) have no location, so a client
can't act on them. This design lets a client number any element it writes, in the same instruction and without
round trips. Later phases add read-back, positions and lifetime notices for structure that editing changes.
Rebuilding every client is acceptable.

## Principles

- **The client owns every number.** The server never makes one up. Each number names at most one element, and
  each element has at most one number, so comparing numbers compares elements.
- **Only what the client numbered can be addressed.** Elements made by the parser, by editing or by pasting are
  reached through positions and read-back (later phases), not by numbers of their own.
- **No round trips** to number or use an element.

---

# Phase 1: numbering markup elements

## 1. Number space

- A location is a 64-bit number. `0` means the body (or "none" in replies), as now.
- Numbers are **per container**: a framed app's numbers never collide with its parent's.
- Numbers are allocated by the client library **per session**: each session has its own pool, guarded by the
  session's send lock and released when the session closes. (Today the pool is process-global and unlocked: a
  second session in one process is disconnected for skipping numbers, and two threads can race on it.)
- **The top bit is reserved.** Client pools never produce numbers with it set. It is kept for possible
  server-assigned numbers later.
- hiped's table becomes **sparse** (a hash map from number to element, sized by the numbers in use). The rule "at
  most one more than the highest number so far" is dropped: with reservation separate from sending, numbers
  legitimately arrive out of order (reserved earlier but sent later, two threads, first-fit reuse). hiped checks
  that each new number:
  - is free in this container. A number already in use means the client and server disagree: fatal;
  - has the top bit clear: fatal otherwise;
  - keeps the container under a cap of **4,194,304 (2^22) numbers in use**: fatal otherwise. An editor numbering
    every line of a 50,000-line file uses about 50,000, so the cap leaves a wide margin.

## 2. Numbers stored in the engine

hipecore stores each element's number inside the element: internal data, not a DOM attribute. It isn't visible to
the document or to CSS, and it isn't copied by cloning, editing, copy or undo.

- `QWebElement::setHipeLocation(quint64)`, `QWebElement::hipeLocation()` (0 when none).
- hiped sets it whenever it binds a number (markup, APPEND_TAG, INSERT_TAG) and clears it when the number is
  freed. hiped uses it to answer "which number does this element have?" instantly, replacing the linear
  `findReferenceableElement()` scan. hiped's table (section 1) stays the way from number to element.

## 3. Numbering elements in markup

The client puts `hipe-loc="N"` on the elements it wants to address, and lists the numbers it reserved for this
markup in `arg[2]`:

```
HIPE_OP_SET_TEXT     location, arg[0] = markup, arg[1] = "3", arg[2] = reserved numbers
HIPE_OP_APPEND_TEXT  (same)
arg[2] examples: "57"   "57,58,93"   "1000-1499"   "57,1000-1499,1502"
```

| Case | Result |
|---|---|
| A listed number is on an element in the markup | It is bound to that element. The attribute is removed. |
| A listed number is on no element (the parser dropped the element, or the client didn't use the number) | It is bound to no element, like a refused tag: instructions sent to it do nothing, and it can be freed as usual. hiped sends one non-fatal `HIPE_OP_SERVER_NOTICE` per instruction, giving the count and the first few numbers. |
| A listed number is in use, has the top bit set, or would exceed the cap | Fatal, with a reason (section 6). |
| The same listed number on two elements | The first in document order is bound; the attribute is removed from both. Non-fatal notice. |
| A `hipe-loc` value that isn't in the list, or isn't a number | The attribute is removed and ignored. |
| `arg[2]` empty | No numbers are bound; every `hipe-loc` is removed. |

- Listed numbers are processed in the order given (ranges ascending).
- The engine does the attribute work while inserting: `QWebElement::setInnerXml()` / `appendInside()` take
  `hipe-loc` off the parsed fragment before it is inserted (so the attribute never reaches the document) and return
  each element that carried it with its value; `prependOutside()` removes it too. hiped only applies the rules
  above to that list; it never scans the document or the markup text.
- `HIPE_OP_SET_ATTRIBUTE` refuses the name `hipe-loc`. (Stripping it from pasted and dropped HTML belongs to
  phase 2, where read-back would otherwise show it.)
- **Replaced and cleared content:** numbers on elements that SET_TEXT replaces or CLEAR removes stay allocated
  until the client frees them. The detached element stays alive, with its whole subtree, until then. A client
  that re-renders a region frees the old numbers with one ranged FREE_LOCATION and reserves new ones.

**List syntax** (for `arg[2]` and FREE_LOCATION's list): comma-separated decimal numbers and ranges `a-b` with
`a <= b`, no spaces, at most the cap in total. A malformed list is fatal with a reason, and nothing in it is applied.

## 4. Other instructions

- **APPEND_TAG / INSERT_TAG:** unchanged on the wire (the new number still travels in `requestor`; all four
  arguments are in use) and unchanged internally, apart from recording the number in the engine (section 2) and
  the sparse table's checks (section 1).
- **Lookups** (`GET_BY_ID`, `GET_FIRST_CHILD`, `GET_LAST_CHILD`, `GET_NEXT_SIBLING`, `GET_PREV_SIBLING`): same
  meaning, the element's number or 0. They now find markup-numbered elements too, and are instant.
- **FREE_LOCATION:** also accepts a list in `arg[0]` (section 3 syntax), freeing many numbers in one instruction;
  the `location` field is then ignored. libhipe frees them in the session's pool too. Freeing a number also drops
  all other state hiped keeps for it: the GET_CONTENT mode 4 snapshot, an unfinished chunked SET_SRC upload, and
  the selected canvas if it was that element. Freeing a number that isn't in use does nothing.
- **Freeing clears the number stored on the element** (section 2). This is a stated rule, not a detail: if the
  element later returns to the document (an editing undo re-inserts the same node object), it comes back without a
  number, so it can never clash with an element the client has since given that number.

## 5. Protocol version

libhipe sends its protocol version in `HIPE_OP_REQUEST_CONTAINER` `arg[3]` (unused today), starting at `"3"`.
hiped refuses a missing or older version with `HIPE_OP_SERVER_DENIED` and a reason ("client built against an
older libhipe; rebuild it"). This applies on every path into a container: the keyfile, `HIPE_HOSTKEY`, and frame
keys. The refusal happens before the key is used, so a refused framed client leaves its frame vacant and the key
still valid (the parent gets no frame event). Later phases only add to the protocol and don't need a new version.

## 6. Errors and notices

- Before any hard disconnect for a protocol violation, hiped sends `HIPE_OP_SERVER_DENIED` with `arg[0]` a short
  reason, e.g. `"location 57 already in use"`. (Today the reason only goes to hiped's stderr.)
- Non-fatal client bugs are reported with a new `HIPE_OP_SERVER_NOTICE` (opcode 86), `arg[0]` the message.
- libhipe prints both to stderr and keeps the last fatal reason: `const char* hipe_last_error(hipe_session)`, so an
  app can tell its user why it lost its display.

## 7. Client library (libhipe)

- Per-session pool (section 1).
- `hipe_newest_location()` keeps its signature and meaning: the number this thread's last APPEND_TAG or
  INSERT_TAG allocated. Reservations don't change it. A thread sending to two sessions reads it straight after
  each APPEND_TAG.
- `hipe_loc hipe_reserve_location(hipe_session)`: reserves one number, sending nothing.
- `hipe_loc hipe_reserve_locations(hipe_session, size_t n)`: reserves `n` consecutive numbers, first-fit (freed
  runs are reused), and returns the first.
- `int hipe_send_markup(hipe_session, hipe_loc where, const char* markup, int append, const hipe_loc* reserved,
  size_t count)`: sends mode 3 with `arg[2]` built from the list, merging consecutive numbers into ranges.
- `const char* hipe_last_error(hipe_session)` (section 6).
- Reserved numbers that are never sent must be freed, as with any number.
- (A C++ markup builder comes later; `hipe::loc` can already wrap reserved numbers.)

## 8. Documentation

- A new **"Locations"** page in the manual: who allocates numbers, numbering markup, lookups, freeing (single and
  ranged), what happens to numbers in replaced content (including that a detached element and its subtree stay
  alive until freed), the cap, and the fatal cases. Instruction pages link to it.
- Pages: SET_TEXT, APPEND_TEXT (mode 3 `arg[2]`), FREE_LOCATION, SET_ATTRIBUTE (`hipe-loc` refused),
  SERVER_DENIED (reason), SERVER_NOTICE (new), REQUEST_CONTAINER (version), plus the libhipe API reference and
  `hipe_instruction.h`.

## 9. Tests

- **Engine (ctest):** number set/get/clear; not copied by `cloneNode`, editing copy or undo; a cleared number
  stays cleared when undo re-inserts the node.
- **hiped (test clients on a private hiped):**
  - every row of the section 3 table, including ranges and gaps (`"57,1000-1499"`), with the notices received
  - the fatal cases (in use, top bit, cap, malformed list), each with its reason received
  - lookups returning numbers of markup elements; events, SET_ATTRIBUTE and SET_SRC on markup-numbered elements
  - numbers sent out of order (reserved earlier, sent later; two threads; first-fit reuse) all accepted
  - two sessions in one process
  - a 500-line group in one instruction, rebuilt repeatedly and freed with one ranged FREE_LOCATION, checking
    that hiped's memory doesn't grow; ranged frees clearing mode 4 snapshots and unfinished uploads
  - an old client (no version) refused with a reason, both top-level and through a frame key (the key stays
    usable by a rebuilt client)
  - free, reuse, undo: an editing deletion removes element N, the client frees N and gives N to a new element,
    then undo brings the old node back; the new element keeps N and the returned node has none
- **Performance:** Appscape's progressive build (`tests/live/bench.sh 20000 50000`; 1.48 s and 3.59 s to
  highlight on 2026-10-03) must not regress.
- **Regression:** periscope, Appscape, hipexwm and the samples, rebuilt, on a private display.

---

# Later phases (each gets its own short spec)

## Phase 2: read-back with numbers
- GET_CONTENT mode 1 with `arg[1] == "loc"` writes `hipe-loc="N"` on numbered descendants; plain mode 1 stays
  clean; numbers never go into clipboard HTML.
- Round trip: read with `"loc"`, insert with mode 3, gives the same DOM (elements, attributes, text, white space;
  not the same string). Trap: the newline after `<pre>`.
- Strip `hipe-loc` from pasted and dropped HTML and from editing commands that insert HTML.
- Open questions:
  - **Keeping numbers through a re-render** (must ship with read-back, which depends on it). Re-inserting
    read-back HTML with the same numbers is fatal under phase 1, because the old elements still hold them.
    Proposed: a listed number moves to the new element **only** if its current element is inside the content
    this same instruction removes (so it can't be addressed after the instruction anyway). Anywhere else, "in
    use" stays fatal. (Relaxes phase 1; can't break working clients.)
  - **Replacing an element itself** (like `outerHTML`), e.g. turning a numbered paragraph into a heading while
    keeping its number. Proposed: a mode 3 option in SET_TEXT's spare `arg[3]`.

## Phase 3: positions
- Events: `arg[2]` nearest numbered element at or above the target within the listener's element; `arg[3]` the
  caret boundary nearest the pointer (`caretPositionFromPoint`), for pointer events only. Across frames the parent
  sees the iframe's number in its own numbering.
- Caret and selection: `GET_CARAT_POSITION` with `arg[0] == "n"` replies `"number:offset"`.
- Open question: **offset units.** Text deltas (GET_CONTENT mode 4) count characters of the DOM text; the caret
  API counts displayed (layout) text: block boundaries as newlines, collapsed spaces once, hidden content not at
  all. They agree in a `pre` but not in ordinary rich text. Appscape's view: editors need one unit for
  everything within an element, and it should be layout text, so add an opt-in layout-text form of the mode 4
  delta and make positions and highlights use layout text, leaving mode 4's default unchanged. Either way, the
  Locations page gets one table stating each API's unit.

## Phase 4: removal and return notices
- Opt-in `"detach"` event, per numbered subtree, coalesced per editing command, delivered before that command's
  `input` event; editing-caused only unless `"all"`.
- Open questions:
  - **Undo re-inserts the same node object** (verified: `RemoveNodeCommand::doUnapply`), so a numbered element
    that editing removed can come back with its number. The notice reports returns as well as removals, in the
    same coalesced message (a removed list and a returned list). A node whose number was freed comes back
    unnumbered and isn't reported (phase 1 rule).
  - **Instructions sent to a detached element act on it invisibly**, and those changes appear if undo brings it
    back. Document it and recommend freeing numbers when notified.

## Phase 5: APPEND_TAG through the markup binding
One binding mechanism instead of two, if Appscape's benchmark shows no cost.

## Phase 6 and beyond: editor features
Range highlights anchored to text (on WebCore's document markers); input events that carry their delta and a
pushed selection-change event; cancellable editing intents (like `beforeinput`) with undoable application by the
client; copy/cut interception; formatted-run read-back; observers, IME, a fallback focus target, hit testing,
accessibility; a C++ markup builder.

## Not in this design
Server-assigned numbers, for a future client that acts on the structure of HTML it didn't write. If added, they
come from the reserved top-bit range and are explicitly weak (reported when the engine destroys the node).

## Decided along the way
1. Read-back with numbers is GET_CONTENT mode 1 with `"loc"`; plain mode 1 stays clean.
2. The removal notice is requestable per numbered subtree and reports editing-caused removals only unless `"all"`.
3. Key events carry no offset.
4. No automatic freeing of numbers inside replaced content; a ranged FREE_LOCATION does it in one instruction.
5. The live-number cap is 2^22 per container, fatal with a reason when reached.
