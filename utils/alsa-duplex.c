/*
 * http://www.mail-archive.com/alsa-devel@lists.sourceforge.net/msg00959.html
 * 1) initial write: the ring buffer should contain data for two periods
 *   before the streams are started
 * 2) xrun recovery: if you call snd_pcm_prepare, you have to call
 *    snd_pcm_start, too; otherwise the state will be SND_PCM_STATE_PREPARED
 * 3) count of written / read frames is invalid for i/o operations (you're
 *    using count in bytes)
 * 4) poll is not synchronized as you think; it's the main problem causing
 *    scratchy sound; you must always preserve the order of read/write i/o
 *    calls; please, look to 'r' variable and for "unsync" messages in my
 *    code
 */

#include <stdio.h>
#include <alsa/asoundlib.h>
#include <string.h>
#include <sched.h>

#define OK 0
#define FAIL 1

char *audiobuf = NULL;
int audiobuf_bytes = 0;

snd_pcm_t *cards[2] = { NULL };

int fdcount = 0;
struct pollfd *ufds = NULL;

int
setup_device (int index, char *device, int read)
{
  int err;
  snd_pcm_t *handle;
  snd_pcm_hw_params_t *params;       /* Hardware parameters   */
  snd_pcm_sw_params_t *swparams;     /* Software parameters   */

  int period_size;              /* Number of frames in period */
  int period_bytes;             /* size of period */
  int buffer_size;              /* size of entire audio buffer */
  unsigned int rate;
  snd_pcm_uframes_t start_threshold, stop_threshold;
  int count;

  /* Open output sound card */
  if (err = snd_pcm_open (&handle, device,
                    read ? SND_PCM_STREAM_PLAYBACK : SND_PCM_STREAM_CAPTURE,
                    SND_PCM_NONBLOCK) < 0)
    {
      fprintf (stderr, "snd_pcm_open failed: %s\n", snd_strerror (err));
      return (FAIL);
    }

  cards[index] = handle;

  snd_pcm_hw_params_alloca (&params);
  snd_pcm_sw_params_alloca (&swparams);
  snd_pcm_hw_params_any (handle, params);

  snd_pcm_hw_params_set_access(handle, params,
                               SND_PCM_ACCESS_RW_INTERLEAVED);

  snd_pcm_hw_params_set_format (handle, params,
                                SND_PCM_FORMAT_S16_LE);
  snd_pcm_hw_params_set_channels (handle, params, 1);

  rate = 44100;
  snd_pcm_hw_params_set_rate_near (handle, params, &rate, 0);

  period_size = 384;

  if (snd_pcm_hw_params_set_period_size (handle, params, period_size, 0) < 0)
    {
      fprintf (stderr, "Failed to set period size!\n");
      return (FAIL);
    }

  buffer_size = period_size * 2;
  if (snd_pcm_hw_params_set_buffer_size (handle, params, buffer_size) < 0)
    {
      fprintf (stderr, "Failed to set buffer size!\n");
      return (FAIL);
    }

  err = snd_pcm_hw_params (handle, params);

  /* set software parameters */
  snd_pcm_sw_params_current (handle, swparams);

  snd_pcm_sw_params_set_sleep_min (handle, swparams, 0);

//  snd_pcm_sw_params_set_avail_min (handle, swparams, period_size);

//  start_threshold = (double) rate * (1 / 1000000);
  start_threshold = 0x7fffffff;
  snd_pcm_sw_params_set_start_threshold(handle, swparams, start_threshold);

  stop_threshold = buffer_size;
  snd_pcm_sw_params_set_stop_threshold(handle, swparams, stop_threshold);

  snd_pcm_sw_params (handle, swparams);

  /* Prepare our buffer */
  period_bytes = period_size * 2; /* 2 bytes for 16 bit */

  if (audiobuf_bytes == 0)
    {
      audiobuf = malloc (period_bytes);
      audiobuf_bytes = period_bytes;
    }
  else if (audiobuf_bytes != period_bytes)
    {
      fprintf (stderr, "audio buffers not same size (%d != %d)!\n",
               audiobuf_bytes, period_bytes);
      return (FAIL);
    }

  count = snd_pcm_poll_descriptors_count (handle);
  if (count <= 0)
    {
      fprintf (stderr, "invalid poll descriptors count!\n");
      return (FAIL);
    }

  if (fdcount == 0)
    ufds = malloc (sizeof (struct pollfd) * count);
  else ufds = realloc (ufds, sizeof (struct pollfd) * (fdcount + count));

  snd_pcm_poll_descriptors (handle, &ufds[fdcount], count);

  fdcount += count;

  return (OK);
}

void
xrun_recovery()
{
  int err;

  fprintf(stderr, "XRUN recovery\n");
  if (err = snd_pcm_prepare(cards[0]) < 0) {
    fprintf (stderr, "Failed to prepare devices\n");
    exit (1);
  }
  if (err = snd_pcm_writei (cards[0], audiobuf, audiobuf_bytes) < 0) {
    fprintf (stderr, "Failed to write initial bytes\n");
    exit (1);
  }
  snd_pcm_start(cards[1]);
}

int
main (void)
{
  int i, r = 0;
  short revents;
  int err;
  struct sched_param schp;

  memset (&schp, 0, sizeof (schp));
  schp.sched_priority = sched_get_priority_max (SCHED_FIFO);

  if (sched_setscheduler (0, SCHED_FIFO, &schp) != 0)
    fprintf (stderr, "Failed to run SCHED_FIFO\n");

  /* Open input sound card */
  if (setup_device (1, "plughw:0,0", 0) != OK)
    {
      fprintf (stderr, "Failed to setup input device\n");
      exit (1);
    }

  /* Open output sound card */
  if (setup_device (0, "plughw:0,0", 1) != OK)
    {
      fprintf (stderr, "Failed to setup output device\n");
      exit (1);
    }

  if (snd_pcm_link (cards[1], cards[0]) < 0)
    {
      fprintf (stderr, "Failed to link devices\n");
      exit (1);
    }

  if (err = snd_pcm_prepare (cards[0]) < 0) {
      fprintf (stderr, "Failed to prepare devices\n");
      exit (1);
  }

  if (err = snd_pcm_writei (cards[0], audiobuf, audiobuf_bytes) < 0) {
      fprintf (stderr, "Failed to write initial bytes\n");
      exit (1);
  }

  snd_pcm_start (cards[1]);

  do
    {
      poll (ufds, fdcount, -1);

      for (i=0; i < fdcount; i++)
        {
          revents = ufds[i].revents;
          if (revents & POLLERR)
            {
              snd_pcm_t *handle;
              snd_pcm_state_t state;

              if (revents & POLLOUT) handle = cards[0];
              else handle = cards[1];

              switch ((state = snd_pcm_state (handle)))
                {
                case SND_PCM_STATE_XRUN:
                  fprintf (stderr, "XRUN on %s\n",
                           (revents & POLLOUT != 0) ? "output" : "input");
                  xrun_recovery();
                  r = 0;
                  break;
                default:
                  fprintf (stderr, "Bad state %d on %s\n", state,
                           (revents & POLLOUT != 0) ? "output" : "input");
                }
            }
          else if (revents & POLLIN)
            {
              if (r != 0) {
                printf("unsync: POLLIN\n");
                continue;
              }
              if ((err = snd_pcm_readi (cards[1], audiobuf, audiobuf_bytes)) < 0)
                {
                  if (err == -EPIPE)
                    {
                      fprintf (stderr, "readi XRUN on input\n");
                    }
                }
               else
              if (err > 0 && err != audiobuf_bytes)
                fprintf (stderr, "read error: requested %i read %i\n", audiobuf_bytes, 
err);
              printf("POLLIN: res = %i\n", err);
              r = 1;
            }
          else if (revents & POLLOUT)
            {
              if (r != 1) {
                printf("unsync: POLLOUT\n");
                continue;
              }
              if ((err = snd_pcm_writei (cards[0], audiobuf,
                                        audiobuf_bytes)) < 0)
                {
                  if (err == -EPIPE)
                    {
                      fprintf (stderr, "writei XRUN on output\n");
                    }
                }
               else
              if (err > 0 && err != audiobuf_bytes)
                fprintf (stderr, "write error: requested %i written %i\n", 
audiobuf_bytes, err);
              printf("POLLOUT: res = %i\n", err);
              r = 0;
            }
        }
    }
  while (1);

  exit (0);
}

