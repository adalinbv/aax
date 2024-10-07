
#include <aax/aeonwave>

int main()
{
   aeonwave::AeonWave aax;

   while (const char* d = aax.drivers())
   {
      printf("\ndriver: %s\n", d);
      while (const char* r = aax.devices(true))
      {
         printf("  device: '%s'\n", r);
         while (const char* i = aax.interfaces(true)) {
            printf("    interface: '%s'\n", i);
         }
      }
   }

   return 0;
}
