# Paragraph breaks (U+2029)

Status: draft, 2026-10-05. Not built.

A client puts a paragraph break in text by sending U+2029 PARAGRAPH SEPARATOR (UTF-8 `E2 80 A9`). The engine shows
it as a line break followed by a themeable gap and/or indent. The character stays in the DOM, so text reads back
exactly as it was sent. Until now a client could only choose between `\n` (a line break, no gap) and markup (extra
elements, which change what reads back).

## What clients see

### Display

- U+2029 is a **forced line break in every element**, whatever its `white-space` (Unicode defines it as a mandatory
  break). A `\n` keeps its current meaning: a break only where `white-space` preserves newlines.
- The line that ends with U+2029 gets extra space below it: `-hipe-paragraph-spacing`.
- The first line after U+2029 is indented: `-hipe-paragraph-indent`.
- The character itself has no width and is never drawn.
- Spaces next to it behave as they do next to a `<br>`: in collapsing modes, spaces before it at the end of a line
  and after it at the start of the next line are not shown.

| Text | Shows as |
|---|---|
| `a`U+2029`b` | `a`, gap, `b` (indented if an indent is set) |
| `a`U+2029U+2029`b` | `a`, gap, an empty line, gap, `b` |
| U+2029`b` | an empty first line, gap, `b` |
| `a`U+2029 at the end of the element | `a`; no empty line and no gap after the last line (as a trailing `\n` in `pre-wrap`) |
| `a\nb` | unchanged: a break in `pre`, `pre-wrap` and `pre-line`, a space otherwise |

### CSS properties

Both are inherited, so a theme sets them once (e.g. on `body`) and they apply everywhere.

| Property | Values | Default | Effect |
|---|---|---|---|
| `-hipe-paragraph-spacing` | a number (a multiple of the line height, like a unitless `line-height`) or a length; not negative | `0.5` | Extra space below each line that ends with U+2029 |
| `-hipe-paragraph-indent` | a length or a percentage of the containing block's width, like `text-indent`; may be negative | `0` | Indent of the first line after each U+2029, at the line's start edge (the right in right-to-left text) |

The values come from the element that directly contains the U+2029. A theme that indents instead of spacing sets
`-hipe-paragraph-spacing: 0; -hipe-paragraph-indent: 2em`. The first line of an element is still indented by
`text-indent` only. `GET_STYLE` reads both properties back.

### Reading back, caret and editing

- `GET_CONTENT` modes 0, 2 and 3 return U+2029 as sent; mode 1 (HTML) contains it as the character.
- Caret offsets count it as one character, as they already do. The caret after it sits at the start of the next
  line, as after `\n`.
- Line-based caret movement, clicking and selection follow the new lines.
- Typing, deleting and pasting treat it like any other character. Return does not insert it; a client that wants
  Return to make a paragraph inserts it with `EDIT_ACTION` `"t"`.
- Commands that work on whole paragraphs (select paragraph, move by paragraph) don't treat it as a paragraph
  boundary. Possible later.
- A caret after a U+2029 at the very end of an editable element shows at the end of the previous line, as it does
  after a trailing `\n` (existing WebKit behaviour).
- `<textarea>` values break at it too.

### What does not change

- hiped: no change. Text modes 0, 1, 2 and 3 pass U+2029 through as it is.
- Text without U+2029 lays out exactly as before.

## Engine

All in hipecore's WebCore; hiped is untouched.

1. **Properties.** `-hipe-paragraph-spacing` and `-hipe-paragraph-indent` in `CSSPropertyNames.in` (inherited), parser
   validation, style-builder conversion, two fields in `StyleRareInheritedData` with `RenderStyle` accessors, and
   computed-style output for `GET_STYLE`. The spacing keeps whether it is a number or a length, so a number follows
   the line height.
2. **Forced break.** One predicate for "forced break at this position": `\n` where newlines are preserved, or U+2029
   always. Used where the line code now tests `'\n'` with `preserveNewline()`: `InlineIterator::atTextParagraphSeparator`,
   `BreakingContext.h` (whitespace classification, break opportunities, the forced-break branch, trailing
   whitespace), `InlineTextBox::isLineBreak`, `RenderText`'s preferred-width and whitespace scans, and
   `RenderBlockLineLayout`'s collapsible-whitespace test. The character stays in the renderer's text: mapping it to
   `\n` would make `GET_CONTENT` mode 2 read back `\n`.
3. **Simple line layout.** Text containing U+2029 is kept out of it (one new avoidance reason) and uses the full line
   layout, so only one path needs the gap and the indent.
4. **Gap.** A root line box that ends with a U+2029 break records the spacing, and it is included in the line's
   bottom (`lineBottomWithLeading`), so the next line starts lower and lines reused by incremental layout keep it.
   No spacing after the last line of the block.
5. **Indent.** The existing hook for indenting after a hard break (`requiresIndent`, used by CSS3
   `text-indent: each-line`, which is compiled out) gets a U+2029 case using `-hipe-paragraph-indent`, everywhere the
   line width is worked out (line breaking and positioning), so wrapping accounts for the indent.

New CSS properties change generated code, so this needs a clean engine build (about 1¼ hours here) and a clean
build again before pushing.

## Tests

- **`server/tests/text.c`** (new, run by `run.sh`), on a private hiped:
  - heights: U+2029 in `pre-wrap` and `normal` is one line plus the spacing taller than the same text without it;
    `\n` in `normal` adds nothing;
  - spacing set as a number, as a length, and as 0;
  - indent: the x position of the character after U+2029 (`GET_RANGE_GEOMETRY`) moves by the indent, and the first
    line doesn't;
  - read-back: modes 0 and 2 return the text unchanged;
  - caret: an offset after U+2029 round-trips;
  - `GET_STYLE` returns both properties;
  - a `<textarea>` value breaks;
  - text without U+2029 has the same height as before.
- Engine `ctest` for regressions.

## Manual

- "Tags, attributes and styles": a "Paragraph breaks" section with the table above and both properties.
- `SET_TEXT` and `APPEND_TEXT`: one line saying U+2029 makes a paragraph break in every text mode, with a link.
