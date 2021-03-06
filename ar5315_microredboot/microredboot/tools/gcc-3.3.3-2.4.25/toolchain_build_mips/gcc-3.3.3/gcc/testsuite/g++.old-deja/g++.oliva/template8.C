// Build don't link:

// Copyright (C) 1999 Free Software Foundation

// by Alexandre Oliva <oliva@dcc.unicamp.br>
// simplified from bug report by redleaf <e1wwater@dingo.cc.uq.edu.au>

struct B {
  template <class> void bar();
} b;

template <class T> void foo() {
  b.bar<T>(); // no longer gets bogus error - bar undeclared
  b.template bar<T>(); // no longer gets bogus error - ditto
  b.B::bar<T>(); // ok
}

template void foo<void>(); // no longer gets bogus error
