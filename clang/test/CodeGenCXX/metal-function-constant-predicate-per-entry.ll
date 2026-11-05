; RUN: %clang_cc1 -triple air64-apple-macosx14.0.0 -x ir -disable-llvm-passes -emit-metallib -o %t.metallib %s
; RUN: %python %S/Inputs/metal-function-constants-per-entry.py %t.metallib enabled
;
; A required bool constant used only by an AIR conditional output still belongs
; to that fragment. Its predicate constructor and llvm.compiler.used entry must
; not keep it in the other fragment's per-entry library.

target triple = "air64-apple-macosx14.0.0"

@enabled = internal addrspace(2) global i8 undef
@enabled.MTL_FC_INIT_7_b = internal addrspace(2) externally_initialized constant i8 undef, section "air.fc_initializer"
@__metal_implicit_fc_pred = internal addrspace(2) global i8 0
@llvm.compiler.used = appending global [1 x i8*] [i8* addrspacecast (i8 addrspace(2)* @__metal_implicit_fc_pred to i8*)], section "llvm.metadata"
@llvm.global_ctors = appending global [2 x { i32, void ()*, i8* }] [
  { i32, void ()*, i8* } { i32 65535, void ()* @__metal_fc_init_7, i8* null },
  { i32, void ()*, i8* } { i32 65535, void ()* @__metal_fc_pred_init, i8* null }
]

declare i8 @air.normalize_function_constant_predicate.i8(i8) readnone

define internal void @__metal_fc_init_7() section "air.static_init" {
  %value = load i8, i8 addrspace(2)* @enabled.MTL_FC_INIT_7_b
  store i8 %value, i8 addrspace(2)* @enabled
  ret void
}

define internal void @__metal_fc_pred_init() section "air.static_init" {
  %value = load i8, i8 addrspace(2)* @enabled.MTL_FC_INIT_7_b
  %normalized = call i8 @air.normalize_function_constant_predicate.i8(i8 %value)
  store i8 %normalized, i8 addrspace(2)* @__metal_implicit_fc_pred
  ret void
}

define <4 x float> @fragment_with_conditional_output() {
  ret <4 x float> zeroinitializer
}

define <4 x float> @fragment_without_constant() {
  ret <4 x float> zeroinitializer
}

!air.fragment = !{!0, !1}
!air.function_constants = !{!3}
!0 = !{<4 x float> ()* @fragment_with_conditional_output, !4, !2}
!1 = !{<4 x float> ()* @fragment_without_constant, !2, !2}
!2 = !{}
!3 = !{i8 addrspace(2)* @enabled.MTL_FC_INIT_7_b, !"bool", !"enabled", i32 7, i1 true}
!4 = !{!5}
!5 = !{!"air.function_constant", !6, !"air.render_target", i32 0, i32 0, !"air.arg_type_name", !"float4", !"air.arg_name", !"color"}
!6 = !{i8 addrspace(2)* @__metal_implicit_fc_pred, !"bool", !"enabled"}
