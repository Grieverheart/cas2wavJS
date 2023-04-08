emcc -O3 -s "EXPORTED_FUNCTIONS=['_cas2wav','_malloc']" -sEXPORTED_RUNTIME_METHODS=ccall -sALLOW_TABLE_GROWTH -sALLOW_MEMORY_GROWTH=1 cas2wav.c -o cas2wav.js
