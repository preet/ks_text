### What is ks?
ks is a small cross platform c++ library that can be used to help create applications and libraries.

### What is ks_text?
The ks_text module wraps FreeType,HarfBuzz,a subset of ICU and libunibreak to provide basic text rendering, shaping and layout. All of the dependencies are built with the source to make it easier to deploy in certain environments.

### License
This module is licensed under the Apache License, version 2.0. For more information see the LICENSE file.

### Dependencies

* ks_shared
* [**FreeType**](http://www.freetype.org) (FreeType License): For text rendering
* [**HarfBuzz**](https://github.com/behdad/harfbuzz) (MIT License): For text shaping
* [**ICU**](http://icu-project.org) (ICU License): For bidirectional text
* [**libunibreak**](https://github.com/adah1972/libunibreak) (zlib/libpng License): For line breaking

### Building
The provided pri file can be added to a qmake project. Ensure the dependent ks modules are included in any project that uses this module.

### Documentation
TODO
