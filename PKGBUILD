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
sha256sums=('8ae5bfa9e10e0aae9bdbc46bd8998faa189ed3338e54856c66c8f6f02c1f3748')

package() {
    cd "${srcdir}/${pkgname}-${pkgver}"
    install -Dm755 myfetch "${pkgdir}/usr/bin/8fetch"
}
