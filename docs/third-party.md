# Third-party software

hebOCR is free software under the GNU General Public License, version 3
or later (see `COPYING`). Its source code is at
<https://github.com/aharonium/hebocr>.

hebOCR is built with the following free software. Each is used under its
own license, which allows this use.

**Qt 6** (<https://www.qt.io>), the toolkit of the hebocr-gui program.
Used under the GNU Lesser General Public License, version 3. Qt is
linked dynamically: its libraries are separate files (the `Qt6*.dll`
files and plugin folders in the Windows package), and you may replace
them with your own builds of Qt. Qt's source code is available from
<https://download.qt.io/>. Help ▸ About Qt shows Qt's own notice.

**GLib** (<https://gitlab.gnome.org/GNOME/glib>), used by the hebocr
command-line tool for reading options. Used under the GNU Lesser General
Public License, version 2.1 or later, and linked dynamically.

**LibTIFF** (<https://libtiff.gitlab.io/libtiff/>), for reading TIFF
images. Copyright © 1988–1997 Sam Leffler, © 1991–1997 Silicon Graphics,
Inc. Used under the LibTIFF license, which asks that its copyright and
permission notices appear in documentation; they are included with the
Windows package in `licenses/libtiff/`.

**stb_image** v2.30 by Sean Barrett and contributors
(<https://github.com/nothings/stb>), for reading PNG, JPEG, BMP, GIF and
TGA images; included in hebOCR's source code. Public domain, or MIT
License, at your choice.

## The Windows package

The Windows package also contains libraries that the programs above
depend on, taken from the MSYS2 project (<https://www.msys2.org>): among
them compiler runtime libraries, compression libraries, image format
libraries, and font and text-shaping libraries used by Qt. The file
`THIRD-PARTY-NOTICES.txt` in the package lists every one of them with
its version, home page and license, and the `licenses` folder contains
their license texts.

## The Linux AppImage

The AppImage bundles Qt, GLib, LibTIFF and the libraries they depend on,
taken from Ubuntu 24.04 packages. Inside the AppImage,
`usr/share/doc/hebocr/THIRD-PARTY-NOTICES.txt` lists each package with
its version and home page, and `usr/share/doc/hebocr/licenses` holds
their copyright and license files. (Run the AppImage with
`--appimage-extract` to unpack it and see them.) The bundled libraries
can be replaced in the unpacked folder.

## Earlier contributors

The hebOCR engine was written by Kobi (Yaacov) Zamir, with contributions
from Diego Iastrubni, Oron Peled, Yuval Tanny, Mikael Ylikoski, and Lior
Kaplan and the Debian Hebrew Packaging Team.
