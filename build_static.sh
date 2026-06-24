#!/usr/bin/env bash
# ============================================================
# deb2pkg 全静态编译脚本 (musl libc + 静态链接所有依赖)
# 用法: bash build_static.sh
# ============================================================
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build_static"
STATIC_DIR="$BUILD_DIR/static_libs"
SRC_DIR="$BUILD_DIR/src"
JOBS=$(nproc)

echo "===== deb2pkg 全静态编译 ====="
echo "工作目录: $BUILD_DIR"

# 只清理本次的编译产物，保留已下载源码和已编译静态库
rm -rf "$BUILD_DIR/build"
mkdir -p "$STATIC_DIR/lib" "$STATIC_DIR/include" "$SRC_DIR"

# --------------------------------------------------
# 1. zlib (静态)
# --------------------------------------------------
if [ ! -f "$STATIC_DIR/lib/libz.a" ]; then
    echo "[1/6] 编译 zlib..."
    (
        cd "$SRC_DIR"
        ZLIB_VER=1.3.1
        curl -sL "https://github.com/madler/zlib/archive/refs/tags/v${ZLIB_VER}.tar.gz" -o zlib.tar.gz
        tar xzf zlib.tar.gz
        cd "zlib-${ZLIB_VER}"
        CFLAGS="-O2 -fPIC" ./configure --static --prefix="$STATIC_DIR"
        make -j$JOBS && make install
    )
    echo "  zlib OK"
else
    echo "[1/6] zlib 已存在，跳过"
fi

# --------------------------------------------------
# 2. bzip2 (静态)
# --------------------------------------------------
if [ ! -f "$STATIC_DIR/lib/libbz2.a" ]; then
    echo "[2/6] 编译 bzip2..."
    (
        cd "$SRC_DIR"
        BZIP2_VER=1.0.8
        curl -sL "https://sourceware.org/pub/bzip2/bzip2-${BZIP2_VER}.tar.gz" -o bzip2.tar.gz
        tar xzf bzip2.tar.gz
        cd "bzip2-${BZIP2_VER}"
        make -j$JOBS CFLAGS="-O2 -fPIC" libbz2.a
        cp libbz2.a "$STATIC_DIR/lib/"
        cp bzlib.h "$STATIC_DIR/include/"
    )
    echo "  bzip2 OK"
else
    echo "[2/6] bzip2 已存在，跳过"
fi

# --------------------------------------------------
# 3. xz/liblzma (静态)
# --------------------------------------------------
if [ ! -f "$STATIC_DIR/lib/liblzma.a" ]; then
    echo "[3/6] 编译 xz..."
    (
        cd "$SRC_DIR"
        XZ_VER=5.6.4
        curl -sL "https://github.com/tukaani-project/xz/releases/download/v${XZ_VER}/xz-${XZ_VER}.tar.gz" -o xz.tar.gz
        tar xzf xz.tar.gz
        cd "xz-${XZ_VER}"
        ./configure --prefix="$STATIC_DIR" --disable-shared --enable-static \
            --disable-xz --disable-xzdec --disable-lzmadec --disable-lzmainfo \
            --disable-lzma-links --disable-scripts --disable-doc
        make -j$JOBS && make install
    )
    echo "  xz OK"
else
    echo "[3/6] xz 已存在，跳过"
fi

# --------------------------------------------------
# 4. zstd (静态)
# --------------------------------------------------
if [ ! -f "$STATIC_DIR/lib/libzstd.a" ]; then
    echo "[4/6] 编译 zstd..."
    (
        cd "$SRC_DIR"
        ZSTD_VER=1.5.6
        curl -sL "https://github.com/facebook/zstd/releases/download/v${ZSTD_VER}/zstd-${ZSTD_VER}.tar.gz" -o zstd.tar.gz
        tar xzf zstd.tar.gz
        cd "zstd-${ZSTD_VER}/lib"
        make -j$JOBS CFLAGS="-O2 -fPIC" libzstd.a
        cp libzstd.a "$STATIC_DIR/lib/"
        cp zstd.h zdict.h zstd_errors.h "$STATIC_DIR/include/"
    )
    echo "  zstd OK"
else
    echo "[4/6] zstd 已存在，跳过"
fi

# --------------------------------------------------
# 5. libarchive (静态，禁用 ACL/XATTR)
# --------------------------------------------------
if [ ! -f "$STATIC_DIR/lib/libarchive.a" ]; then
    echo "[5/6] 编译 libarchive..."
    (
        cd "$SRC_DIR"
        LA_VER=3.8.7
        curl -sL "https://github.com/libarchive/libarchive/releases/download/v${LA_VER}/libarchive-${LA_VER}.tar.gz" -o libarchive.tar.gz
        tar xzf libarchive.tar.gz
        cd "libarchive-${LA_VER}"
        cmake . \
            -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
            -DCMAKE_BUILD_TYPE=Release \
            -DBUILD_SHARED_LIBS=OFF \
            -DENABLE_LZ4=OFF \
            -DENABLE_LZO=OFF \
            -DENABLE_NETTLE=OFF \
            -DENABLE_OPENSSL=OFF \
            -DENABLE_LIBXML2=OFF \
            -DENABLE_EXPAT=OFF \
            -DENABLE_PCREPOSIX=OFF \
            -DENABLE_LibGCC=OFF \
            -DENABLE_CNG=OFF \
            -DENABLE_ACL=OFF \
            -DENABLE_XATTR=OFF \
            -DENABLE_TAR=ON \
            -DENABLE_CPIO=ON \
            -DENABLE_CAT=OFF \
            -DENABLE_TEST=OFF \
            -DCMAKE_INSTALL_PREFIX="$STATIC_DIR" \
            -DCMAKE_PREFIX_PATH="$STATIC_DIR"
        make -j$JOBS && make install
    )
    echo "  libarchive OK"
else
    echo "[5/6] libarchive 已存在，跳过"
fi

# --------------------------------------------------
# 6. musl 兼容层 (填补 __*_chk 等 glibc→musl 缺失符号)
# --------------------------------------------------
echo "[6/6] 编译 musl 兼容层..."
(
    cd "$SCRIPT_DIR/src/core"
    clang --target=x86_64-linux-musl -O2 -c musl_compat.c -o "$STATIC_DIR/lib/musl_compat.o"
    cd "$STATIC_DIR/lib"
    ar rcs libmuslcompat.a musl_compat.o
)
echo "  musl 兼容层 OK"

# --------------------------------------------------
# 7. 编译 deb2pkg（全静态链接）
# --------------------------------------------------
echo ""
echo "===== 编译 deb2pkg（全静态）====="
cd "$BUILD_DIR"
rm -rf build && mkdir build && cd build

CC=clang CXX=clang++ cmake "$SCRIPT_DIR" \
    -DCMAKE_CXX_FLAGS="--target=x86_64-linux-musl -O2 -Wall -Wextra -pedantic" \
    -DCMAKE_EXE_LINKER_FLAGS="-L/usr/lib/musl/lib -L${STATIC_DIR}/lib -static  -lmuslcompat -larchive -lz -lbz2 -llzma -lzstd" \
    -DCMAKE_CXX_STANDARD=17 \
    -DCMAKE_PREFIX_PATH="$STATIC_DIR"

make -j$JOBS VERBOSE=1

echo ""
echo "===== 完成 ====="
file "$BUILD_DIR/build/deb2pkg"
echo ""
echo "动态链接检查:"
ldd "$BUILD_DIR/build/deb2pkg" 2>&1 || true
echo ""
echo "产出: $BUILD_DIR/build/deb2pkg"
