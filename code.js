console.log('hello world');

/* headers definitions */
const HEADER = new Uint8Array([0x1F,0xA6,0xDE,0xBA,0xCC,0x13,0x7D,0x74]);
const ASCII  = new Uint8Array([0xEA,0xEA,0xEA,0xEA,0xEA,0xEA,0xEA,0xEA,0xEA,0xEA]);
const BIN    = new Uint8Array([0xD0,0xD0,0xD0,0xD0,0xD0,0xD0,0xD0,0xD0,0xD0,0xD0]);
const BASIC  = new Uint8Array([0xD3,0xD3,0xD3,0xD3,0xD3,0xD3,0xD3,0xD3,0xD3,0xD3]);
const OUTPUT_FREQUENCY = 43200;
const PCM_WAVE_FORMAT  = 1;
const MONO             = 1;
const STEREO           = 2;

var enc = new TextEncoder();

let data = new Array();

// Write header
{
    const header = new ArrayBuffer(44);

    let RiffID          = new Uint8Array(header, 0, 4);
    let RiffSize        = new Uint32Array(header, 4, 1);
    let WaveID          = new Uint8Array(header, 8, 4);
    let FmtID           = new Uint8Array(header, 12, 4);
    let FmtSize         = new Uint32Array(header, 16, 1);
    let wFormatTag      = new Uint16Array(header, 20, 1);
    let nChannels       = new Uint16Array(header, 22, 1);
    let nSamplesPerSec  = new Uint32Array(header, 24, 1);
    let nAvgBytesPerSec = new Uint32Array(header, 28, 1);
    let nBlockAlign     = new Uint16Array(header, 32, 1);
    let wBitsPerSample  = new Uint16Array(header, 34, 1);
    let DataID          = new Uint8Array(header, 36, 4);
    let nDataBytes      = new Uint32Array(header, 40, 1);
    

    RiffID.set(enc.encode("RIFF"));
    RiffSize.set([0]);
    WaveID.set(enc.encode("WAVE"));
    FmtID.set(enc.encode("fmt "));
    FmtSize.set([16]);
    wFormatTag.set([PCM_WAVE_FORMAT]);
    nChannels.set([MONO]);
    nSamplesPerSec.set([OUTPUT_FREQUENCY]);
    nAvgBytesPerSec.set([OUTPUT_FREQUENCY]);
    nBlockAlign.set([1]);
    wBitsPerSample.set([8]);
    DataID.set(enc.encode("data"));
    nDataBytes.set([0]);

    data = data.concat(...new Uint8Array(header));
}

console.log(data);
