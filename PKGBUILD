# Maintainer: Albert Tang <tangalbert919@yahoo.com>

pkgbase=gigabyte-laptop
pkgname=gigabyte-laptop-dkms
pkgver=0.0.3
pkgrel=1
pkgdesc="Kernel module allowing more control on Gigabyte laptops"
arch=('x86_64')
url="https://www.github.com/tangalbert919/gigabyte-laptop-wmi"
license=('GPL2')
depends=('dkms')
source=("${url}/releases/download/${pkgver}/driver.tar.gz")
sha256sums=("$(curl -sL ${url}/releases/download/${pkgver}/sum.txt | cut -d ' ' -f1)")

package() {
  # Set name and version
  sed -e "s/@PKGVER@/${pkgver}/" -i dkms.conf
  install -Dt "${pkgdir}/usr/src/${pkgbase}-${pkgver}" -m644 Makefile aorus-laptop.c dkms.conf
}