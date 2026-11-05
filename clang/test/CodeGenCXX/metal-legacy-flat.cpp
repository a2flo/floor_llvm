// RUN: %clang_cc1 -triple air64-apple-macosx14.0.0 -x metal -std=metal3.2 -Werror=unknown-attributes -emit-llvm -disable-llvm-passes -o - %s | FileCheck %s

// flat and stage_input both predate the explicit MSL interface extension.
// Neither spelling of flat should opt this record into stage_in validation.
typedef float float4 __attribute__((ext_vector_type(4)));
struct LegacyInput {
  float first [[flat]];
  float second __attribute__((flat));
};
__attribute__((fragment_shader))
float4 legacy_fragment(LegacyInput input [[stage_input]]) {
  return float4(input.first + input.second);
}
// Legacy flat also applies to an aggregate field, not only native scalars.
struct LegacyNested { float value; };
struct LegacyAggregate { LegacyNested value [[flat]]; };

struct LegacyOutput {
  float4 position [[position]];
  float value [[flat]];
};
__attribute__((vertex_shader)) LegacyOutput legacy_vertex() {
  return {float4(0.0f), 1.0f};
}
// CHECK-DAG: define {{.*}} @legacy_fragment(
// CHECK-DAG: define {{.*}} @legacy_vertex(
// CHECK-DAG: !"air.fragment_input", {{.*}}!"air.flat", !"air.arg_type_name", !"float", !"air.arg_name", !"first"
// CHECK-DAG: !"air.fragment_input", {{.*}}!"air.flat", !"air.arg_type_name", !"float", !"air.arg_name", !"second"
