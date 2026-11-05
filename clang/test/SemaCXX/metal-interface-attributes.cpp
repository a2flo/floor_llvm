// RUN: %clang_cc1 -triple air64-apple-macosx14.0.0 -x metal -std=metal3.2 -Werror=unknown-attributes -fsyntax-only -verify %s
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -x c++ -std=c++17 -DNOT_METAL -fsyntax-only -verify=host %s

#ifdef NOT_METAL
int threadgroup;
void host_buffer(int *p [[buffer(0)]]); // host-error {{buffer attribute requires Metal language mode}}
struct HostInput {};
void host_stage(HostInput p [[stage_in]]); // host-error {{stage_in requires Metal language mode}}
int host_constant [[function_constant(0)]]; // host-error {{function_constant attribute requires Metal language mode}}
#else

#define DEVICE __attribute__((global_as))
#define CONSTANT __attribute__((constant_as))
#define KERNEL __attribute__((compute_kernel, kernel_dim(3)))
typedef float float4 __attribute__((ext_vector_type(4)));

// Buffer and threadgroup bindings are separate namespaces. Binding is a
// property of the parameter, not a property cached on its pointer type.
KERNEL void valid(DEVICE int *out [[buffer(0)]],
                  CONSTANT int *in [[buffer(30)]],
                  threadgroup int *scratch [[threadgroup(0)]]) {
  scratch[0] = in[0];
  out[0] = scratch[0];
}
KERNEL void cached_type(threadgroup int *bound [[threadgroup(1)]],
                        threadgroup int *unbound); // expected-error {{pointer arguments to kernel functions must reside in}}

void buffer_wrong_type(int *p [[buffer(0)]]); // expected-error {{buffer attribute requires a device or constant pointer or reference}}
void buffer_range(DEVICE int *p [[buffer(31)]]); // expected-error {{Metal buffer index must be in range 0 through 30}}
void buffer_duplicate(DEVICE int *p [[buffer(0), buffer(1)]]); // expected-error {{duplicate buffer attribute on parameter}}
void group_wrong_type(DEVICE int *p [[threadgroup(0)]]); // expected-error {{threadgroup resource attribute requires a threadgroup pointer or reference}}
void group_range(threadgroup int *p [[threadgroup(31)]]); // expected-error {{threadgroup resource index is outside the supported range 0 through 30}}
void group_not_kernel(threadgroup int *p [[threadgroup(0)]]); // expected-error {{threadgroup resource bindings require a compute kernel}}

struct ValidInput {
  float4 position [[attribute(0)]];
  float4 color [[attribute(30)]];
};
struct InvalidInput {
  float4 range [[attribute(31)]]; // expected-error {{invalid or conflicting Metal vertex attribute}}
  bool type [[attribute(0)]]; // expected-error {{Metal stage IO requires a float, half, int or uint scalar/vector field}}
  float4 mixed [[attribute(1), user(locn1)]]; // expected-error {{user attribute requires a distinct locnN semantic}}
  float4 leading_zero [[user(locn01)]]; // expected-error {{user attribute requires a distinct locnN semantic}}
  float4 interpolation [[user(locn2), center_perspective, flat]]; // expected-error {{conflicting Metal interpolation attributes}}
};

CONSTANT bool enabled [[function_constant(7)]];
CONSTANT int count [[function_constant(65535)]];
CONSTANT bool duplicate [[function_constant(7)]]; // expected-error {{duplicate function_constant index}}
CONSTANT bool too_large [[function_constant(65536)]]; // expected-error {{function_constant index must be in range 0 through 65535}}
CONSTANT float unsupported [[function_constant(8)]]; // expected-error {{function_constant requires a non-volatile constant bool or int scalar}}
CONSTANT bool initialized [[function_constant(9)]] = true; // expected-error {{function_constant initializers are not supported}}

struct ConditionalColor {
  float4 value [[color(0), function_constant(enabled)]];
};
struct BadConditionalColor {
  float4 expression [[color(0), function_constant(!enabled)]]; // expected-error {{conditional function_constant requires a direct bool function constant}}
};

#endif
