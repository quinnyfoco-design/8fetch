# Maintainer: Quinny <quinny12@web.de>

pkgname=8fetch
pkgver=2
pkgrel=1
pkgdesc="A minimal system information fetch tool"
arch=('any')
url="https://github.com/quinnyfoco-design/8fetch"
license=('AGPL-3.0-or-later')
depends=('bash')
makedepends=('git')
source=("${pkgname}::git+https://github.com/quinnyfoco-design/${pkgname}.git")
sha256sums=('SKIP')

pkgver() {
    cd "${srcdir}/${pkgname}"
    git rev-list --count HEAD
}

package() {
    cd "${srcdir}/${pkgname}"
    install -Dm755 myfetch "${pkgdir}/usr/bin/8fetch"
}
