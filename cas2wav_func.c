/**************************************************************************/
/*                                                                        */
/* file:         cas2wav.c                                                */
/* version:      1.31 (April 11, 2016)                                    */
/*                                                                        */
/* description:  This tool exports the contents of a .cas file to a .wav  */
/*               file. The .cas format is the standard format for MSX to  */
/*               emulate tape recorders. The wav can be copied onto a     */
/*               tape to be read by a real MSX.                           */
/*                                                                        */
/*                                                                        */
/*  This program is free software; you can redistribute it and/or modify  */
/*  it under the terms of the GNU General Public License as published by  */
/*  the Free Software Foundation; either version 2, or (at your option)   */
/*  any later version. See COPYING for more details.                      */
/*                                                                        */
/*                                                                        */
/* Copyright 2001-2016 Vincent van Dam (vincentd@erg.verweg.com)          */
/* MultiCPU Copyright 2007 Ramones     (ramones@kurarizeku.net)           */
/*                                                                        */
/**************************************************************************/

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <math.h>

/* CPU type defines */
#if (BIGENDIAN)
#define BIGENDIANSHORT(value)  ( ((value & 0x00FF) << 8) | \
                                 ((value & 0xFF00) >> 8) )

#define BIGENDIANINT(value)    ( ((value & 0x000000FF) << 24) | \
                                 ((value & 0x0000FF00) << 8)  | \
                                 ((value & 0x00FF0000) >> 8)  | \
                                 ((value & 0xFF000000) >> 24) )
#define BIGENDIANLONG(value)   BIGENDIANINT(value) // I suppose Long=int
#else
#define BIGENDIANSHORT(value) value
#define BIGENDIANINT(value)   value
#define BIGENDIANLONG(value)  value
#endif

/* number of ouput bytes for silent parts */
#define SHORT_SILENCE     OUTPUT_FREQUENCY    /* 1 second  */
#define LONG_SILENCE      OUTPUT_FREQUENCY*2  /* 2 seconds */

/* frequency for pulses */
#define LONG_PULSE        1200
#define SHORT_PULSE       2400

/* number of uint16_t pulses for headers */
#define LONG_HEADER       16000
#define SHORT_HEADER      4000

/* output settings */
#define OUTPUT_FREQUENCY  43200

/* headers definitions */
static const char HEADER[8] = { 0x1F,0xA6,0xDE,0xBA,0xCC,0x13,0x7D,0x74 };
static const char ASCII[10] = { 0xEA,0xEA,0xEA,0xEA,0xEA,0xEA,0xEA,0xEA,0xEA,0xEA };
static const char BIN[10]   = { 0xD0,0xD0,0xD0,0xD0,0xD0,0xD0,0xD0,0xD0,0xD0,0xD0 };
static const char BASIC[10] = { 0xD3,0xD3,0xD3,0xD3,0xD3,0xD3,0xD3,0xD3,0xD3,0xD3 };

/* definitions for .wav file */
#define PCM_WAVE_FORMAT   1
#define MONO              1
#define STEREO            2

typedef struct
{
    uint8_t* data;
    uint32_t capacity;
    uint32_t size;
} Buffer;

void buffer_write_byte(Buffer* buffer, uint8_t byte)
{
    if(buffer->size + 1 >= buffer->capacity)
    {
        buffer->capacity *= 2;
        buffer->data = (uint8_t*) realloc(buffer->data, buffer->capacity);
    }
    buffer->data[buffer->size - 1] = byte;
    ++buffer->size;
}

void buffer_write_bytes(Buffer* buffer, const uint8_t* bytes, uint32_t n_bytes)
{
    if(buffer->size + n_bytes >= buffer->capacity)
    {
        buffer->capacity = 2 * (buffer->capacity + n_bytes);
        buffer->data = (uint8_t*) realloc(buffer->data, buffer->capacity);
    }
    memcpy(buffer->data + buffer->size - 1, bytes, n_bytes);
    buffer->size += n_bytes;
}

void buffer_fill(Buffer* buffer, uint8_t byte, uint32_t n_bytes)
{
    if(buffer->size + n_bytes >= buffer->capacity)
    {
        buffer->capacity = 2 * (buffer->size + n_bytes);
        buffer->data = (uint8_t*) realloc(buffer->data, buffer->capacity);
    }
    memset(buffer->data + buffer->size - 1, byte, n_bytes);
    buffer->size += n_bytes;
}

typedef struct
{
  char      RiffID[4];
  uint32_t  RiffSize;
  char      WaveID[4];
  char      FmtID[4];
  uint32_t  FmtSize;
  uint16_t  wFormatTag;
  uint16_t  nChannels;
  uint32_t  nSamplesPerSec;
  uint32_t  nAvgBytesPerSec;
  uint16_t  nBlockAlign;
  uint16_t  wBitsPerSample;
  char      DataID[4];
  uint32_t  nDataBytes;
} WAVE_HEADER;



/* write a pulse */
static inline void writePulse(Buffer* buffer, uint32_t f, uint32_t BAUDRATE)
{
    uint32_t n;
    double length = OUTPUT_FREQUENCY / (BAUDRATE * (f / 1200));
    double scale  = 2.0 * M_PI / (double)length;

    // @todo: maybe resize
    for(n = 0; n < (uint32_t)length; ++n)
    {
        char val = (char)(sin((double)n * scale)*127)^128;
        buffer_write_byte(buffer, val);
    }
}

/* write a header signal */
static inline void writeHeader(Buffer* output, uint32_t s, uint32_t BAUDRATE)
{
    for(int i = 0; i < s*(BAUDRATE/1200); ++i) writePulse(output, SHORT_PULSE, BAUDRATE);
}



/* write silence */
static inline void writeSilence(Buffer *buffer, uint32_t s)
{
    buffer_fill(buffer, 128u, s);
}



/* write a byte */
static inline void writeByte(Buffer* buffer, int byte, uint32_t BAUDRATE)
{
    /* one start bit */
    writePulse(buffer, LONG_PULSE, BAUDRATE);

    /* eight data bits */
    for(int i = 0; i < 8; ++i)
    {
        if(byte & 1)
        {
            writePulse(buffer, SHORT_PULSE, BAUDRATE);
            writePulse(buffer, SHORT_PULSE, BAUDRATE);
        }
        else
            writePulse(buffer, LONG_PULSE, BAUDRATE);

        byte >>= 1;
    }

    /* two stop bits */
    for(int i = 0; i < 4; ++i)
        writePulse(buffer, SHORT_PULSE, BAUDRATE);
}



/* write data until a header is detected */
static inline void writeData(const uint8_t* data, uint32_t size_data, uint32_t *position, Buffer* buffer, uint32_t BAUDRATE, bool* eof)
{
    int  read = 8;
    char file_buffer[8];

    *eof = false;
    while(true)
    {
        if(*position + 8 >= size_data)
        {
            read = size_data - *position;
            break;
        }

        const uint8_t* file_buffer = data + *position;
        if(memcmp(file_buffer, HEADER, 8) == 0) return;

        writeByte(buffer, file_buffer[0], BAUDRATE);
        if(file_buffer[0] == 0x1a) *eof = true;
        ++*position;
    }

    for(int i = 0; i < read; ++i) writeByte(buffer, file_buffer[i], BAUDRATE);

    if(file_buffer[0] == 0x1a) *eof = true;
    *position += read;

    return;
}


uint8_t* cas2wav(const uint8_t* cas, uint32_t cas_size, uint32_t* wav_size, int stime, uint32_t BAUDRATE)
{
    Buffer wav_buffer;
    wav_buffer.size = sizeof(WAVE_HEADER);
    wav_buffer.capacity = 2 * sizeof(WAVE_HEADER);
    wav_buffer.data = (uint8_t*) malloc(wav_buffer.capacity);

    ///* write initial .wav header */
    //memcpy(wav_buffer.data, (uint8_t*)&waveheader, sizeof(waveheader));

    uint32_t position = 0;
    /* search for a header in the .cas file */
    while(true)
    {
        if(position + 8 > cas_size) break;

        const uint8_t* buffer = cas + position;
        if(memcmp(buffer, HEADER, 8) == 0)
        {

            /* it probably works fine if a long header is used for every */
            /* header but since the msx bios makes a distinction between */
            /* them, we do also (hence a lot of code).                   */

            bool eof;
            position += 8;
            if(position + 10 > cas_size)
            {
                writeSilence(&wav_buffer, (stime > 0)? OUTPUT_FREQUENCY*stime: LONG_SILENCE);
                writeHeader(&wav_buffer, LONG_HEADER, BAUDRATE);
                writeData(cas, cas_size, &position, &wav_buffer, BAUDRATE, &eof);
                free(wav_buffer.data);
                return NULL;
            }

            buffer = cas + position;

            if(memcmp(buffer, ASCII, 10) == 0)
            {
                writeSilence(&wav_buffer, (stime > 0)? OUTPUT_FREQUENCY*stime: LONG_SILENCE);
                writeHeader(&wav_buffer, LONG_HEADER, BAUDRATE);
                writeData(cas, cas_size, &position, &wav_buffer, BAUDRATE, &eof);

                do
                {
                    position += 8;
                    writeSilence(&wav_buffer, SHORT_SILENCE);
                    writeHeader(&wav_buffer, SHORT_HEADER, BAUDRATE);
                    writeData(cas, cas_size, &position, &wav_buffer, BAUDRATE, &eof);
                }
                while(!eof && position < cas_size);

            }
            else if (!memcmp(buffer, BIN, 10) || !memcmp(buffer, BASIC, 10))
            {
                writeSilence(&wav_buffer,stime>0?OUTPUT_FREQUENCY*stime:LONG_SILENCE);
                writeHeader(&wav_buffer,LONG_HEADER, BAUDRATE);
                writeData(cas, cas_size, &position, &wav_buffer, BAUDRATE, &eof);
                writeSilence(&wav_buffer,SHORT_SILENCE);
                writeHeader(&wav_buffer,SHORT_HEADER, BAUDRATE);
                position+=8;
                writeData(cas, cas_size, &position, &wav_buffer, BAUDRATE, &eof);
            }
            else
            {
                writeSilence(&wav_buffer,LONG_SILENCE);
                writeHeader(&wav_buffer,LONG_HEADER, BAUDRATE);
                writeData(cas, cas_size, &position, &wav_buffer, BAUDRATE, &eof);
            }

        }
        else
        {
            /* should not occur */
            ++position;
        }
    }

    WAVE_HEADER waveheader =
    {
        { "RIFF" },
        BIGENDIANLONG(0),
        { "WAVE" },
        { "fmt " },
        BIGENDIANLONG(16),
        BIGENDIANSHORT(PCM_WAVE_FORMAT),
        BIGENDIANSHORT(MONO),
        BIGENDIANLONG(OUTPUT_FREQUENCY),
        BIGENDIANLONG(OUTPUT_FREQUENCY),
        BIGENDIANSHORT(1),
        BIGENDIANSHORT(8),
        { "data" },
        BIGENDIANLONG(0)
    };

    /* write final .wav header */
    uint32_t size = wav_buffer.size - sizeof(waveheader);
    waveheader.nDataBytes = BIGENDIANLONG(size);
    waveheader.RiffSize = BIGENDIANLONG(size);

    memcpy(wav_buffer.data, (uint8_t*)&waveheader, sizeof(waveheader));

    uint8_t* wav = wav_buffer.data;
    wav = (uint8_t*) realloc(wav, wav_buffer.size);
    *wav_size = wav_buffer.size;

    return wav;
}

int main(int argc, char* argv[])
{
  FILE *output,*input;
  int  i,j;
  int  stime = -1;

  char *ifile = NULL;
  char *ofile = NULL;

  uint32_t BAUDRATE = 1200;
  /* parse command line options */
  for (i=1; i<argc; i++) {

    if (argv[i][0]=='-') {

      for(j=1;j && argv[i][j]!='\0';j++)

        switch(argv[i][j]) {

        case '2': BAUDRATE=2400; break;
        case 's': stime=atof(argv[++i]); j=-1; break;

        default:
          fprintf(stderr,"%s: invalid option\n",argv[0]);
          exit(1);
        }

      continue;
    }

    if (ifile==NULL) { ifile=argv[i]; continue; }
    if (ofile==NULL) { ofile=argv[i]; continue; }

    fprintf(stderr,"%s: invalid option\n",argv[0]);
    exit(1);
  }

  /* open input/output files */
  if ((input=fopen(ifile,"rb"))==NULL) {
    fprintf(stderr,"%s: failed opening %s\n",argv[0],argv[1]);
    exit(1);
  }

  if ((output=fopen(ofile,"wb"))==NULL) {
    fprintf(stderr,"%s: failed writing %s\n",argv[0],argv[2]);
    exit(1);
  }

  fseek(input, 0, SEEK_END);
  uint32_t cas_size = ftell(input);
  fseek(input, 0, SEEK_SET);
  uint8_t* cas = (uint8_t*) malloc(cas_size);
  fread(cas, 1, cas_size, input);
  fclose(input);

  uint32_t wav_size;
  uint8_t* wav = cas2wav(cas, cas_size, &wav_size, stime, BAUDRATE);


  fwrite(wav, 1, wav_size, output);
  fclose(output);

  return 0;
}

