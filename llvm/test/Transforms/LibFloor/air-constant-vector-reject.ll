; RUN: split-file %s %t
; RUN: not opt -enable-new-pm=0 -PointerBCFixup -verify-each -disable-output %t/escape.ll 2>&1 | FileCheck %s --check-prefix=ESCAPE --implicit-check-not=must_not_appear
; RUN: not opt -enable-new-pm=0 -PointerBCFixup -verify-each -disable-output %t/write.ll 2>&1 | FileCheck %s --check-prefix=WRITE
; RUN: not opt -enable-new-pm=0 -PointerBCFixup -verify-each -disable-output %t/volatile.ll 2>&1 | FileCheck %s --check-prefix=VOLATILE
; RUN: not opt -enable-new-pm=0 -PointerBCFixup -verify-each -disable-output %t/atomic.ll 2>&1 | FileCheck %s --check-prefix=ATOMIC
; RUN: not opt -enable-new-pm=0 -PointerBCFixup -verify-each -disable-output %t/unknown-root.ll 2>&1 | FileCheck %s --check-prefix=UNKNOWN
; RUN: not opt -enable-new-pm=0 -PointerBCFixup -verify-each -disable-output %t/nominal-mismatch.ll 2>&1 | FileCheck %s --check-prefix=NOMINAL
; RUN: not opt -enable-new-pm=0 -PointerBCFixup -verify-each -disable-output %t/unanchored-cycle.ll 2>&1 | FileCheck %s --check-prefix=CYCLE
; RUN: not opt -enable-new-pm=0 -PointerBCFixup -verify-each -disable-output %t/regroup-lanes.ll 2>&1 | FileCheck %s --check-prefix=REGROUP
;
; Unsupported graphs must reach the legacy rejection without silently
; generalizing the constant-buffer rewrite. Diagnostics contain opcode names,
; not the function, type, or SSA names of the source program.
;
; ESCAPE: error: invalid pointer bitcast: failed to run bitcast fixup (unhandled instructions); unsupported users: getelementptr; constant-vector graph: unsupported pointer user (ptrtoint)
; WRITE: error: invalid pointer bitcast: failed to run bitcast fixup (unhandled instructions); unsupported users: getelementptr; constant-vector graph: unsupported pointer user (store)
; VOLATILE: error: invalid pointer bitcast: failed to run bitcast fixup (unhandled instructions); unsupported users: getelementptr; constant-vector graph: volatile or atomic load (load)
; ATOMIC: error: invalid pointer bitcast: failed to run bitcast fixup (unhandled instructions); unsupported users: getelementptr; constant-vector graph: volatile or atomic load (load)
; UNKNOWN: error: invalid pointer bitcast: failed to run bitcast fixup (unhandled instructions); unsupported users: select; constant-vector graph: unmapped argument or constant pointer root
; NOMINAL: error: invalid pointer bitcast: failed to run bitcast fixup (unhandled instructions); unsupported users: select; constant-vector graph: inverse root nominal type mismatch (bitcast)
; CYCLE: error: invalid pointer bitcast: failed to run bitcast fixup (unhandled instructions); unsupported users: phi; constant-vector graph: unanchored inverse pointer cycle (phi)
; REGROUP: error: invalid pointer bitcast: failed to run bitcast fixup (unhandled instructions); unsupported users: getelementptr{{$}}

;--- escape.ll
target triple = "air64-apple-macosx14.0.0"
%must_not_appear_type = type { [8 x <4 x float>] }
define cc 76 void @must_not_appear_function(%must_not_appear_type addrspace(2)* %buffer, i64 addrspace(1)* %out) {
  %must_not_appear_view = bitcast %must_not_appear_type addrspace(2)* %buffer to <4 x i32> addrspace(2)*
  %lane = getelementptr <4 x i32>, <4 x i32> addrspace(2)* %must_not_appear_view, i64 0, i64 1
  %escaped = ptrtoint i32 addrspace(2)* %lane to i64
  store i64 %escaped, i64 addrspace(1)* %out, align 8
  ret void
}

;--- write.ll
target triple = "air64-apple-macosx14.0.0"
%Buffer = type { [8 x <4 x float>] }
define cc 76 void @write(%Buffer addrspace(2)* %buffer) {
  %view = bitcast %Buffer addrspace(2)* %buffer to <4 x i32> addrspace(2)*
  %lane = getelementptr <4 x i32>, <4 x i32> addrspace(2)* %view, i64 0, i64 1
  store i32 42, i32 addrspace(2)* %lane, align 4
  ret void
}

;--- volatile.ll
target triple = "air64-apple-macosx14.0.0"
%Buffer = type { [8 x <4 x float>] }
define cc 76 void @volatile_load(%Buffer addrspace(2)* %buffer, i32 addrspace(1)* %out) {
  %view = bitcast %Buffer addrspace(2)* %buffer to <4 x i32> addrspace(2)*
  %lane = getelementptr <4 x i32>, <4 x i32> addrspace(2)* %view, i64 0, i64 1
  %value = load volatile i32, i32 addrspace(2)* %lane, align 4
  store i32 %value, i32 addrspace(1)* %out, align 4
  ret void
}

;--- atomic.ll
target triple = "air64-apple-macosx14.0.0"
%Buffer = type { [8 x <4 x float>] }
define cc 76 void @atomic_load(%Buffer addrspace(2)* %buffer, i32 addrspace(1)* %out) {
  %view = bitcast %Buffer addrspace(2)* %buffer to <4 x i32> addrspace(2)*
  %lane = getelementptr <4 x i32>, <4 x i32> addrspace(2)* %view, i64 0, i64 1
  %value = load atomic i32, i32 addrspace(2)* %lane monotonic, align 4
  store i32 %value, i32 addrspace(1)* %out, align 4
  ret void
}

;--- unknown-root.ll
target triple = "air64-apple-macosx14.0.0"
%Buffer = type { [8 x <4 x float>] }
define cc 76 void @unknown_root(%Buffer addrspace(2)* %buffer, <4 x i32> addrspace(2)* %unknown, i1 %condition, <4 x i32> addrspace(1)* %out) {
  %view = bitcast %Buffer addrspace(2)* %buffer to <4 x i32> addrspace(2)*
  %chosen = select i1 %condition, <4 x i32> addrspace(2)* %view, <4 x i32> addrspace(2)* %unknown
  %value = load <4 x i32>, <4 x i32> addrspace(2)* %chosen, align 16
  store <4 x i32> %value, <4 x i32> addrspace(1)* %out, align 16
  ret void
}

;--- nominal-mismatch.ll
target triple = "air64-apple-macosx14.0.0"
%Left = type { [8 x <4 x float>] }
%Right = type { [8 x <4 x float>] }
define cc 76 void @nominal_mismatch(%Left addrspace(2)* %left, %Right addrspace(2)* %right, i1 %condition, float addrspace(1)* %out) {
  %a = bitcast %Left addrspace(2)* %left to <4 x i32> addrspace(2)*
  %b = bitcast %Right addrspace(2)* %right to <4 x i32> addrspace(2)*
  %chosen = select i1 %condition, <4 x i32> addrspace(2)* %a, <4 x i32> addrspace(2)* %b
  %restored = bitcast <4 x i32> addrspace(2)* %chosen to %Left addrspace(2)*
  %lane = getelementptr %Left, %Left addrspace(2)* %restored, i64 0, i32 0, i64 0, i64 1
  %value = load float, float addrspace(2)* %lane, align 4
  store float %value, float addrspace(1)* %out, align 4
  ret void
}

;--- unanchored-cycle.ll
target triple = "air64-apple-macosx14.0.0"
%Buffer = type { [8 x <4 x float>] }
; The merge has a valid root, but its unreachable cyclic arm has no native
; provenance. Finding one anchored arm must not bless the entire component.
define cc 76 void @unanchored_cycle(%Buffer addrspace(2)* %buffer, i1 %condition, float addrspace(1)* %out) {
entry:
  %view = bitcast %Buffer addrspace(2)* %buffer to <4 x i32> addrspace(2)*
  br label %join
cycle:
  %self = phi <4 x i32> addrspace(2)* [ %self, %cycle ]
  br i1 %condition, label %cycle, label %join
join:
  %chosen = phi <4 x i32> addrspace(2)* [ %view, %entry ], [ %self, %cycle ]
  %restored = bitcast <4 x i32> addrspace(2)* %chosen to %Buffer addrspace(2)*
  %lane = getelementptr %Buffer, %Buffer addrspace(2)* %restored, i64 0, i32 0, i64 0, i64 1
  %value = load float, float addrspace(2)* %lane, align 4
  store float %value, float addrspace(1)* %out, align 4
  ret void
}

;--- regroup-lanes.ll
target triple = "air64-apple-macosx14.0.0"
%Buffer = type { [8 x <4 x float>] }
define cc 76 void @regroup_lanes(%Buffer addrspace(2)* %buffer, <2 x i64> addrspace(1)* %out) {
  %view = bitcast %Buffer addrspace(2)* %buffer to <2 x i64> addrspace(2)*
  %element = getelementptr <2 x i64>, <2 x i64> addrspace(2)* %view, i64 1
  %value = load <2 x i64>, <2 x i64> addrspace(2)* %element, align 16
  store <2 x i64> %value, <2 x i64> addrspace(1)* %out, align 16
  ret void
}
