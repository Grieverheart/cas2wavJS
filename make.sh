emcc -O3 -s "EXPORTED_FUNCTIONS=['_cas2wav','_malloc']" -sEXPORTED_RUNTIME_METHODS=ccall -sALLOW_TABLE_GROWTH -sALLOW_MEMORY_GROWTH=1 cas2wav_func.c -o cas2wav.js
gcc -Wall -O1 -g -o main cas2wav.c
