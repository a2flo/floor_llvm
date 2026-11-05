; RUN: %python %S/Inputs/metal-function-constant-metadata-limits.py generate %s %t.max.ll 65528
; RUN: %clang_cc1 -triple air64-apple-macosx14.0.0 -x ir -disable-llvm-passes -emit-metallib -o %t.max.metallib %t.max.ll
; RUN: %python %S/Inputs/metal-function-constant-metadata-limits.py check %t.max.metallib
; RUN: %python %S/Inputs/metal-function-constant-metadata-limits.py generate %s %t.over.ll 65529
; RUN: not %clang_cc1 -triple air64-apple-macosx14.0.0 -x ir -disable-llvm-passes -emit-metallib -o %t.over.metallib %t.over.ll 2>&1 | FileCheck %s
; RUN: %python %S/Inputs/metal-function-constant-metadata-limits.py invalid %s %t.empty.ll empty
; RUN: not %clang_cc1 -triple air64-apple-macosx14.0.0 -x ir -disable-llvm-passes -emit-metallib -o %t.empty.metallib %t.empty.ll 2>&1 | FileCheck %s --check-prefix=INVALID
; RUN: %python %S/Inputs/metal-function-constant-metadata-limits.py invalid %s %t.short.ll short
; RUN: not %clang_cc1 -triple air64-apple-macosx14.0.0 -x ir -disable-llvm-passes -emit-metallib -o %t.short.metallib %t.short.ll 2>&1 | FileCheck %s --check-prefix=INVALID
; RUN: %python %S/Inputs/metal-function-constant-metadata-limits.py invalid %s %t.wide.ll wide
; RUN: not %clang_cc1 -triple air64-apple-macosx14.0.0 -x ir -disable-llvm-passes -emit-metallib -o %t.wide.metallib %t.wide.ll 2>&1 | FileCheck %s --check-prefix=INVALID
; INVALID: error: invalid Metal function constant metadata
; CHECK: error: Metal function constant metadata exceeds CNST limits
;
; A CNST payload is 2 count bytes plus (name length + 1 + 4) per record.
; Exercise the real writer at exactly UINT16_MAX bytes and one byte over it.
; The generated long metadata name avoids a huge checked-in test fixture.

target triple = "air64-apple-macosx14.0.0"
@fc = addrspace(2) externally_initialized constant i32 0, section "air.fc_initializer"

define void @entry() {
  %value = load volatile i32, i32 addrspace(2)* @fc
  ret void
}

!air.kernel = !{!0}
!air.function_constants = !{!2}
!0 = !{void ()* @entry, !1, !1}
!1 = !{}
!2 = !{i32 addrspace(2)* @fc, !"int", !"@LONG_NAME@", i32 0, i1 true}
