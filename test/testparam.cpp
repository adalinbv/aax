
#include <cstdio>
#include <cstring>

#include <aax/aeonwave.hpp>

namespace aax = aeonwave;

int main()
{
   aax::Param p = 3.1415927536f;
   float v = 3.1415927536f;

   if (p != v) printf("Error: %f != %f (should be equal)\n", float(p), v);

   p += 1.0f;
   if (p == v) printf("%f == %f (should be different)\n", float(p), v);
   if (p != v) {} else printf("not %f != %f (should be different)\n", float(p), v);
   if (p < v) printf("%f < %f (should be greater)\n", float(p), v);
   if (p > v) {} else printf("not %f > %f (should be greater)\n", float(p), v);

   v += 1.0f;
   if (p != v) printf("inc: %f != %f (should be equal)\n", float(p), v);

   p *= 7.5;
   v *= 7.5f;
   if (p != v) printf("mul: %f != %f (should be equal)\n", float(p), v);

   p /= 0.33f;
   v /= 0.33f;
   if (p != v) printf("div: %f != %f (should be equal)\n", float(p), v);

   p -= 1e3f;
   v -= 1e3f;
   if (p != v) printf("sub: %f != %f (should be equal)\n", float(p), v);

   p += p;
   v += v;

   aax::Param t = v;
   if (p != t) printf("Param add: %f != %f (should be equal)\n", float(p), float(t));
   if (p == t) {} else printf("Param add: not %f == %f (should be equal)\n", float(p), float(t));
   if (p < t) printf("Param %f < %f (should be equal)\n", float(p), float(t));
   p -= 0.5f;
   if (p > t) printf("Param add: %f > %f (should be less)\n", float(p), float(t));
   

   return 0;
}
