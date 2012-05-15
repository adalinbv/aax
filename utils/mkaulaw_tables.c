
#include <stdio.h>
#include <stdint.h>
#include <assert.h>

/*
 * From: http://www.multimedia.cx/simpleaudio.html#tth_sEc6.1
 */
static const uint16_t exp_lut[8] = {
   0, 132, 396, 924, 1980, 4092, 8316, 16764
};

int16_t
mulaw2linear (uint8_t mulawbyte)
{
  int16_t sign, exponent, mantissa, sample;

  mulawbyte = ~mulawbyte;
  sign = (mulawbyte & 0x80);
  exponent = (mulawbyte >> 4) & 0x07;
  mantissa = mulawbyte & 0x0F;

  sample = exp_lut[exponent] + (mantissa << (exponent + 3));
  if (sign != 0)
    {
      sample = -sample;
    }
  return sample;
}

/*
 * From: http://www.multimedia.cx/simpleaudio.html#tth_sEc6.2
 */
#define SIGN_BIT (0x80)         /* Sign bit for a A-law byte. */
#define QUANT_MASK (0xf)        /* Quantization field mask. */
#define SEG_SHIFT (4)           /* Left shift for segment number. */
#define SEG_MASK (0x70)         /* Segment field mask. */
int16_t
alaw2linear (uint8_t a_val)
{
  uint16_t t, seg;
  a_val ^= 0x55;
  t = (a_val & QUANT_MASK) << 4;
  seg = ((uint16_t) a_val & SEG_MASK) >> SEG_SHIFT;

  switch (seg)
    {
    case 0:
      t += 8;
      break;
    case 1:
      t += 0x108;
      break;
    default:
      t += 0x108;
      t <<= seg - 1;
    }
  return (a_val & SIGN_BIT) ? t : -t;
}


int main()
{
   int i;

   printf("\n#include <base/types.h>\n\n");

   printf("const int16_t _ima4_index_table[16] =\n{\n   -1, -1, -1, -1, 2, 4, 6, 8,\n   -1, -1, -1, -1, 2, 4, 6, 8\n};\n\n");
   printf("const int16_t _ima4_index_adjust[16] =\n{\n   1, 3, 5, 7, 9, 11, 13, 15,\n   -1, -3, -5, -7, -9, -11, -13, -15\n};\n\n");
   printf("const int16_t _ima4_step_table[89] =\n{\n   7, 8, 9, 10, 11, 12, 13, 14, 16, 17,\n   19, 21, 23, 25, 28, 31, 34, 37, 41, 45,\n   50, 55, 60, 66, 73, 80, 88, 97, 107, 118,\n   130, 143, 157, 173, 190, 209, 230, 253, 279, 307,\n   337, 371, 408, 449, 494, 544, 598, 658, 724, 796,\n   876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066,\n   2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358,\n   5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899,\n   15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767\n};\n\n");

   printf("const int16_t _mulaw2linear_table[256] = {");
   for (i=0; i<255; i++)
   {
      if ((i%8) == 0) printf("\n");
      printf("  %6i,", mulaw2linear(i));
   }
   printf("  %6i\n};\n\n", mulaw2linear(255));

   printf("const int16_t _alaw2linear_table[256] = {");
   for (i=0; i<255; i++)
   {
      if ((i%8) == 0) printf("\n");
      printf("  %6i,", alaw2linear(i));
   }  
   printf("  %6i\n};\n\n", alaw2linear(255));

   return 0;
}
