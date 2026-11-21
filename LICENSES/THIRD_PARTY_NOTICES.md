# Third-Party Licenses & Notices

This browser application compiles and links unmodified C++ source libraries from **KDE Krita** alongside open-source WebAssembly runtime dependencies. All original copyright headers and license terms are preserved.

## 1. KDE Krita Core Libraries (`libs/version`, `libs/global`, `libs/multiarch`, `libs/koplugin`, `libs/store`, `libs/command`, `libs/resources`, `libs/pigment`)
- **Upstream Repository**: https://github.com/KDE/krita
- **License**: GNU General Public License v3.0 or later (`GPL-3.0-or-later`) and GNU Lesser General Public License v2.0/v2.1 (`LGPL-2.0-or-later`, `LGPL-2.1-or-later`) per file SPDX headers.
- **Copyright**: KDE / Krita Foundation contributors (1999–2026).

## 2. Qt 6 for WebAssembly (`QtCore`, `QtGui`, `QtWidgets`, `QtOpenGL`, `QtOpenGLWidgets`, `QtXml`, `QtSql`, `QtSvg`, `QtCore5Compat`)
- **License**: GNU Lesser General Public License v3 (`LGPL-3.0`) / GPL-3.0.
- **Copyright**: The Qt Company Ltd.

## 3. Little CMS 2 (`liblcms2`)
- **License**: MIT License.
- **Copyright**: Marti Maria Saguer.

## 4. QuaZip (`libquazip1-qt6`)
- **License**: LGPL-2.1-or-later (with static linking exception / GPL-compatible).
- **Copyright**: Sergey A. Tachenov and QuaZip contributors.

## 5. zlib (`libz`)
- **License**: zlib License.
- **Copyright**: Jean-loup Gailly and Mark Adler.

## 6. Header-Only C++ Libraries (`Boost`, `Eigen3`, `immer`, `zug`, `lager`, `xsimd`)
- **Boost**: Boost Software License 1.0.
- **Eigen3**: Mozilla Public License 2.0 (`MPL-2.0`).
- **immer / zug / lager**: Boost Software License 1.0 (`BSL-1.0`).
- **xsimd**: BSD-3-Clause License.

## 7. Emscripten Runtime & Browser Web Shell
- **Emscripten**: MIT / University of Illinois/NCSA Open Source License.
- **Web Application Shell**: Apache-2.0 / GPL-3.0 compatible distribution.
