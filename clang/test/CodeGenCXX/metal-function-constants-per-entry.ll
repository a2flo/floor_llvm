; RUN: %clang_cc1 -triple air64-apple-macosx14.0.0 -x ir -disable-llvm-passes -emit-metallib -o %t.metallib %s
; RUN: %python %S/Inputs/metal-function-constants-per-entry.py %t.metallib used
;
; Each entry inherits llvm.global_ctors from the source module. Only the first
; entry uses @used, and neither entry uses @unused. Its constructor must not
; make either constant appear in the second entry's CNST tag.

target triple = "air64-apple-macosx14.0.0"

@used = internal addrspace(2) global i32 undef
@unused = internal addrspace(2) global i32 undef
@used_pointer = internal constant i32 addrspace(2)* @used
@used.MTL_FC_INIT_7_i = internal addrspace(2) externally_initialized constant i32 undef, section "air.fc_initializer"
@unused.MTL_FC_INIT_8_i = internal addrspace(2) externally_initialized constant i32 undef, section "air.fc_initializer"
@llvm.global_ctors = appending global [2 x { i32, void ()*, i8* }] [
  { i32, void ()*, i8* } { i32 65535, void ()* @__metal_fc_init_7, i8* null },
  { i32, void ()*, i8* } { i32 65535, void ()* @__metal_fc_init_8, i8* null }
]

define internal void @__metal_fc_init_7() section "air.static_init" {
  %value = load i32, i32 addrspace(2)* @used.MTL_FC_INIT_7_i
  store i32 %value, i32 addrspace(2)* @used
  ret void
}

define internal void @__metal_fc_init_8() section "air.static_init" {
  %value = load i32, i32 addrspace(2)* @unused.MTL_FC_INIT_8_i
  store i32 %value, i32 addrspace(2)* @unused
  ret void
}

define internal void @read_constant() {
  %pointer = load i32 addrspace(2)*, i32 addrspace(2)** @used_pointer
  %value = load volatile i32, i32 addrspace(2)* %pointer
  ret void
}

define void @entry_using_constant() {
  call void @read_constant()
  ret void
}

define void @entry_without_constant() {
  ret void
}

!air.kernel = !{!0, !1}
!air.function_constants = !{!3, !4}
!0 = !{void ()* @entry_using_constant, !2, !2}
!1 = !{void ()* @entry_without_constant, !2, !2}
!2 = !{}
!3 = !{i32 addrspace(2)* @used.MTL_FC_INIT_7_i, !"int", !"used", i32 7, i1 true}
!4 = !{i32 addrspace(2)* @unused.MTL_FC_INIT_8_i, !"int", !"unused", i32 8, i1 true}
