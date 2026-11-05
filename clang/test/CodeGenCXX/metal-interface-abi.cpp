// RUN: %clang_cc1 -triple air64-apple-macosx14.0.0 -x metal -std=metal3.2 -Werror=unknown-attributes -emit-llvm -disable-llvm-passes -o - %s | FileCheck %s

#define DEVICE __attribute__((global_as))
#define CONSTANT __attribute__((constant_as))
#define KERNEL __attribute__((compute_kernel, kernel_dim(3)))
#define VERTEX __attribute__((vertex_shader))
#define FRAGMENT __attribute__((fragment_shader))
typedef float float4 __attribute__((ext_vector_type(4)));

struct Input {
  float4 value [[attribute(4)]];
};

// A record's stage attributes do not turn explicitly bound storage into stage
// input. Device and threadgroup binding 2 must not collide with each other.
KERNEL void stage_buffers(Input input [[stage_in]],
                          CONSTANT Input *source [[buffer(7)]],
                          DEVICE Input *output [[buffer(2)]],
                          threadgroup Input *scratch [[threadgroup(2)]],
                          unsigned lane [[thread_index_in_simdgroup]]) {
  scratch[lane].value = input.value + source[lane].value;
  output[lane].value = scratch[lane].value;
}

struct VertexOutput {
  float4 position [[position, invariant]];
  float4 value [[user(locn3)]];
};
VERTEX VertexOutput explicit_vertex(Input input [[stage_in]],
                                    unsigned id [[vertex_id]],
                                    unsigned instance [[instance_id]],
                                    unsigned base [[base_instance]]) {
  return {input.value, input.value + float(id + instance + base)};
}

struct FragmentInput {
  float4 value [[user(locn3), sample_no_perspective]];
};
FRAGMENT float4 explicit_fragment(FragmentInput input [[stage_in]],
                                   bool facing [[front_facing]]) {
  return facing ? input.value : -input.value;
}

// Explicit builtins replace their implicit counterparts. Check the raw ABI
// count before MetalFinal consumes it, as well as unmangled entry names.
// CHECK-LABEL: define {{.*}} @stage_buffers(
// CHECK-SAME: !floor.implicit_arg_count ![[KCOUNT:[0-9]+]]
// CHECK-LABEL: define {{.*}} @explicit_vertex(
// CHECK-SAME: !floor.implicit_arg_count ![[VCOUNT:[0-9]+]]
// CHECK-LABEL: define {{.*}} @explicit_fragment(
// CHECK-SAME: !floor.implicit_arg_count ![[VCOUNT]]
// CHECK-DAG: ![[KCOUNT]] = !{i32 9}
// CHECK-DAG: ![[VCOUNT]] = !{i32 1}
// CHECK-DAG: !{i32 0, !"air.stage_in", !"air.location_index", i32 4, i32 1, !"air.arg_type_name", !"float4", !"air.arg_name", !"value"}
// CHECK-DAG: !{i32 0, !"air.vertex_input", !"air.location_index", i32 4, i32 1, !"air.arg_type_name", !"float4", !"air.arg_name", !"value"}
// CHECK-DAG: !{i32 1, !"air.buffer", !"air.location_index", i32 7, i32 1, !"air.read", !"air.address_space", i32 2, {{.*}}!"air.arg_name", !"source"}
// CHECK-DAG: !{i32 2, !"air.buffer", !"air.location_index", i32 2, i32 1, !"air.read_write", !"air.address_space", i32 1, {{.*}}!"air.arg_name", !"output"}
// CHECK-DAG: !{i32 3, !"air.buffer", !"air.location_index", i32 2, i32 1, !"air.read_write", !"air.address_space", i32 3, {{.*}}!"air.arg_name", !"scratch"}
// CHECK-DAG: !{i32 4, !"air.thread_index_in_simdgroup", !"air.arg_type_name", !"uint", !"air.arg_name", !"lane"}
// CHECK-DAG: !{i32 1, !"air.vertex_id", !"air.arg_type_name", !"uint", !"air.arg_name", !"id"}
// CHECK-DAG: !{i32 2, !"air.instance_id", !"air.arg_type_name", !"uint", !"air.arg_name", !"instance"}
// CHECK-DAG: !{i32 3, !"air.base_instance", !"air.arg_type_name", !"uint", !"air.arg_name", !"base"}
// CHECK-DAG: !"air.position", !"air.invariant"
// CHECK-DAG: !{i32 0, !"air.fragment_input", !"user(locn3)", !"air.sample", !"air.no_perspective", !"air.arg_type_name", !"float4", !"air.arg_name", !"value"}
// CHECK-DAG: !{i32 1, !"air.front_facing", !"air.arg_type_name", !"bool", !"air.arg_name", !"facing"}
