// RUN: %clang_cc1 -triple air64-apple-macosx14.0.0 -x metal -std=metal3.2 -emit-llvm -disable-llvm-passes -o - %s | FileCheck %s

#define DEVICE __attribute__((global_as))
#define CONSTANT __attribute__((constant_as))
#define THREAD __attribute__((private_as))

struct Box {
  int value;
  DEVICE int *device_address() DEVICE;
  const CONSTANT int *constant_address() const CONSTANT;
  threadgroup int *group_address() threadgroup;
  THREAD int *thread_address() THREAD;
};

// Each return value and implicit 'this' parameter must retain its own address
// space. A generic pointer plus a cast at the call site is not equivalent.
// CHECK-LABEL: define {{.*}}i32 addrspace(1)* {{.*}}device_address{{.*}}(%struct.Box addrspace(1)*
// CHECK: getelementptr inbounds %struct.Box, %struct.Box addrspace(1)*
// CHECK: ret i32 addrspace(1)*
DEVICE int *Box::device_address() DEVICE { return &value; }

// CHECK-LABEL: define {{.*}}i32 addrspace(2)* {{.*}}constant_address{{.*}}(%struct.Box addrspace(2)*
// CHECK: getelementptr inbounds %struct.Box, %struct.Box addrspace(2)*
// CHECK: ret i32 addrspace(2)*
const CONSTANT int *Box::constant_address() const CONSTANT { return &value; }

// CHECK-LABEL: define {{.*}}i32 addrspace(3)* {{.*}}group_address{{.*}}(%struct.Box addrspace(3)*
// CHECK: getelementptr inbounds %struct.Box, %struct.Box addrspace(3)*
// CHECK: ret i32 addrspace(3)*
threadgroup int *Box::group_address() threadgroup { return &value; }

// CHECK-LABEL: define {{.*}}i32* {{.*}}thread_address{{.*}}(%struct.Box*
// CHECK: getelementptr inbounds %struct.Box, %struct.Box*
// CHECK: ret i32*
THREAD int *Box::thread_address() THREAD { return &value; }
