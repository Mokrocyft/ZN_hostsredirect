cmake_minimum_required(VERSION 3.31...4.5)
project(hostsredirect LANGUAGES C)

add_library(hostsredirect SHARED src/hook.c)

target_compile_options(hostsredirect PRIVATE
    -std=c2y
    -O3
    -flto
    -march=armv9.2-a+crypto
    -mtune=oryon-1
    -mbranch-protection=standard
    -fno-unwind-tables
    -fno-asynchronous-unwind-tables
    -fno-stack-protector
    -fvisibility=hidden
    -ffunction-sections
    -fdata-sections
    -fmerge-all-constants
    -fomit-frame-pointer
    -fno-plt
    -fno-ident
    -Wall
    -Wextra
    -Werror
)

target_link_options(hostsredirect PRIVATE
    -O3
    -flto
    -fuse-ld=lld
    -Wl,-O2
    -Wl,--gc-sections
    -Wl,--icf=all
    -Wl,--strip-all
    -Wl,--pack-dyn-relocs=relr
    -Wl,--hash-style=gnu
    -Wl,--build-id=none
    -Wl,-Bsymbolic
    -Wl,--exclude-libs,ALL
    -Wl,--no-undefined
    -Wl,-z,max-page-size=16384
    -Wl,-z,common-page-size=16384
    -Wl,-z,relro
    -Wl,-z,now
    -Wl,-z,noexecstack
)
