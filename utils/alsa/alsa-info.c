#include <stdio.h>
#include <alloca.h>

#include <alsa/asoundlib.h>

int main()
{
   snd_pcm_info_t *info;
   int card_idx, i;

   printf("ALSA version %s\n\n", snd_asoundlib_version());

   snd_pcm_info_alloca(&info);

   for (i=0; i<2; i++)
   {
      const char *_type = i ? "Output" : "Input";
      void **hints;
      int res;

      res= snd_device_name_hint(-1, "pcm", &hints);
      if (!res && hints)
      {
         void **lst = hints;
         while (*lst)
         {
            char *type = snd_device_name_get_hint(*lst, "IOID");
            if (!type || (type && !strcmp(type, _type)))
            {
               char *name = snd_device_name_get_hint(*lst, "NAME");
               if (name)
               {
                  char *ptr, *desc = snd_device_name_get_hint(*lst, "DESC");
                  if (!desc) desc = name;
                  ptr = strchr(desc, '\n');
                  if (ptr) *ptr = '|';
                  printf("%s: %s\n", _type ? _type : "I/O", name);
                  printf("  %s\n\n", desc);
                  free(desc);
               }
               free(name);
            }
            free(type);
            ++lst;
         }
      }
      res = snd_device_name_free_hint(hints);
   }

   return 0;
}
