
#include <stdio.h>

#include <aax/strings>

int main()
{
   for (enum aaxFilterType f = aaxFilterType(AAX_FILTER_NONE+1);
        f < aaxGetByType(AAX_MAX_FILTER); f = aaxFilterType(f+1))
   {
      std::string fs = aeonwave::to_string(f);
      printf("%s\n", fs.c_str());
   }

   for (enum aaxEffectType e = aaxEffectType(AAX_EFFECT_NONE+1);
        e < aaxGetByType(AAX_MAX_EFFECT); e = aaxEffectType(e+1))
   {
      std::string fs = aeonwave::to_string(e);
      printf("%s\n", fs.c_str());
   }

   return 0;
}
