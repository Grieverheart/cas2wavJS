
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>

#ifdef MAIN
#include <stdio.h>
#endif

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
} WaveHeader;



/* write a pulse */
static inline void write_pulse(Buffer* buffer, uint32_t f, uint32_t baud_rate)
{
    uint32_t n;
    double length = OUTPUT_FREQUENCY / (baud_rate * (f / 1200));
    double scale  = 2.0 * M_PI / (double)length;

    // @todo: maybe resize
    for(n = 0; n < (uint32_t)length; ++n)
    {
        char val = (char)(sin((double)n * scale)*127)^128;
        buffer_write_byte(buffer, val);
    }
}

/* write a header signal */
static inline void write_header(Buffer* output, uint32_t s, uint32_t baud_rate)
{
    for(int i = 0; i < s*(baud_rate/1200); ++i) write_pulse(output, SHORT_PULSE, baud_rate);
}



/* write silence */
static inline void write_silence(Buffer *buffer, uint32_t s)
{
    buffer_fill(buffer, 128u, s);
}


/* write a byte */
static inline void write_byte(Buffer* buffer, int byte, uint32_t baud_rate)
{
    /* one start bit */
    write_pulse(buffer, LONG_PULSE, baud_rate);

    /* eight data bits */
    for(int i = 0; i < 8; ++i)
    {
        if(byte & 1)
        {
            write_pulse(buffer, SHORT_PULSE, baud_rate);
            write_pulse(buffer, SHORT_PULSE, baud_rate);
        }
        else
            write_pulse(buffer, LONG_PULSE, baud_rate);

        byte >>= 1;
    }

    /* two stop bits */
    for(int i = 0; i < 4; ++i)
        write_pulse(buffer, SHORT_PULSE, baud_rate);
}



/* write data until a header is detected */
static inline void write_data(const uint8_t* data, uint32_t size_data, uint32_t *position, Buffer* buffer, uint32_t baud_rate, bool* eof)
{
    int  read = 8;
    const uint8_t* file_buffer = NULL;

    *eof = false;
    while(true)
    {
        if(*position + 8 >= size_data)
        {
            read = size_data - *position;
            break;
        }

        file_buffer = data + *position;
        if(memcmp(file_buffer, HEADER, 8) == 0) return;

        write_byte(buffer, file_buffer[0], baud_rate);
        if(file_buffer[0] == 0x1a) *eof = true;
        ++*position;
    }

    if(!file_buffer) return;

    for(int i = 0; i < read; ++i) write_byte(buffer, file_buffer[i], baud_rate);

    if(file_buffer[0] == 0x1a) *eof = true;
    *position += read;

    return;
}


// @todo: Needs some cleaning up. Perhaps implement from scratch?
uint8_t* cas2wav(const uint8_t* cas, uint32_t cas_size, uint32_t* wav_size, uint32_t* mode, int stime, uint32_t baud_rate)
{
    Buffer wav_buffer;
    wav_buffer.size = sizeof(WaveHeader);
    wav_buffer.capacity = 2 * sizeof(WaveHeader);
    wav_buffer.data = (uint8_t*) malloc(wav_buffer.capacity);


    bool first_header = true;

    /* search for a header in the .cas file */
    uint32_t position = 0;
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
            if(position + 10 < cas_size)
            {
                uint32_t current_mode = 0;
                buffer = cas + position;

                if(!memcmp(buffer, ASCII, 10))      current_mode = 1;
                else if(!memcmp(buffer, BIN, 10))   current_mode = 2;
                else if(!memcmp(buffer, BASIC, 10)) current_mode = 3;

                if(first_header)
                {
                    *mode = current_mode;
                    first_header = false;
                }

                if(current_mode == 0)
                {
                    write_silence(&wav_buffer, LONG_SILENCE);
                    write_header(&wav_buffer, LONG_HEADER, baud_rate);
                    write_data(cas, cas_size, &position, &wav_buffer, baud_rate, &eof);
                }
                else if(current_mode == 1)
                {
                    // This is the filename.
                    //for(size_t k = 0; k < 6; ++k)
                    //    printf("%c", cas[position + 10 + k]);
                    //printf("\n");

                    write_silence(&wav_buffer, (stime > 0)? OUTPUT_FREQUENCY*stime: LONG_SILENCE);
                    write_header(&wav_buffer, LONG_HEADER, baud_rate);
                    write_data(cas, cas_size, &position, &wav_buffer, baud_rate, &eof);

                    do
                    {
                        position += 8;
                        write_silence(&wav_buffer, SHORT_SILENCE);
                        write_header(&wav_buffer, SHORT_HEADER, baud_rate);
                        write_data(cas, cas_size, &position, &wav_buffer, baud_rate, &eof);
                    }
                    while(!eof && position < cas_size);

                }
                else
                {
                    // This is the filename.
                    //for(size_t k = 0; k < 6; ++k)
                    //    printf("%c", cas[position + 10 + k]);
                    //printf("\n");

                    // @note: Unknown file type, using long header.
                    write_silence(&wav_buffer, (stime > 0)? OUTPUT_FREQUENCY*stime: LONG_SILENCE);
                    write_header(&wav_buffer, LONG_HEADER, baud_rate);
                    write_data(cas, cas_size, &position, &wav_buffer, baud_rate, &eof);
                    write_silence(&wav_buffer, SHORT_SILENCE);
                    write_header(&wav_buffer, SHORT_HEADER, baud_rate);
                    position+=8;
                    write_data(cas, cas_size, &position, &wav_buffer, baud_rate, &eof);
                }
            }
            else
            {
                // @note: Unknown file type, using long header.
                write_silence(&wav_buffer, (stime > 0)? OUTPUT_FREQUENCY*stime: LONG_SILENCE);
                write_header(&wav_buffer, LONG_HEADER, baud_rate);
                write_data(cas, cas_size, &position, &wav_buffer, baud_rate, &eof);
            }

        }
        else
        {
            /* should not occur */
            ++position;
        }
    }

    WaveHeader waveheader =
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

#ifdef MAIN
int main(int argc, char* argv[])
{
  FILE *output,*input;
  int  i,j;
  int  stime = -1;

  char *ifile = NULL;
  char *ofile = NULL;

  uint32_t baud_rate = 1200;
  /* parse command line options */
  for (i=1; i<argc; i++) {

    if (argv[i][0]=='-') {

      for(j=1;j && argv[i][j]!='\0';j++)

        switch(argv[i][j]) {

        case '2': baud_rate=2400; break;
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

  uint32_t wav_size, mode;
  uint8_t* wav = cas2wav(cas, cas_size, &wav_size, &mode, stime, baud_rate);


  fwrite(wav, 1, wav_size, output);
  fclose(output);

  return 0;
}
#endif

