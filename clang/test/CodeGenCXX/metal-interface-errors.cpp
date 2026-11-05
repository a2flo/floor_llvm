// RUN: %clang_cc1 -triple air64-apple-macosx14.0.0 -x metal -std=metal3.2 -DCASE=1 -emit-llvm -disable-llvm-passes -o /dev/null -verify %s
// RUN: %clang_cc1 -triple air64-apple-macosx14.0.0 -x metal -std=metal3.2 -DCASE=2 -emit-llvm -disable-llvm-passes -o /dev/null -verify %s
// RUN: %clang_cc1 -triple air64-apple-macosx14.0.0 -x metal -std=metal3.2 -DCASE=3 -emit-llvm -disable-llvm-passes -o /dev/null -verify %s
// RUN: %clang_cc1 -triple air64-apple-macosx14.0.0 -x metal -std=metal3.2 -DCASE=4 -emit-llvm -disable-llvm-passes -o /dev/null -verify %s
// RUN: %clang_cc1 -triple air64-apple-macosx14.0.0 -x metal -std=metal3.2 -DCASE=5 -emit-llvm -disable-llvm-passes -o /dev/null -verify %s
// RUN: %clang_cc1 -triple air64-apple-macosx14.0.0 -x metal -std=metal3.2 -DCASE=6 -emit-llvm -disable-llvm-passes -o /dev/null -verify %s

#define DEVICE __attribute__((global_as))
#define CONSTANT __attribute__((constant_as))
#define KERNEL __attribute__((compute_kernel, kernel_dim(3)))
typedef float float4 __attribute__((ext_vector_type(4)));

// These restrictions are diagnosed when AIR entry metadata is generated, so a
// syntax-only test would not exercise them. Do not silently alias resources or
// drop interface semantics that Sema has already accepted.
#if CASE == 1
KERNEL void duplicate_buffers(DEVICE int *a [[buffer(2)]], DEVICE int *b [[buffer(2)]]) {} // expected-error {{duplicate or invalid Metal buffer binding}}
#elif CASE == 2
KERNEL void missing_binding(CONSTANT int *a [[buffer(7)]], DEVICE int *b) {} // expected-error {{all buffer parameters require explicit buffer attributes}}
#elif CASE == 3
KERNEL void duplicate_groups(threadgroup int *a [[threadgroup(2)]], threadgroup int *b [[threadgroup(2)]]) {} // expected-error {{missing, duplicate or invalid threadgroup resource binding}}
#elif CASE == 4
struct DuplicateInput {
  float4 first [[attribute(1)]];
  float4 second [[attribute(1)]]; // expected-error {{duplicate MSL stage-input semantic}}
};
KERNEL void duplicate_stage_input(DuplicateInput input [[stage_in]]) {}
#elif CASE == 5
struct Input {
  float4 value [[attribute(1)]];
};
struct NestedInput {
  Input nested; // expected-error {{unsupported or conflicting MSL stage-input field}}
};
KERNEL void nested_stage_input(NestedInput input [[stage_in]]) {}
#elif CASE == 6
// A legacy flat marker must not disable explicit MSL output validation.
struct ModernFlatOutput {
  float4 position [[position]];
  float value [[user(locn0), flat]]; // expected-error {{unsupported or conflicting MSL vertex-output semantic}}
};
__attribute__((vertex_shader)) ModernFlatOutput modern_flat_output() {
  return {float4(0.0f), 1.0f};
}
#endif
