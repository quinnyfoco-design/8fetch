# Maintainer: Quinny <quinny12@web.de>

pkgname=8fetch
pkgver=1
pkgrel=1
pkgdesc="A minimal system information fetch tool"
arch=('any')
url="https://github.com/quinnyfoco-design/8fetch"
license=('AGPL-3.0-or-later')
depends=('bash')
source=("${pkgname}-${pkgver}.tar.gz::https://github.com/quinnyfoco-design/${pkgname}/archive/refs/tags/v${pkgver}.tar.gz")
sha256sums=('7d5ce46a941bd95cc8b7828233ff7b12a2e9ff41c2c501b7e21d9823acfdecef')

package() {
    cd "${srcdir}/${pkgname}-${pkgver}"
    install -Dm755 8fetch "${pkgdir}/usr/bin/8fetch"
}
