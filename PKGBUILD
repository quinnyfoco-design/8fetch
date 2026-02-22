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
sha256sums=('f744134ebe86e6edd852175349247aef4e2e235b8c8ef35eff189474445888dc')

package() {
    cd "${srcdir}/${pkgname}-${pkgver}"
    install -Dm755 myfetch "${pkgdir}/usr/bin/8fetch"
}
