/*
 * openal-info: Display information about ALC and AL.
 *
 * Idea based on glxinfo for OpenGL.
 * Initial OpenAL version by Erik Hofman <erik@ehofman.com>.
 * Further hacked by Sven Panne <sven.panne@aedion.de>.
 *
 */

#include <stdio.h>

#include <AL/alut.h>

#define EXIT_SUCCESS		0
#define EXIT_FAILURE		-1

static const int indentation = 4;
static const int maxmimumWidth = 79;

static void
printChar (int c, int *width)
{
  putchar (c);
  *width = (c == '\n') ? 0 : (*width + 1);
}

static void
indent (int *width)
{
  int i;
  for (i = 0; i < indentation; i++)
    {
      printChar (' ', width);
    }
}

static void
printExtensions (const char *header, char separator, const char *extensions)
{
  int width = 0, start = 0, end = 0;

  printf ("%s:\n", header);
  if (extensions == NULL || extensions[0] == '\0')
    {
      return;
    }

  indent (&width);
  while (1)
    {
      if (extensions[end] == separator || extensions[end] == '\0')
        {
          if (width + end - start + 2 > maxmimumWidth)
            {
              printChar ('\n', &width);
              indent (&width);
            }
          while (start < end)
            {
              printChar (extensions[start], &width);
              start++;
            }
          if (extensions[end] == '\0')
            {
              break;
            }
          start++;
          end++;
          if (extensions[end] == '\0')
            {
              break;
            }
          printChar (',', &width);
          printChar (' ', &width);
        }
      end++;
    }
  printChar ('\n', &width);
}

static void
die (const char *kind, const char *description)
{
  fprintf (stderr, "%s error %s occured\n", kind, description);
  exit (EXIT_FAILURE);
}

static void
checkForErrors ()
{
  {
    ALenum error = alutGetError ();
    if (error != ALUT_ERROR_NO_ERROR)
      {
        die ("ALUT", (const char *) alutGetErrorString (error));
      }
  }
  {
    ALCdevice *device = alcGetContextsDevice (alcGetCurrentContext ());
    ALCenum error = alcGetError (device);
    if (error != ALC_NO_ERROR)
      {
        die ("ALC", (const char *) alcGetString (device, error));
      }
  }
  {
    ALenum error = alGetError ();
    if (error != AL_NO_ERROR)
      {
        die ("AL", (const char *) alGetString (error));
      }
  }
}

static void
printALUTInfo ()
{
  ALint major, minor;
  const ALchar *s;

  major = alutGetMajorVersion ();
  minor = alutGetMinorVersion ();
  checkForErrors ();
  printf ("ALUT version: %d.%d\n", (int) major, (int) minor);

  s = alutGetMIMETypes (ALUT_LOADER_BUFFER);
  checkForErrors ();
  printExtensions ("ALUT buffer loaders", ',', (const char *) s);

  s = alutGetMIMETypes (ALUT_LOADER_MEMORY);
  checkForErrors ();
  printExtensions ("ALUT memory loaders", ',', (const char *) s);
}

static void
printALCInfo ()
{
  const ALCchar *s;
  ALCint major, minor;
  ALCdevice *device;

  if (alcIsExtensionPresent (NULL, (const ALCchar *) "ALC_ENUMERATION_EXT") ==
      AL_TRUE)
    {
      s = alcGetString (NULL, ALC_DEVICE_SPECIFIER);
      checkForErrors ();
      printf ("available devices:\n");
      while (*s != '\0')
        {
          printf ("    %s\n", (const char *) s);
          while (*s++ != '\0')
            ;
        }
    }
  else
    {
      printf ("no device enumeration available\n");
    }

  device = alcGetContextsDevice (alcGetCurrentContext ());
  checkForErrors ();

  printf ("default device: %s\n",
          (const char *) alcGetString (device, ALC_DEFAULT_DEVICE_SPECIFIER));
  checkForErrors ();

  alcGetIntegerv (device, ALC_MAJOR_VERSION, 1, &major);
  alcGetIntegerv (device, ALC_MAJOR_VERSION, 1, &minor);
  checkForErrors ();
  printf ("ALC version: %d.%d\n", (int) major, (int) minor);

  s = alcGetString (device, ALC_EXTENSIONS);
  checkForErrors ();
  printExtensions ("ALC extensions", ' ', (const char *) s);
}

static void
printALInfo ()
{
  const ALchar *s;

  s = alGetString (AL_VENDOR);
  checkForErrors ();
  printf ("OpenAL vendor string: %s\n", (const char *) s);

  s = alGetString (AL_RENDERER);
  checkForErrors ();
  printf ("OpenAL renderer string: %s\n", (const char *) s);

  s = alGetString (AL_VERSION);
  checkForErrors ();
  printf ("OpenAL version string: %s\n", (const char *) s);

  s = alGetString (AL_EXTENSIONS);
  checkForErrors ();
  printExtensions ("OpenAL extensions", ' ', (const char *) s);
}

int
main (int argc, char *argv[])
{
  alutInit (&argc, argv);
  checkForErrors ();

  printALUTInfo ();
  printALCInfo ();
  printALInfo ();
  checkForErrors ();

  alutExit ();

  return EXIT_SUCCESS;
}
