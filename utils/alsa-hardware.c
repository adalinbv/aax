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
      char *mode_str[2] = { "Playback", "Capture" };
      int mode[2] = { SND_PCM_STREAM_PLAYBACK, SND_PCM_STREAM_CAPTURE };

      printf("\nAvailable devices for %s:\n", mode_str[i]);

      card_idx = -1;
      while ((snd_card_next(&card_idx) == 0) && (card_idx >= 0))
      {
         char alsaname[256];
         int dev_idx;
         int subdev_idx;
         snd_ctl_t *ctl;
         char *cardname;

         snd_card_get_name(card_idx, &cardname);
         printf("card %d: %s\n", card_idx, cardname);

         dev_idx = -1;
         sprintf(alsaname, "hw:%d", card_idx);
         snd_ctl_open(&ctl, alsaname, 0);
         while ((snd_ctl_pcm_next_device(ctl, &dev_idx) == 0)
                && (dev_idx >=0))
         {
            char *devname;
            int subdev_cnt;

            snd_pcm_info_set_device(info, dev_idx);
            snd_pcm_info_set_stream(info, mode[i]);

            subdev_idx = 0;
            subdev_cnt = 0;
            snd_pcm_info_set_subdevice(info, subdev_idx);
            if (snd_ctl_pcm_info(ctl, info) >= 0)
            {
               subdev_cnt = snd_pcm_info_get_subdevices_count(info);
               devname = (char *)snd_pcm_info_get_name(info);
               printf("\tdev %d: %-40s", dev_idx, devname);
               printf("\tno. subdevices: %3i\n", subdev_cnt);
            }
         }
         snd_ctl_close(ctl);
      }
   }

   return 0;
}
