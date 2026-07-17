# Maintainer: realstrawhat

pkgname=csakura
pkgver=2.0.0
pkgrel=1
pkgdesc="A sakura tree with falling petals for your terminal (cmatrix-style)"
arch=('x86_64' 'aarch64')
url="https://github.com/realstrawhat/csakura"
license=('MIT')
depends=('ncurses')
makedepends=('git' 'gcc' 'make')
source=("git+${url}.git#branch=main")
sha256sums=('SKIP')

pkgver() {
    cd "${srcdir}/${pkgname}"
    printf "2.0.0.r%s.g%s" "$(git rev-list --count HEAD)" "$(git rev-parse --short HEAD)"
}

build() {
    cd "${srcdir}/${pkgname}"
    make
}

package() {
    cd "${srcdir}/${pkgname}"
    make PREFIX=/usr DESTDIR="${pkgdir}" install
    install -Dm644 LICENSE "${pkgdir}/usr/share/licenses/${pkgname}/LICENSE"
    install -Dm644 README.md "${pkgdir}/usr/share/doc/${pkgname}/README.md"
}
