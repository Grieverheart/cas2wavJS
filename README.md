# Browser-based CAS to WAV converter

This is a browser-based application for converting CAS files to WAV files for playback on digital devices.
The implementation is based on [cas2wav](https://github.com/joyrex2001/castools).

# Requirements
* [Emscripten](https://emscripten.org)

# Usage
Run`make.sh` on the command-line to compile the wasm and js code using [Emscripten](https://emscripten.org).

You need to serve `index.html` for example using Python:
```
python -m http.server
```
and then visit the site (localhost) to load the application or you can visit [http://nicktasios.nl/projects/cas2wav.html](http://nicktasios.nl/projects/cas2wav.html).

For creating cables for your MSX, you can check [http://www.finnov.net/~wierzbowsky/caslink2.htm](http://www.finnov.net/~wierzbowsky/caslink2.htm).

#Contributing
