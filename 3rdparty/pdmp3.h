/*
 Public Domain (www.unlicense.org)
 This is free and unencumbered software released into the public domain.
 Anyone is free to copy, modify, publish, use, compile, sell, or distribute this
 software, either in source code form or as a compiled binary, for any purpose,
 commercial or non-commercial, and by any means.
 In jurisdictions that recognize copyright laws, the author or authors of this
 software dedicate any and all copyright interest in the software to the public
 domain. We make this dedication for the benefit of the public at large and to
 the detriment of our heirs and successors. We intend this dedication to be an
 overt act of relinquishment in perpetuity of all present and future rights to
 this software under copyright law.
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

 Original version written by Krister Lagerström(krister@kmlager.com)
 Website: https://sites.google.com/a/kmlager.com/www/projects

 Contributors:
   technosaurus
   Erik Hofman(erik@ehofman.com)
     - added a subset of the libmpg123 compatible streaming API
     - added ID3v2 support
*/

#if HAVE_CONFIG_H
#include "config.h"
#endif

#ifndef PDMP3_H
#define PDMP3_H

#define PDMP3_INBUF_SIZE	(4*4096)

/** define a subset of a libmpg123 compatible streaming API */
#define PDMP3_OK           0
#define PDMP3_ERR         -1
#define PDMP3_NEED_MORE  -10
#define PDMP3_NEW_FORMAT -11
#define PDMP3_NO_SPACE     7

typedef enum { /* Layer number */
  mpeg1_layer_reserved = 0,
  mpeg1_layer_3        = 1,
  mpeg1_layer_2        = 2,
  mpeg1_layer_1        = 3
}
t_mpeg1_layer;

typedef enum { /* Modes */
  mpeg1_mode_stereo = 0,
  mpeg1_mode_joint_stereo,
  mpeg1_mode_dual_channel,
  mpeg1_mode_single_channel
}
t_mpeg1_mode;

typedef struct { /* MPEG1 Layer 1-3 frame header */
  unsigned id;                 /* 1 bit */
  t_mpeg1_layer layer;         /* 2 bits */
  unsigned protection_bit;     /* 1 bit */
  unsigned bitrate_index;      /* 4 bits */
  unsigned sampling_frequency; /* 2 bits */
  unsigned padding_bit;        /* 1 bit */
  unsigned private_bit;        /* 1 bit */
  t_mpeg1_mode mode;           /* 2 bits */
  unsigned mode_extension;     /* 2 bits */
  unsigned copyright;          /* 1 bit */
  unsigned original_or_copy;   /* 1 bit */
  unsigned emphasis;           /* 2 bits */
}
t_mpeg1_header;

typedef struct {  /* MPEG1 Layer 3 Side Information : [2][2] means [gr][ch] */
  unsigned main_data_begin;         /* 9 bits */
  unsigned private_bits;            /* 3 bits in mono,5 in stereo */
  unsigned scfsi[2][4];             /* 1 bit */
  unsigned part2_3_length[2][2];    /* 12 bits */
  unsigned big_values[2][2];        /* 9 bits */
  unsigned global_gain[2][2];       /* 8 bits */
  unsigned scalefac_compress[2][2]; /* 4 bits */
  unsigned win_switch_flag[2][2];   /* 1 bit */
  /* if(win_switch_flag[][]) */ //use a union dammit
  unsigned block_type[2][2];        /* 2 bits */
  unsigned mixed_block_flag[2][2];  /* 1 bit */
  unsigned table_select[2][2][3];   /* 5 bits */
  unsigned subblock_gain[2][2][3];  /* 3 bits */
  /* else */
  /* table_select[][][] */
  unsigned region0_count[2][2];     /* 4 bits */
  unsigned region1_count[2][2];     /* 3 bits */
  /* end */
  unsigned preflag[2][2];           /* 1 bit */
  unsigned scalefac_scale[2][2];    /* 1 bit */
  unsigned count1table_select[2][2];/* 1 bit */
  unsigned count1[2][2];            /* Not in file,calc. by huff.dec.! */
}
t_mpeg1_side_info;

typedef struct { /* MPEG1 Layer 3 Main Data */
  unsigned  scalefac_l[2][2][21];    /* 0-4 bits */
  unsigned  scalefac_s[2][2][12][3]; /* 0-4 bits */
  float is[2][2][576];               /* Huffman coded freq. lines */
}
t_mpeg1_main_data;

typedef struct
{
  char *p;
  size_t size;
  size_t fill;
} pdmp3_string;

typedef struct
{
  char lang[3];
  char id[4];
  pdmp3_string description;
  pdmp3_string text;
  unsigned char encoding;
} pdmp3_text;

typedef struct
{
  pdmp3_string description;
  pdmp3_string mime_type;
  size_t size;
  unsigned char *data;
} pdmp3_picture;

typedef struct
{
  char tag[3];
  char title[30];
  char artist[30];
  char album[30];
  char year[4];
  char comment[30];
  unsigned char genre;
} pdmp3_id3v1;

typedef struct
{
  unsigned char version;
  pdmp3_string *title;
  pdmp3_string *artist;
  pdmp3_string *album;
  pdmp3_string *year;
  pdmp3_string *genre;
  pdmp3_string *comment;
  pdmp3_text *comment_list;
  size_t comments;
  pdmp3_text *text;
  size_t texts;
  pdmp3_text *extra;
  size_t extras;
  pdmp3_picture *picture;
  size_t pictures;
  pdmp3_text _text[32];
} pdmp3_id3v2;

typedef struct
{
  size_t processed;
  unsigned istart,iend,ostart;
  unsigned char in[PDMP3_INBUF_SIZE];
  unsigned out[2][576];
  t_mpeg1_header g_frame_header;  
  t_mpeg1_side_info g_side_info;  /* < 100 words */
  t_mpeg1_main_data g_main_data;
  
  unsigned hsynth_init;
  unsigned synth_init;
  /* Bit reservoir for main data */
  unsigned g_main_data_vec[2*1024];/* Large static data */
  unsigned *g_main_data_ptr;/* Pointer into the reservoir */
  unsigned g_main_data_idx;/* Index into the current byte(0-7) */
  unsigned g_main_data_top;/* Number of bytes in reservoir(0-1024) */
  /* Bit reservoir for side info */
  unsigned side_info_vec[32+4];
  unsigned *side_info_ptr;  /* Pointer into the reservoir */
  unsigned side_info_idx;  /* Index into the current byte(0-7) */
  
  pdmp3_id3v2 *id3v2;
  unsigned id3v2_size;
  unsigned id3v2_frame_pos;
  unsigned id3v2_frame_size;
  char id3v2_processing;
  char id3v2_flags;
  char new_header;
}
pdmp3_handle;

int Read_Header(pdmp3_handle*);
void Free_ID3v2(pdmp3_id3v2*);

#endif // PDMP3_H
