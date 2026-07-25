# Maintainer: realstrawhat <realstrawhat@users.noreply.github.com>

pkgname=csakura
pkgver=2.0.0
pkgrel=1
pkgdesc="A sakura tree with falling petals for your terminal (cmatrix-style)"
arch=('x86_64' 'aarch64')
url="https://github.com/realstrawhat/csakura"
license=('MIT')
depends=('ncurses')
source=("$pkgname-$pkgver.tar.gz::$url/archive/refs/tags/v$pkgver.tar.gz")
# Fill in with `updpkgsums` once the v2.0.0 tag is pushed.
sha256sums=('0000000000000000000000000000000000000000000000000000000000000000')

build() {
    cd "$pkgname-$pkgver"
    make
}

package() {
    cd "$pkgname-$pkgver"
    make PREFIX=/usr DESTDIR="$pkgdir" install
    install -Dm644 LICENSE "$pkgdir/usr/share/licenses/$pkgname/LICENSE"
    install -Dm644 README.md "$pkgdir/usr/share/doc/$pkgname/README.md"
}
