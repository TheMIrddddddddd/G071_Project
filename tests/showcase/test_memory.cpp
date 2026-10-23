#include "smooth_memory.h"
#include <new>
#include <cassert>
#include <cstdio>
int main()
{
  bool caught=false;try{void *p=::operator new(4096);::operator delete(p);}catch(const std::bad_alloc&){caught=true;}
  assert(caught);
  void *a=::operator new(32),*b=::operator new(64);::operator delete(a);::operator delete(b);
  assert(SmoothMemory_GetStats().live_bytes==0);
  a=::operator new(1500);::operator delete(a);
  SmoothMemory_Seal();caught=false;
  try{a=::operator new(1);}catch(const std::bad_alloc&){caught=true;}
  assert(caught);std::puts("PASS bounded 2KB allocator, coalescing, seal and exception path");
}
