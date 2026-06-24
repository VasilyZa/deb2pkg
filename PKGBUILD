# Maintainer: deb2pkg developer
# 将 deb/rpm 包转换为 Arch Linux PKGBUILD 的命令行工具

pkgname=deb2pkg
pkgver=1.0.0
pkgrel=1
pkgdesc="将 .deb/.rpm 安装包自动转换为 Arch Linux PKGBUILD 文件的工具"
arch=(x86_64)
license=(MIT)
url="https://github.com/local/deb2pkg"
depends=('libarchive' 'binutils' 'gcc-libs')
makedepends=('cmake' 'clang')
optdepends=('pkgfile: 自动匹配库文件对应的 Arch 包名'
            'base-devel: 调用 makepkg 构建安装包')
source=("deb2pkg-${pkgver}.tar.gz")
sha256sums=('SKIP')

build() {
  cd "${srcdir}/deb2pkg-${pkgver}"
  mkdir -p build && cd build
  cmake .. \
    -DCMAKE_CXX_COMPILER=clang++ \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr
  make -j$(nproc)
}

package() {
  cd "${srcdir}/deb2pkg-${pkgver}"

  # 安装可执行文件
  install -Dm755 build/deb2pkg "${pkgdir}/usr/bin/deb2pkg"

  # 安装文档
  install -Dm644 README.md "${pkgdir}/usr/share/doc/${pkgname}/README.md"

  # 安装许可证
  install -Dm644 LICENSE "${pkgdir}/usr/share/licenses/${pkgname}/LICENSE"
}
