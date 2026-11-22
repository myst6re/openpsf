# OpenPSF

Tiny lib to stream PSF files (PlayStation Midi).

C++ API is defined in openpsf.h and C API is defined in openpsf_c.h.

## Build

```sh
git submodule update --init
cmake --build ./.build -j3
```

## Credits

This library shares lot of code with the Highly Experimental
PSF Decoder for foobar2000.

Many thanks to its authors:
 - kode54 (maintainer of many foobar2000 plugins)
 - Neill Corlett (author of Highly Experimental)
 - And all project contributors 
