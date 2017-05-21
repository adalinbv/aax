
#include <stdio.h>
#include <base/memory.h>

int main()
{
   static const hw = "hElLo woRLD";
   char buf[4096];
   char *res;
   int i;

   for (i=0; i<4096; ++i) {
      buf[i] = 1+(i%200);
   }

   res = strnstr(buf, "Hello World", 4096);
   res = strncasestr(buf, "Hello World", 4096);

   res = buf+3000;
   memcpy(res, hw, 11);

   res = strnstr(buf, "hElLo woRLD", 4096);
   if (!res) printf("case sensitive text not found\n");

   res = strncasestr(buf, "Hello World", 4096);
   if (!res) printf("case insensitive text not found\n");
}

