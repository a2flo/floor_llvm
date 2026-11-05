// RUN: %clang_cc1 -triple air64-apple-macosx14.0.0 -x metal -std=metal3.2 -emit-llvm -disable-llvm-passes -o - %s | FileCheck %s

#define DEVICE __attribute__((global_as))
#define CONSTANT __attribute__((constant_as))
#define KERNEL __attribute__((compute_kernel, kernel_dim(3)))
typedef float float3 __attribute__((ext_vector_type(3)));

struct Leaf {
  float3 direction;
  unsigned code;
};
struct Packet {
  unsigned tag;
  Leaf leaves[2];
  float3 values[2];
  unsigned tail;
};
static_assert(sizeof(float3) == 16 && alignof(float3) == 16, "vector allocation");
static_assert(sizeof(Leaf) == 32 && alignof(Leaf) == 16, "nested stride");
static_assert(__builtin_offsetof(Leaf, code) == 16, "float3 tail padding");
static_assert(sizeof(Packet) == 128 && alignof(Packet) == 16, "buffer size");
static_assert(__builtin_offsetof(Packet, leaves) == 16, "nested alignment");
static_assert(__builtin_offsetof(Packet, values) == 80, "nested array stride");
static_assert(__builtin_offsetof(Packet, tail) == 112, "vector array stride");

KERNEL void layout(CONSTANT Packet &packet [[buffer(5)]],
                   DEVICE float *out [[buffer(0)]]) {
  out[0] = packet.leaves[1].direction.x + packet.values[1].y +
           float(packet.tag + packet.leaves[0].code + packet.tail);
}

// Sparse resource IDs also change the actual C++ record layout, not only the
// resource-index metadata. These are ordinary pointer slots, with no SDK types.
struct Resources {
  DEVICE int *first [[id(2)]];
  CONSTANT int *last [[id(7)]];
};
static_assert(sizeof(Resources) == 64 && alignof(Resources) == 8, "resource table");
static_assert(__builtin_offsetof(Resources, first) == 16, "first resource ID");
static_assert(__builtin_offsetof(Resources, last) == 56, "sparse resource ID");
KERNEL void resources(CONSTANT Resources &table [[buffer(9)]]) {
  table.first[0] = table.last[0];
}

// The frontend layout must already use the padded allocation size of float3.
// CHECK: target datalayout = "{{.*}}-v96:128:128-{{.*}}"
// CHECK: %struct.Packet = type { i32, [2 x %struct.Leaf], [2 x <3 x float>], i32, [12 x i8] }
// CHECK: %struct.Leaf = type { <3 x float>, i32, [12 x i8] }
// CHECK: %struct.Resources = type { [16 x i8], i32 addrspace(1)*, [32 x i8], i32 addrspace(2)* }
// CHECK-LABEL: define {{.*}} @layout(
// CHECK: getelementptr inbounds [2 x %struct.Leaf], [2 x %struct.Leaf] addrspace(2)* {{.*}}, i{{32|64}} 0, i{{32|64}} 1
// CHECK: getelementptr inbounds [2 x <3 x float>], [2 x <3 x float>] addrspace(2)* {{.*}}, i{{32|64}} 0, i{{32|64}} 1
// CHECK-LABEL: define {{.*}} @resources(
// CHECK: getelementptr inbounds %struct.Resources, %struct.Resources addrspace(2)* {{.*}}, i32 0, i32 3
// CHECK: getelementptr inbounds %struct.Resources, %struct.Resources addrspace(2)* {{.*}}, i32 0, i32 1
// CHECK-DAG: !{i32 0, !"air.buffer", !"air.buffer_size", i32 128, !"air.location_index", i32 5, i32 1, !"air.read", !"air.address_space", i32 2, !"air.struct_type_info", ![[PACKET:[0-9]+]], !"air.arg_type_size", i32 128, !"air.arg_type_align_size", i32 16, !"air.arg_type_name", !"Packet", !"air.arg_name", !"packet"}
// CHECK-DAG: ![[PACKET]] = !{i32 0, i32 4, i32 0, !"uint", !"tag", !"air.struct_type_info", ![[LEAF:[0-9]+]], i32 16, i32 32, i32 2, !"Leaf", !"leaves", i32 80, i32 16, i32 2, !"float3", !"values", i32 112, i32 4, i32 0, !"uint", !"tail"}
// CHECK-DAG: ![[LEAF]] = !{i32 0, i32 16, i32 0, !"float3", !"direction", i32 16, i32 4, i32 0, !"uint", !"code"}
// CHECK-DAG: !"air.indirect_buffer", !"air.buffer_size", i32 64, !"air.location_index", i32 9, i32 1
// CHECK-DAG: !"air.location_index", i32 2, i32 1, !"air.read_write", !"air.address_space", i32 1
// CHECK-DAG: !"air.location_index", i32 7, i32 1, !"air.read", !"air.address_space", i32 2
