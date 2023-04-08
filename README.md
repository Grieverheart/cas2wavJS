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
and then visit the site (localhost) to load the application or you can visit [http://nicktasios.nl/projects/cas2wav.html](http://nicktasios.nl/projects/cas2wav).

You can load either a CAS file or a ZIP file containing a CAS file.

For creating cables for your MSX, you can check [http://www.finnov.net/~wierzbowsky/caslink2.htm](http://www.finnov.net/~wierzbowsky/caslink2.htm).

# Contributing

If you'd like to contribute, either create an issue with your question/request, or make a pull request. Currently the UI is very basic, and if you'd like to create simple and clean interface I'd appreciate it.
