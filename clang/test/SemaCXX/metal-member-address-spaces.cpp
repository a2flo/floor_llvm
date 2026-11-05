// RUN: %clang_cc1 -triple air64-apple-macosx14.0.0 -x metal -std=metal3.2 -fsyntax-only -verify %s

// The trailing qualifier is the address space of the implicit object, not of
// the return type. Exercise overload resolution, template instantiation, and
// out-of-line definitions without an SDK or compatibility-header dependency.
#define DEVICE __attribute__((global_as))
#define CONSTANT __attribute__((constant_as))
#define THREAD __attribute__((private_as))

template <class T> struct Array {
  T value;
  THREAD T &get() THREAD { return value; }
  DEVICE T &get() DEVICE { return value; }
  const CONSTANT T &get() const CONSTANT { return value; }
  threadgroup T &get() threadgroup { return value; }
};

struct Box {
  int value;
  DEVICE int *address() DEVICE;
};
DEVICE int *Box::address() DEVICE { return &value; }

void valid(DEVICE Array<int> &d, CONSTANT Array<int> &c,
           threadgroup Array<int> &g, DEVICE Box &b) {
  Array<int> t;
  THREAD int &tr = t.get();
  DEVICE int &dr = d.get();
  const CONSTANT int &cr = c.get();
  threadgroup int &gr = g.get();
  DEVICE int *p = b.address();
}

struct Restricted {
  int get() DEVICE; // expected-note {{candidate function not viable}}
  int get() const CONSTANT; // expected-note {{candidate function not viable}}
};

void wrong_object_space() {
  Restricted local;
  local.get(); // expected-error {{no matching member function for call to 'get'}}
}

struct Conflicting {
  void invalid() DEVICE THREAD; // expected-error {{multiple address spaces specified for type}}
};
