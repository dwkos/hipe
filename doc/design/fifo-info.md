# FIFO relationship properties (HIPE_OP_FIFO_INFO)

Status: 2026-10-08, approved and built (hiped, periscope ea97ea9, fileshell export). Requested by fileshell (export)
for "notes.txt is open in Appscape". The owner approved the initial keys `ability` and `peer-title` only; the
others below are candidates, not part of the protocol until needed.

One new instruction carries key/value properties about a FIFO relationship, so new information needs a new key
in a table, not a new opcode or argument. It keeps apart what the framing manager states (trustworthy) and what
the other application says about itself.

## The instruction

`HIPE_OP_FIFO_INFO` (87). Like the other FIFO instructions, hiped relays it without reading it.

| Argument | Meaning |
|---|---|
| `arg[0]` | The resource path, as in `HIPE_OP_FIFO_RESPONSE`. |
| `arg[1]` | Properties stated by the framing manager. |
| `arg[2]` | Properties the other application stated about itself. |
| `arg[3]` | Reserved, empty. |

Properties are lines of `key=value`, split at the first `=`. Keys are lower case `a`-`z`, `0`-`9` and `-`;
values are UTF-8 text without newlines. Each message carries only the keys that changed: a key replaces its
earlier value from the same source, and an empty value means the value is no longer known. Receivers ignore
keys they don't know. Keys not in the manual's table start with `x-` and the sender's name, e.g. `x-appscape-`.

## Who sends what

| Sender | When | Arguments | The framing manager |
|---|---|---|---|
| An application (either end) | When something it states about itself is known or changes. | `arg[2]` = its properties, `arg[1]` empty. | Relays it by path to the other end: `arg[2]` unchanged, `arg[1]` its own properties or empty. Never copies anything from an application into `arg[1]`. |
| An application | To ask for the framing manager's properties. | `arg[1]` and `arg[2]` empty. | Answers the asker only, with `arg[1]`; `arg[2]` empty. Doesn't relay. |
| The framing manager | To both ends after relaying a successful `HIPE_OP_FIFO_RESPONSE`, and when its properties change. | `arg[1]` = its properties, `arg[2]` empty. | — |

`arg[1]` came from the framing manager. hiped only registers the opcode as relayed, like the other FIFO instructions.
`arg[1]` came from the framing manager. Nothing in hiped changes.

Properties describe **the other end** of the relationship, not "the host" or "the client", so a role swap
(`s`) changes nothing.

## Keys

Defined (both from the framing manager, `arg[1]`): `ability`, `peer-title`. Candidates for later: `made`, `name`,
`size`.

From the framing manager (`arg[1]`):

| Key | Value |
|---|---|
| `peer-title` | The other application's title, as the framing manager shows it to the user. |
| `ability` | The name of the ability the user chose (the host's `HIPE_OP_FIFO_ADD_ABILITY` name). |
| `made` | When the relationship was made: ISO 8601 date and time, UTC, e.g. `2026-10-08T14:03:00Z`. |

From an application about itself (`arg[2]`):

| Key | Value |
|---|---|
| `name` | The application's name, e.g. `Appscape`. |
| `size` | The length in bytes of the data the sender will write in the next transfer. Sent before the sender's `HIPE_OP_FIFO_OPEN` (client) or before relaying it back (host), so the reader can show progress. |

## Compatibility

- Applications that don't know the instruction ignore it, as they do any unknown instruction. Applications that
  use it must work without it: it may never arrive.
- A framing manager that doesn't know it drops it, so the other end never sees it. (To check: periscope and
  fileshell's mdiframe ignore unknown opcodes from children.)
- A top-level application with no framing manager gets nothing back.

## Not chosen

- A query-only design (ask the framing manager about the other end): it can't carry what an application says
  about itself, such as `size`, and needs polling to see changes. The empty-arguments form covers asking.
- Putting provenance in the key names (`fm-title`) in one argument: a framing manager would have to filter every
  relayed line, and one that forgets lets an application forge them. Two arguments make it structural.
