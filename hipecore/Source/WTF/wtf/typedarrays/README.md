# wtf/typedarrays — carryover from JavaScriptCore

These headers are the C++ typed-array subsystem (`ArrayBuffer`, `ArrayBufferView`,
`DataView`, the `Uint8Array` … `Float64Array` views, the generic view template and
its adaptors, `TypedArrayType`, and the `GCIncomingRefCounted` base that
`ArrayBuffer` still derives from). They originated in
`Source/JavaScriptCore/runtime/` (and `Source/JavaScriptCore/heap/` for
`GCIncomingRefCounted.h`).

hipecore has no JavaScript engine — `Source/JavaScriptCore/` was deleted in
Bucket 5.7 — but WebCore genuinely needs these types for canvas `ImageData`,
WebGL buffers, `<video>`/MSE (`SourceBuffer`, `DataView` box parsing), `DataCue`,
Web Audio channel buffers and the SVG/CSS filter pipeline. So the pure-C++,
header-only remainder was relocated here rather than reimplemented.

## Still-open cleanup

* The classes are still in `namespace JSC`. WebCore refers to them as
  `JSC::ArrayBuffer` / `JSC::Uint8ClampedArray` etc. (or bare, via `using`).
  Renamespacing to `WTF::` across the ~35 WebCore consumers is a follow-up; the
  `wtf/typedarrays/` path plus the retained `JSC::` namespace is the "in
  transition" marker.
* JS-wrapper hooks have already been stripped: `ArrayBuffer::transfer()` and the
  `Weak<JSArrayBuffer>` cache (Bucket 5.5), the virtual `wrap(ExecState*,
  JSGlobalObject*)` on the views, `TypedArrayAdaptors`' `toJSValue()` and the
  `#include "JSCJSValue.h"` / `"MathCommon.h"` (the ECMAScript ToInt32 it needed
  is now inlined here as `typedArrayToInt32`), and `TypedArrayType`'s
  `typeForTypedArrayType()` / `constructorClassInfoForType()` / the `printInternal`
  dataLog hook (Bucket 5.7).
* `GCIncomingRefCounted`'s out-of-line template methods lived in
  `GCIncomingRefCountedInlines.h`, which was *not* brought over — they only
  integrated `ArrayBuffer` with the JS GC's incoming-reference tracking and have
  no caller here. The base itself is retained because `ArrayBuffer` derives from
  it (through `DeferrableRefCounted`, which is plain WTF).
