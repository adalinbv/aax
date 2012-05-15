#ifndef __WAVEFILE_H
#define __WAVEFILE_H

#include <aax.h>

#define _OPENAL_SUPPORT		0

aaxBuffer bufferFromFile(aaxConfig, const char *);
void *fileLoad(const char *, unsigned int *, unsigned *, int *, char *, char *, unsigned int *);
#define fileWrite(a, b, c, d, e, f) \
        aaxWritePCMToFile(a, b, c, d, e, f)

void *fileDataConvertToInterleaved(void *, char, char, unsigned int);
enum aaxFormat getFormatFromFileFormat(unsigned int, int);

#endif
