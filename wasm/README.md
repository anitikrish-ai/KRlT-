# Krita WebAssembly Build & Architecture Documentation

This directory compiles real C++ subsystems from the official [KDE/krita](https://github.com/KDE/krita) repository into a browser-executable WebAssembly application powered by **Qt 6.8.3 for WebAssembly**, **Emscripten 3.1.56**, **OpenGL ES 3.0 / WebGL 2**, **LittleCMS 2**, **QuaZip**, **zlib**, and **IndexedDB (`IDBFS`)**.

## 1. Upstream Krita Subsystems Compiled to WebAssembly

The target `krita_wasm` (`wasm/CMakeLists.txt`) compiles and statically links the following official KDE/krita C++ libraries directly from `/opt/krita`:

| Krita Library | Upstream Path | Purpose in Browser Build |
| :--- | :--- | :--- |
| `kritaversion` | `libs/version` | Upstream `KritaVersionWrapper` version & build identification |
| `kritaglobal` | `libs/global` | Math/2D algebra (`KisAlgebra2D`), Bezier/Region utilities, debug/assert macros, `KoID`, `KoUnit` |
| `kritamultiarch` | `libs/multiarch` | `KisSupportedArchitectures` & `xsimd` universal scalar architecture dispatch (`wasm32`) |
| `kritaplugin` | `libs/koplugin` | `KoPluginLoader`, `KoJsonTrader`, `KisMimeDatabase` |
| `kritastore` | `libs/store` | `KoStore` / `KoQuaZipStore` `.kra` ZIP archive read/write & XML writer |
| `kritacommand` | `libs/command` | `KUndo2Stack`, `KUndo2View`, `KUndo2Command` undo/redo engine |
| `kritaresources` | `libs/resources` | SQLite-backed resource models, tags, storage bundles, resource locator |
| `kritapigment` | `libs/pigment` | `KoColorSpaceRegistry`, `KoColor`, RGBA8, RGBA16, CIE L*a*b* 16-bit (`lcms2`), `KoColorSpace::bitBlt` blend ops |

### Compatibility Layers & Minimal Upstream Patch
To keep native Windows/Linux/macOS/Android desktop builds intact without modifying Krita's desktop architecture, browser shims live under `wasm/compat/`:
- **`wasm/compat/kf/`**: Lightweight `QSettings`-backed implementations of `KConfig`, `KConfigGroup`, `KSharedConfig`, `KLocalizedString` (`i18n`/`ki18n`), and `KPluginFactory`.
- **`wasm/compat/widgetutils/`**: Minimal `KoIcon`, `KActionCollection`, `KStandardAction`, and `KStandardShortcut` adapters for `kritacommand`.
- **`wasm/compat/generated/`**: Static export headers (`*_export.h`) and Krita `config-*.h` headers matching WebAssembly capabilities (`HAVE_LCMS2=1`, `HAVE_GSL` undefined).
- **`wasm/krita-wasm-fixes.patch`**: Guards GSL-only helper templates (`my_f`, `my_fdf`, `my_df`) inside `libs/global/KisBezierUtils.cpp` with `#ifdef HAVE_GSL` (upstream already guarded `calculateLocalPosImpl` with `#ifdef HAVE_GSL` and an `#else return approxStart;` fallback), and updates string literal concatenations in `libs/pigment/resources/KoColorSet.cpp` for Qt 6 strict `QStringView` compatibility.

## 2. Desktop-Only Subsystems Excluded (and Blockers)

The following desktop Krita modules are intentionally not compiled into the single-binary browser runtime and are clearly separated in the UI:
1. **Full `kritaui` Desktop Workspace & Dockers**: Depends heavily on unported KDE desktop integrations (`KXmlGui`, `KCompletion`, `DBus`, native X11/Wayland/Win32 windowing, and desktop-patched Qt).
2. **`kritaimage` Advanced Filter/Node Graph**: Pulls in desktop multi-threaded tile scheduler pools, OCIO/OpenEXR/FFTW, and Fontconfig OS font enumeration.
3. **Dynamic Native Plugins (`plugins/*`)**: Browser WASM sandboxes cannot dynamically `dlopen` arbitrary native `.so`/`.dll` plugins at runtime without pre-linked Emscripten side modules.
4. **Python Scripting (`PyQt` / `SIP`), G'MIC-Qt, MLT/FFmpeg Video Export, Native Printing (`PrintSupport`)**: Require native OS subprocesses, Python C-API wheels, or hardware drivers unavailable in standard browser sandboxes.

## 3. Toolchain & Reproducible Build Steps

1. **Emscripten SDK**: `3.1.56` (matching Qt 6.8.3 WASM toolchain requirement) in `/opt/emsdk`.
2. **Qt 6.8.3**: Host tools (`gcc_64`) + `wasm_singlethread` + `qt5compat`, `qtimageformats`, `qtshadertools`, `qtsvg`, `qtdeclarative` in `/opt/qt/6.8.3`.
3. **Cross-compiled Static Dependencies** (`/opt/wasm-deps/install`):
   - `zlib` (`libz.a`), `LittleCMS 2` (`liblcms2.a`), `QuaZip` (`libquazip1-qt6.a`)
   - Header-only: `Boost`, `Eigen3`, `immer`, `zug`, `lager`, `xsimd`
4. **Build & Publish**:
   ```bash
   bash wasm/build.sh
   ```
   Outputs `public/wasm/qtloader.js`, `public/wasm/krita_wasm.js`, and `public/wasm/krita_wasm.wasm`.

## 4. Browser Runtime, Storage & Deployment Notes

- **Single-Thread Build (`wasm_singlethread`)**: Chosen so the application executes reliably on standard static hosts and embedded iframes without mandatory `Cross-Origin-Opener-Policy: same-origin` / `Cross-Origin-Embedder-Policy: require-corp` (`SharedArrayBuffer`) headers. Switching Qt prefix to `wasm_multithread` enables pthreads when COOP/COEP headers are served.
- **Persistence**: Emscripten `IDBFS` mounts `/krita-projects` and `/krita-data/config` to browser IndexedDB before `main()` starts, allowing `.kra` projects and `QSettings` preferences to persist locally across page reloads with zero server upload.
- **Static Hosting**: `public/_headers` and `vercel.json` configure `Content-Type: application/wasm` and rewrite `/browser` to `/index.html`.
- **PWA Offline Caching**: `vite-plugin-pwa` caches `krita_wasm.wasm`, `krita_wasm.js`, `qtloader.js`, and all UI assets locally for offline usage.
