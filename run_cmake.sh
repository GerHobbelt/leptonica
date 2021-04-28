#! /bin/bash
#
# Because otherwise I loose the magic incantation...
# 

mkdir build
rm -rf build/C*
cd build
rm *.txt
# cmake -D CMAKE_FIND_DEBUG_MODE=ON -D 'CMAKE_PREFIX_PATH=../libpng;../libjpeg;../libtiff;../lcms2;../openjpeg;../jbig2dec;../zlib'  ..
cmake -D ZLIB_ROOT=../zlib/ -D JPEG_ROOT=../libjpeg/ -D TIFF_ROOT=../libtiff/ -D 'CMAKE_PREFIX_PATH=../libpng;../libjpeg;../libtiff;../lcms2;../openjpeg;../jbig2dec;../zlib'  ..
