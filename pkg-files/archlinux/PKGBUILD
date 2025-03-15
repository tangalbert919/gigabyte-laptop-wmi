# Maintainer: Albert Tang <tangalbert919@yahoo.com>

pkgbase=aorus-laptop
pkgname=aorus-laptop-dkms
pkgver=0.1.0_rc1
pkgrel=1
pkgdesc="Kernel module allowing more control on Gigabyte Aero/AORUS laptops"
arch=('x86_64')
url="https://www.github.com/tangalbert919/gigabyte-laptop-wmi"
license=('GPL2')
depends=('dkms')
replaces=('gigabyte-laptop-dkms')
source=("${url}/releases/download/${pkgver//_/-}/driver.tar.gz")
sha256sums=("$(curl -sL ${url}/releases/download/${pkgver//_/-}/sum.txt | cut -d ' ' -f1)")

package() {
  # Set name and version
  sed -e "s/@PKGVER@/${pkgver//_/-}/" -i dkms.conf
  install -Dt "${pkgdir}/usr/src/${pkgbase}-${pkgver//_/-}" -m644 Makefile aorus-laptop.c dkms.conf
}