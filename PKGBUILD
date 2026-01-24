# Maintainer: Quinny <quinny12@web.de>

pkgname=8fetch
pkgver=2
pkgrel=1
pkgdesc="A minimal system information fetch tool"
arch=('any')
url="https://github.com/quinnyfoco-design/8fetch"
license=('AGPL-3.0-or-later')
depends=('bash')
source=("${pkgname}-${pkgver}.tar.gz::https://github.com/quinnyfoco-design/${pkgname}/archive/refs/tags/v${pkgver}.tar.gz")
sha256sums=('53626dec9f98a075ec0c377be6b966288cfbf44cf3168f4cbec750f076b1037c')

package() {
    cd "${srcdir}/${pkgname}-${pkgver}"
    install -Dm755 myfetch "${pkgdir}/usr/bin/8fetch"
}
