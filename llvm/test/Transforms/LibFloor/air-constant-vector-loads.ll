; RUN: opt -enable-new-pm=0 -PointerBCFixup -verify-each -S %s | FileCheck %s --implicit-check-not='bitcast {{.*}}addrspace(2)'
;
; Read-only AS2 pointer graphs must use the real storage type. Only loaded
; values may be reinterpreted. cc 76 is FLOOR_KERNEL in this toolchain.

target datalayout = "e-p:64:64-i64:64-v128:128-n8:16:32:64"
target triple = "air64-apple-macosx14.0.0"

%Buffer = type { [8 x <4 x float>] }
%Wrapper = type { %Buffer }
%Slot = type { %Buffer addrspace(2)* }

; Preserve dynamic signed indices, inbounds, alignment and memory metadata,
; but drop integer value metadata when replacing an integer load with float.
; CHECK-LABEL: @scalar_metadata(
; CHECK: %view.storage = getelementptr %Wrapper, %Wrapper addrspace(2)* %buffer, i32 0, i32 0, i32 0, i32 0
; CHECK: %element.typed = getelementptr inbounds <4 x float>, <4 x float> addrspace(2)* %view.storage, i64 %index, i64 %lane
; CHECK: %value.typed = load float, float addrspace(2)* %element.typed, align 4, !tbaa ![[TBAA:[0-9]+]], !invariant.load ![[INV:[0-9]+]], !nontemporal ![[NT:[0-9]+]]{{$}}
; CHECK-NEXT: %value.bits = bitcast float %value.typed to i32
; CHECK-NEXT: store i32 %value.bits, i32 addrspace(1)* %out, align 4
; CHECK-NEXT: ret void
define cc 76 void @scalar_metadata(%Wrapper addrspace(2)* %buffer, i64 %index, i64 %lane, i32 addrspace(1)* %out) {
  %view = bitcast %Wrapper addrspace(2)* %buffer to <4 x i32> addrspace(2)*
  %element = getelementptr inbounds <4 x i32>, <4 x i32> addrspace(2)* %view, i64 %index, i64 %lane
  %value = load i32, i32 addrspace(2)* %element, align 4, !tbaa !0, !range !3, !nontemporal !4, !invariant.load !5
  store i32 %value, i32 addrspace(1)* %out, align 4
  ret void
}

; A loop-carried pointer and a select must be rebuilt together, not just the
; root cast. The second root starts with native vector storage, not a wrapper.
; CHECK-LABEL: @loop_and_select(
; CHECK: %wrapped.storage = getelementptr %Buffer, %Buffer addrspace(2)* %buffer, i32 0, i32 0, i32 0
; CHECK: %initial.typed = select i1 %choose, <4 x float> addrspace(2)* %wrapped.storage, <4 x float> addrspace(2)* %native
; CHECK: loop:
; CHECK-NEXT: %cursor.typed = phi <4 x float> addrspace(2)* [ %initial.typed, %entry ], [ %next.typed, %loop ]
; CHECK: %value.typed = load <4 x float>, <4 x float> addrspace(2)* %cursor.typed, align 16
; CHECK-NEXT: %value.bits = bitcast <4 x float> %value.typed to <4 x i32>
; CHECK: %next.typed = getelementptr <4 x float>, <4 x float> addrspace(2)* %cursor.typed, i64 -1
; CHECK: store <4 x i32> %value.bits, <4 x i32> addrspace(1)* %out, align 16
define cc 76 void @loop_and_select(%Buffer addrspace(2)* %buffer, <4 x float> addrspace(2)* %native, i1 %choose, i1 %again, <4 x i32> addrspace(1)* %out) {
entry:
  %direct = bitcast <4 x float> addrspace(2)* %native to <4 x i32> addrspace(2)*
  %wrapped = bitcast %Buffer addrspace(2)* %buffer to <4 x i32> addrspace(2)*
  %initial = select i1 %choose, <4 x i32> addrspace(2)* %wrapped, <4 x i32> addrspace(2)* %direct
  br label %loop
loop:
  %cursor = phi <4 x i32> addrspace(2)* [ %initial, %entry ], [ %next, %loop ]
  %value = load <4 x i32>, <4 x i32> addrspace(2)* %cursor, align 16
  %next = getelementptr <4 x i32>, <4 x i32> addrspace(2)* %cursor, i64 -1
  br i1 %again, label %loop, label %exit
exit:
  store <4 x i32> %value, <4 x i32> addrspace(1)* %out, align 16
  ret void
}

; Recover a pointer slot only through its actual first field, and restore an
; exact inverse wrapper cast without changing the native aggregate indices.
; CHECK-LABEL: @pointer_slot(
; CHECK: %loaded.slot = getelementptr %Slot, %Slot addrspace(2)* %slot, i32 0, i32 0
; CHECK-NEXT: %loaded.pointer = load %Buffer addrspace(2)*, %Buffer addrspace(2)* addrspace(2)* %loaded.slot, align 8, !nonnull ![[INV]], !dereferenceable ![[DEREF:[0-9]+]]
; CHECK: %loaded.storage = getelementptr %Buffer, %Buffer addrspace(2)* %loaded.pointer, i32 0, i32 0, i32 0
; CHECK: %element = getelementptr inbounds %Buffer, %Buffer addrspace(2)* %loaded.pointer, i64 0, i32 0, i64 %index, i64 2
; CHECK-NEXT: %native.value = load float, float addrspace(2)* %element, align 4
; CHECK: %value.typed = load <4 x float>, <4 x float> addrspace(2)* %loaded.storage, align 16
; CHECK-NEXT: %value.bits = bitcast <4 x float> %value.typed to <4 x i32>
; CHECK: store <4 x i32> %value.bits, <4 x i32> addrspace(1)* %out, align 16
define cc 76 void @pointer_slot(%Slot addrspace(2)* %slot, i64 %index, <4 x i32> addrspace(1)* %out, float addrspace(1)* %scalar.out) {
  %slot.view = bitcast %Slot addrspace(2)* %slot to <4 x i32> addrspace(2)* addrspace(2)*
  %loaded = load <4 x i32> addrspace(2)*, <4 x i32> addrspace(2)* addrspace(2)* %slot.view, align 8, !nonnull !5, !dereferenceable !6
  %restored = bitcast <4 x i32> addrspace(2)* %loaded to %Buffer addrspace(2)*
  %element = getelementptr inbounds %Buffer, %Buffer addrspace(2)* %restored, i64 0, i32 0, i64 %index, i64 2
  %native.value = load float, float addrspace(2)* %element, align 4
  %value = load <4 x i32>, <4 x i32> addrspace(2)* %loaded, align 16
  store <4 x i32> %value, <4 x i32> addrspace(1)* %out, align 16
  store float %native.value, float addrspace(1)* %scalar.out, align 4
  ret void
}

; Nominally identical wrapper roots can also be restored after a merge.
; CHECK-LABEL: @inverse_select(
; CHECK: %chosen.native = select i1 %condition, %Buffer addrspace(2)* %left, %Buffer addrspace(2)* %right
; CHECK: %element = getelementptr %Buffer, %Buffer addrspace(2)* %chosen.native, i64 0, i32 0, i64 %index, i64 3
; CHECK-NEXT: %value = load float, float addrspace(2)* %element, align 4
; CHECK-NEXT: store float %value, float addrspace(1)* %out, align 4
define cc 76 void @inverse_select(%Buffer addrspace(2)* %left, %Buffer addrspace(2)* %right, i1 %condition, i64 %index, float addrspace(1)* %out) {
  %a = bitcast %Buffer addrspace(2)* %left to <4 x i32> addrspace(2)*
  %b = bitcast %Buffer addrspace(2)* %right to <4 x i32> addrspace(2)*
  %chosen = select i1 %condition, <4 x i32> addrspace(2)* %a, <4 x i32> addrspace(2)* %b
  %restored = bitcast <4 x i32> addrspace(2)* %chosen to %Buffer addrspace(2)*
  %element = getelementptr %Buffer, %Buffer addrspace(2)* %restored, i64 0, i32 0, i64 %index, i64 3
  %value = load float, float addrspace(2)* %element, align 4
  store float %value, float addrspace(1)* %out, align 4
  ret void
}

; Canonical inverse GEPs retain their nonzero/dynamic indices. They must not
; be mistaken for an inverse cast to the zero-offset enclosing wrapper.
; CHECK-LABEL: @canonical_inverse_geps(
; CHECK: %vector.typed = getelementptr inbounds <4 x float>, <4 x float> addrspace(2)* %view.storage, i64 %index
; CHECK: %vector.value = load <4 x float>, <4 x float> addrspace(2)* %vector.typed, align 16
; CHECK: %lane.typed = getelementptr <4 x float>, <4 x float> addrspace(2)* %vector.typed, i64 -1, i64 2
; CHECK: %scalar.value = load float, float addrspace(2)* %lane.typed, align 4
define cc 76 void @canonical_inverse_geps(%Buffer addrspace(2)* %buffer, i64 %index, <4 x float> addrspace(1)* %out, float addrspace(1)* %scalar.out) {
  %view = bitcast %Buffer addrspace(2)* %buffer to <4 x i32> addrspace(2)*
  %vector = getelementptr inbounds <4 x i32>, <4 x i32> addrspace(2)* %view, i64 %index
  %native.vector = bitcast <4 x i32> addrspace(2)* %vector to <4 x float> addrspace(2)*
  %vector.value = load <4 x float>, <4 x float> addrspace(2)* %native.vector, align 16
  %lane = getelementptr <4 x i32>, <4 x i32> addrspace(2)* %vector, i64 -1, i64 2
  %native.scalar = bitcast i32 addrspace(2)* %lane to float addrspace(2)*
  %scalar.value = load float, float addrspace(2)* %native.scalar, align 4
  store <4 x float> %vector.value, <4 x float> addrspace(1)* %out, align 16
  store float %scalar.value, float addrspace(1)* %scalar.out, align 4
  ret void
}

!0 = !{!1, !1, i64 0}
!1 = !{!"omnipotent char", !2, i64 0}
!2 = !{!"Simple C/C++ TBAA"}
!3 = !{i32 0, i32 16}
!4 = !{i32 1}
!5 = !{}
!6 = !{i64 128}
