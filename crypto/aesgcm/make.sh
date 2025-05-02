#!/bin/sh

perl aesni_gcm-x64.pl macosx > aesni_gcm-x64-osx.s
perl aesni-x64.pl macosx     > aesni-x64-osx.s
perl ghash-x64.pl macosx     > ghash-x64-osx.s

perl aesni_gcm-x64.pl gas    > aesni_gcm-x64-linux.s
perl aesni-x64.pl gas        > aesni-x64-linux.s
perl ghash-x64.pl gas        > ghash-x64-linux.s

perl aes-gcm-armv8_64.pl linux64 > aes-gcm-arm64-linux.S
perl ghashv8-armx.pl linux32 > ghash-arm-linux.S

