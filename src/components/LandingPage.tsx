import React, {useMemo, useState} from 'react';
import {
  ArrowRight,
  CheckCircle2,
  Cpu,
  Download,
  ExternalLink,
  HardDrive,
  Layers,
  Menu,
  Monitor,
  Palette,
  ShieldCheck,
  X,
  AlertTriangle,
} from 'lucide-react';
import {detectBrowserCapabilities} from '../wasm/capabilities';
import {PWAInstallButton} from './PWAInstallButton';

interface LandingPageProps {
  onOpenBrowser: () => void;
}

interface MediaFrameProps {
  src: string;
  alt: string;
  caption: string;
  className?: string;
}

const MediaFrame: React.FC<MediaFrameProps> = ({src, alt, caption, className = ''}) => {
  const [failed, setFailed] = useState(false);

  return (
    <figure className={`relative overflow-hidden rounded-xl border border-slate-800/80 bg-[#13161C] ${className}`}>
      {!failed ? (
        <img
          src={src}
          alt={alt}
          referrerPolicy="no-referrer"
          onError={() => setFailed(true)}
          className="h-full w-full object-cover"
        />
      ) : (
        <div className="flex h-full min-h-[240px] w-full flex-col items-center justify-center bg-gradient-to-br from-slate-900 via-[#151A24] to-slate-950 p-6 text-center">
          <Palette className="h-9 w-9 text-sky-400/80" aria-hidden="true" />
          <p className="mt-3 text-sm font-medium text-slate-300">{caption}</p>
        </div>
      )}
      <figcaption className="border-t border-slate-800/80 bg-[#11141A]/95 px-4 py-2.5 text-xs text-slate-400">
        {caption}
      </figcaption>
    </figure>
  );
};

export const LandingPage: React.FC<LandingPageProps> = ({onOpenBrowser}) => {
  const [mobileMenuOpen, setMobileMenuOpen] = useState(false);
  const capabilities = useMemo(() => detectBrowserCapabilities(), []);

  const webglReady = capabilities.wasmSupported && capabilities.webgl2Supported;

  return (
    <div className="min-h-screen bg-[#0C0D10] text-slate-100">
      {/* Top Bar Contract: 3 zones (Wordmark — 4 Nav Links — 1 Primary Action) */}
      <header className="sticky top-0 z-40 border-b border-slate-800/80 bg-[#0C0D10]/90 backdrop-blur-md">
        <div className="mx-auto flex max-w-[1200px] items-center justify-between gap-8 px-6 py-4">
          <a
            href="#top"
            className="text-lg font-bold tracking-tight text-white whitespace-nowrap shrink-0"
          >
            Krita WebAssembly
          </a>

          <nav aria-label="Primary navigation" className="hidden md:flex items-center gap-7 text-sm font-medium text-slate-300">
            <a href="#architecture" className="hover:text-white hover:underline underline-offset-4 transition-colors whitespace-nowrap shrink-0">
              Architecture
            </a>
            <a href="#compatibility" className="hover:text-white hover:underline underline-offset-4 transition-colors whitespace-nowrap shrink-0">
              Compatibility
            </a>
            <a href="#comparison" className="hover:text-white hover:underline underline-offset-4 transition-colors whitespace-nowrap shrink-0">
              Desktop vs Web
            </a>
            <a href="#licensing" className="hover:text-white hover:underline underline-offset-4 transition-colors whitespace-nowrap shrink-0">
              Licensing
            </a>
          </nav>

          <div className="flex items-center gap-3 shrink-0">
            <button
              type="button"
              onClick={onOpenBrowser}
              className="inline-flex items-center gap-2 rounded-lg bg-sky-600 px-4 py-2 text-xs sm:text-sm font-semibold text-white hover:bg-sky-500 transition-colors whitespace-nowrap shrink-0 cursor-pointer"
            >
              <span>Open in Browser</span>
              <ArrowRight className="h-4 w-4" aria-hidden="true" />
            </button>
            <button
              type="button"
              onClick={() => setMobileMenuOpen((v) => !v)}
              aria-label={mobileMenuOpen ? 'Close navigation menu' : 'Open navigation menu'}
              className="inline-flex items-center justify-center rounded-lg border border-slate-800 p-2 text-slate-300 hover:bg-slate-800 md:hidden cursor-pointer"
            >
              {mobileMenuOpen ? <X className="h-4 w-4" /> : <Menu className="h-4 w-4" />}
            </button>
          </div>
        </div>

        {mobileMenuOpen && (
          <nav
            aria-label="Mobile navigation"
            className="border-t border-slate-800 bg-[#111318] px-6 py-4 md:hidden"
          >
            <div className="flex flex-col gap-3 text-sm font-medium text-slate-300">
              <a href="#architecture" onClick={() => setMobileMenuOpen(false)} className="hover:text-white">
                Architecture
              </a>
              <a href="#compatibility" onClick={() => setMobileMenuOpen(false)} className="hover:text-white">
                Compatibility
              </a>
              <a href="#comparison" onClick={() => setMobileMenuOpen(false)} className="hover:text-white">
                Desktop vs Web
              </a>
              <a href="#licensing" onClick={() => setMobileMenuOpen(false)} className="hover:text-white">
                Licensing
              </a>
            </div>
          </nav>
        )}
      </header>

      <main id="top">
        {/* Hero Section */}
        <section className="border-b border-slate-800/70">
          <div className="mx-auto grid max-w-[1200px] grid-cols-1 items-stretch gap-10 px-6 py-14 lg:grid-cols-12 lg:py-20">
            <div className="flex flex-col justify-between gap-8 lg:col-span-6">
              <div>
                <div className="flex flex-wrap items-center gap-2 text-xs font-medium text-slate-400">
                  <span>KDE Krita Source</span>
                  <span aria-hidden="true">·</span>
                  <span>Emscripten WebAssembly</span>
                  <span aria-hidden="true">·</span>
                  <span>Qt 6.8.3 &amp; WebGL 2</span>
                </div>

                <h1
                  className="mt-4 font-display text-3xl font-bold tracking-tight text-white sm:text-5xl lg:text-[3.1rem] lg:leading-[1.08]"
                  style={{textWrap: 'balance'}}
                >
                  Krita&apos;s C++ Color &amp; Layer Engine, Compiled to WebAssembly.
                </h1>

                <p className="mt-5 max-w-[62ch] text-base leading-relaxed text-slate-300">
                  Launch a native WebAssembly compilation of official KDE Krita C++ core libraries directly inside your browser. Paint with pressure- and tilt-aware stylus input, blend layers through <code className="font-mono text-sm text-sky-300">KoColorSpace::bitBlt</code> and LittleCMS 2 color management, and read or export <code className="font-mono text-sm text-sky-300">.kra</code> archives locally without uploading artwork to a server.
                </p>
              </div>

              <div className="space-y-4">
                <div className="flex flex-wrap items-center gap-3">
                  <button
                    type="button"
                    onClick={onOpenBrowser}
                    className="inline-flex items-center gap-2.5 rounded-lg bg-sky-600 px-6 py-3 text-sm font-semibold text-white shadow-sm hover:bg-sky-500 transition-colors whitespace-nowrap shrink-0 cursor-pointer"
                  >
                    <span>Open in Browser</span>
                    <ArrowRight className="h-4 w-4" aria-hidden="true" />
                  </button>

                  <a
                    href="https://krita.org/en/download/"
                    target="_blank"
                    rel="noreferrer"
                    className="inline-flex items-center gap-2 rounded-lg border border-slate-700 bg-[#14171D] px-5 py-3 text-sm font-medium text-slate-200 hover:bg-slate-800 transition-colors whitespace-nowrap shrink-0"
                  >
                    <Download className="h-4 w-4 text-slate-400" aria-hidden="true" />
                    <span>Download Desktop Version</span>
                    <ExternalLink className="h-3.5 w-3.5 text-slate-500" aria-hidden="true" />
                  </a>

                  <PWAInstallButton />
                </div>

                <div className="flex flex-wrap items-center gap-x-5 gap-y-1 text-xs text-slate-400">
                  <span>Runs 100% locally in browser sandbox</span>
                  <span aria-hidden="true">·</span>
                  <span>IndexedDB project persistence</span>
                  <span aria-hidden="true">·</span>
                  <span>Offline PWA capable</span>
                </div>
              </div>
            </div>

            <div className="lg:col-span-6">
              <MediaFrame
                src="/src/assets/images/hero_krita_studio_1795237310717.jpg"
                alt="Digital illustration studio workstation running Krita painting tools on a pen display"
                caption="Digital painting workflow backed by KDE Krita pigment color spaces, layered .kra ZIP storage, and WebGL 2 canvas presentation."
                className="h-full min-h-[280px] lg:min-h-[420px]"
              />
            </div>
          </div>
        </section>

        {/* Architecture & Compiled Subsystems */}
        <section id="architecture" className="border-b border-slate-800/70 bg-[#0F1116]">
          <div className="mx-auto max-w-[1200px] px-6 py-16">
            <div className="max-w-2xl">
              <h2 className="font-display text-2xl font-bold tracking-tight text-white sm:text-3xl">
                Real C++ Krita Subsystems in WebAssembly
              </h2>
              <p className="mt-3 text-sm leading-relaxed text-slate-300">
                Rather than re-implementing a toy canvas in JavaScript, this build cross-compiles upstream C++ source modules from <code className="font-mono text-sky-300">KDE/krita</code> with Emscripten 3.1.56 and links them statically with Qt 6.8.3 for WebAssembly.
              </p>
            </div>

            <div className="mt-10 grid grid-cols-1 gap-6 lg:grid-cols-12">
              <div className="rounded-xl border border-slate-800/80 bg-[#14171E] p-6 lg:col-span-7">
                <div className="grid grid-cols-1 gap-6 sm:grid-cols-2">
                  <div>
                    <h3 className="text-base font-semibold text-white">01. Pigment Color Management</h3>
                    <p className="mt-2 text-sm leading-relaxed text-slate-300">
                      Compiles <code className="font-mono text-xs text-sky-300">kritapigment</code> with LittleCMS 2 static libraries, exposing <code className="font-mono text-xs text-slate-200">KoColorSpaceRegistry</code> across 8-bit RGBA, 16-bit RGBA, and 16-bit CIE L*a*b* pixel buffers plus <code className="font-mono text-xs text-slate-200">bitBlt</code> blend modes.
                    </p>
                  </div>

                  <div>
                    <h3 className="text-base font-semibold text-white">02. Native .kra ZIP Store</h3>
                    <p className="mt-2 text-sm leading-relaxed text-slate-300">
                      Compiles <code className="font-mono text-xs text-sky-300">kritastore</code> backed by static QuaZip and zlib, reading and packaging <code className="font-mono text-xs text-slate-200">maindoc.xml</code>, <code className="font-mono text-xs text-slate-200">mergedimage.png</code>, and layer pixel streams inside local IndexedDB or downloadable <code className="font-mono text-xs text-slate-200">.kra</code> archives.
                    </p>
                  </div>

                  <div>
                    <h3 className="text-base font-semibold text-white">03. KUndo2 History &amp; Core Foundation</h3>
                    <p className="mt-2 text-sm leading-relaxed text-slate-300">
                      Links <code className="font-mono text-xs text-sky-300">kritacommand</code>, <code className="font-mono text-xs text-slate-200">kritaglobal</code>, <code className="font-mono text-xs text-slate-200">kritaresources</code>, <code className="font-mono text-xs text-slate-200">kritaplugin</code>, and <code className="font-mono text-xs text-slate-200">kritamultiarch</code> using a lightweight Qt6 <code className="font-mono text-xs text-slate-200">QSettings</code> compatibility layer for KDE Frameworks.
                    </p>
                  </div>

                  <div>
                    <h3 className="text-base font-semibold text-white">04. WebGL 2 &amp; Tablet Pointer Input</h3>
                    <p className="mt-2 text-sm leading-relaxed text-slate-300">
                      Presents composited artwork inside Qt&apos;s <code className="font-mono text-xs text-sky-300">QOpenGLWidget</code> over OpenGL ES 3.0 / WebGL 2 with <code className="font-mono text-xs text-slate-200">QTabletEvent</code> pressure and tilt telemetry and locked canvas <code className="font-mono text-xs text-slate-200">touch-action: none</code>.
                    </p>
                  </div>
                </div>
              </div>

              <div className="grid grid-cols-1 gap-6 sm:grid-cols-2 lg:col-span-5 lg:grid-cols-1">
                <MediaFrame
                  src="/src/assets/images/showcase_brush_engines_1795237310306.jpg"
                  alt="Digital painting brush strokes showing pressure variation and smooth pigment blending"
                  caption="Pressure-sensitive dab stamping composited via Krita KoColorSpace::bitBlt"
                  className="h-52 lg:h-48"
                />
                <MediaFrame
                  src="/src/assets/images/showcase_layer_illustration_1795237310671.jpg"
                  alt="Layered character illustration organized in non-destructive blend stacks"
                  caption="Multi-layer blend modes (Normal, Multiply, Screen, Overlay, Addition, Darken, Lighten)"
                  className="h-52 lg:h-48"
                />
              </div>
            </div>
          </div>
        </section>

        {/* System Requirements & Live Browser Compatibility */}
        <section id="compatibility" className="border-b border-slate-800/70">
          <div className="mx-auto max-w-[1200px] px-6 py-16">
            <div className="grid grid-cols-1 gap-10 lg:grid-cols-12">
              <div className="lg:col-span-5">
                <h2 className="font-display text-2xl font-bold tracking-tight text-white sm:text-3xl">
                  System &amp; Browser Readiness
                </h2>
                <p className="mt-3 text-sm leading-relaxed text-slate-300">
                  All diagnostics on the right are probed live in your browser session. Krita WebAssembly ships as a single-threaded Emscripten build so it works in standard static hosts and cross-origin iframes without requiring special COOP/COEP headers, while offline Service Worker caching prevents re-downloading the runtime binary.
                </p>

                <div className="mt-6 space-y-4 border-t border-slate-800 pt-6 text-sm">
                  <div className="flex items-start gap-3">
                    <Cpu className="mt-0.5 h-4 w-4 text-sky-400 shrink-0" aria-hidden="true" />
                    <div>
                      <p className="font-medium text-white">Memory &amp; First-Load Footprint</p>
                      <p className="mt-1 text-slate-400">
                        Initial heap allocates 128 MB and grows dynamically on large canvases. Recommended minimum 4 GB system RAM; ~18 MB WebAssembly binary cached locally after first visit.
                      </p>
                    </div>
                  </div>

                  <div className="flex items-start gap-3">
                    <HardDrive className="mt-0.5 h-4 w-4 text-sky-400 shrink-0" aria-hidden="true" />
                    <div>
                      <p className="font-medium text-white">Local-Only File &amp; Project Storage</p>
                      <p className="mt-1 text-slate-400">
                        Opened files load into Emscripten virtual memory. Saved projects flush to browser IndexedDB (<code className="font-mono text-xs text-slate-300">/krita-projects</code>) with zero server transfer.
                      </p>
                    </div>
                  </div>
                </div>
              </div>

              <div className="lg:col-span-7">
                <div className="overflow-hidden rounded-xl border border-slate-800 bg-[#12151B]">
                  <div className="flex items-center justify-between border-b border-slate-800 px-5 py-3.5">
                    <span className="text-sm font-semibold text-white">
                      Detected Client Environment ({capabilities.userAgentSummary})
                    </span>
                    <span className={`inline-flex items-center gap-1.5 text-xs font-medium ${webglReady ? 'text-emerald-400' : 'text-amber-300'}`}>
                      {webglReady ? (
                        <>
                          <CheckCircle2 className="h-3.5 w-3.5" aria-hidden="true" />
                          <span>Ready to Launch</span>
                        </>
                      ) : (
                        <>
                          <AlertTriangle className="h-3.5 w-3.5" aria-hidden="true" />
                          <span>WebGL 2 Required</span>
                        </>
                      )}
                    </span>
                  </div>

                  <dl className="divide-y divide-slate-800/80 text-sm">
                    <div className="flex items-center justify-between gap-4 px-5 py-3">
                      <dt className="text-slate-400">WebAssembly Runtime</dt>
                      <dd className="font-mono text-xs text-slate-100">
                        {capabilities.wasmSupported
                          ? capabilities.wasmStreamingSupported
                            ? 'Supported (Streaming Compile)'
                            : 'Supported (Buffer Compile)'
                          : 'Unsupported'}
                      </dd>
                    </div>
                    <div className="flex items-center justify-between gap-4 px-5 py-3">
                      <dt className="text-slate-400">WebGL 2 / OpenGL ES 3.0</dt>
                      <dd className="font-mono text-xs text-slate-100 text-right truncate max-w-[320px]" title={capabilities.glRenderer}>
                        {capabilities.webgl2Supported
                          ? `${capabilities.glRenderer} (Max ${capabilities.maxTextureSize}px)`
                          : capabilities.webgl1Supported
                          ? 'WebGL 1 only (WebGL 2 missing)'
                          : 'Unavailable'}
                      </dd>
                    </div>
                    <div className="flex items-center justify-between gap-4 px-5 py-3">
                      <dt className="text-slate-400">Stylus / Pointer Pressure &amp; Tilt</dt>
                      <dd className="font-mono text-xs text-slate-100">
                        {capabilities.pointerEventsSupported ? 'PointerEvents API Active' : 'Mouse / Touch Fallback'}
                      </dd>
                    </div>
                    <div className="flex items-center justify-between gap-4 px-5 py-3">
                      <dt className="text-slate-400">Persistent Browser Storage</dt>
                      <dd className="font-mono text-xs text-slate-100">
                        {capabilities.indexedDbSupported ? 'IndexedDB (IDBFS)' : 'Unavailable'}
                        {capabilities.opfsSupported ? ' + OPFS' : ''}
                      </dd>
                    </div>
                    <div className="flex items-center justify-between gap-4 px-5 py-3">
                      <dt className="text-slate-400">Execution Threading</dt>
                      <dd className="font-mono text-xs text-slate-100">
                        {capabilities.crossOriginIsolated && capabilities.sharedArrayBufferSupported
                          ? `COOP/COEP Isolated (${capabilities.hardwareConcurrency} cores)`
                          : `Single-Thread Safe (${capabilities.hardwareConcurrency} cores detected)`}
                      </dd>
                    </div>
                    <div className="flex items-center justify-between gap-4 px-5 py-3">
                      <dt className="text-slate-400">Reported Device Memory &amp; Offline Cache</dt>
                      <dd className="font-mono text-xs text-slate-100">
                        {capabilities.deviceMemoryGb ? `${capabilities.deviceMemoryGb} GB RAM` : 'Memory API restricted'} ·{' '}
                        {capabilities.serviceWorkerSupported ? 'Service Worker Ready' : 'No SW'}
                      </dd>
                    </div>
                  </dl>
                </div>
              </div>
            </div>
          </div>
        </section>

        {/* Honest Comparison: Browser WebAssembly Edition vs Native Desktop Krita */}
        <section id="comparison" className="border-b border-slate-800/70 bg-[#0F1116]">
          <div className="mx-auto max-w-[1200px] px-6 py-16">
            <div className="max-w-2xl">
              <h2 className="font-display text-2xl font-bold tracking-tight text-white sm:text-3xl">
                Browser Edition Scope vs. Native Desktop Krita
              </h2>
              <p className="mt-3 text-sm leading-relaxed text-slate-300">
                Desktop Krita relies on native OS shared plugins, Python/PyQt scripting, Fontconfig, FFmpeg/MLT, and unported KDE desktop libraries. Below is the exact boundary between what runs natively in WebAssembly here and what requires the desktop release.
              </p>
            </div>

            <div className="mt-8 overflow-x-auto rounded-xl border border-slate-800 bg-[#12151B]">
              <table className="w-full min-w-[680px] text-left text-sm">
                <thead className="border-b border-slate-800 bg-[#161A22] text-xs font-semibold text-slate-300">
                  <tr>
                    <th className="px-5 py-3.5">Subsystem / Capability</th>
                    <th className="px-5 py-3.5">Krita WebAssembly (Browser)</th>
                    <th className="px-5 py-3.5">Native Desktop Krita</th>
                  </tr>
                </thead>
                <tbody className="divide-y divide-slate-800/80 text-slate-300">
                  <tr>
                    <td className="px-5 py-3.5 font-medium text-white">Pigment Color Engine (LCMS2)</td>
                    <td className="px-5 py-3.5">Compiled C++ <code className="font-mono text-xs text-sky-300">kritapigment</code> (RGBA8, RGBA16, LABA16, bitBlt)</td>
                    <td className="px-5 py-3.5">Full CPU SIMD multi-arch + CMYK/XYZ/OCIO</td>
                  </tr>
                  <tr>
                    <td className="px-5 py-3.5 font-medium text-white">Document Store (.kra / PNG / JPG)</td>
                    <td className="px-5 py-3.5">Compiled <code className="font-mono text-xs text-sky-300">kritastore</code> (QuaZip + zlib) + Browser File API</td>
                    <td className="px-5 py-3.5">Full native OS filesystem + PSD / EXR / HEIF / TIFF</td>
                  </tr>
                  <tr>
                    <td className="px-5 py-3.5 font-medium text-white">Undo History &amp; Canvas Viewport</td>
                    <td className="px-5 py-3.5">Compiled <code className="font-mono text-xs text-sky-300">kritacommand</code> + Qt 6.8 WebGL 2 + QTabletEvent</td>
                    <td className="px-5 py-3.5">Full `kritaui` workspace, OpenGL/Vulkan, 4K+ tiling</td>
                  </tr>
                  <tr>
                    <td className="px-5 py-3.5 font-medium text-white">Brush Engines &amp; Filters</td>
                    <td className="px-5 py-3.5">Core pressure/hardness dab engine composited through KoColorSpace</td>
                    <td className="px-5 py-3.5">16+ full brush engines, G&apos;MIC filters, vector/text tools</td>
                  </tr>
                  <tr>
                    <td className="px-5 py-3.5 font-medium text-white">Scripting, Video &amp; Printing</td>
                    <td className="px-5 py-3.5 text-slate-400">Not supported in browser sandbox (requires PyQt / MLT / OS print)</td>
                    <td className="px-5 py-3.5">Python scripting, animation export, native printing</td>
                  </tr>
                </tbody>
              </table>
            </div>
          </div>
        </section>

        {/* Licensing & Open Source Attribution */}
        <section id="licensing" className="border-b border-slate-800/70">
          <div className="mx-auto max-w-[1200px] px-6 py-16">
            <div className="grid grid-cols-1 gap-8 rounded-xl border border-slate-800 bg-[#12151B] p-6 sm:p-8 lg:grid-cols-12">
              <div className="lg:col-span-8">
                <div className="flex items-center gap-2.5 text-sm font-semibold text-white">
                  <ShieldCheck className="h-4 w-4 text-sky-400" aria-hidden="true" />
                  <span>Open-Source Licensing &amp; Privacy Notice</span>
                </div>
                <p className="mt-3 text-sm leading-relaxed text-slate-300">
                  Krita is free and open-source software by the KDE community, licensed under the GNU General Public License v3.0 or later (<code className="font-mono text-xs text-slate-200">GPL-3.0-or-later</code>). This WebAssembly distribution links Qt 6.8.3 (GPL/LGPLv3), LittleCMS 2 (MIT), QuaZip (LGPL-2.1), zlib, Boost, Eigen3, and Emscripten runtime components. Artwork opened or painted in this application remains exclusively inside your local browser memory and IndexedDB storage—zero analytics, trackers, or remote file uploads are used.
                </p>
              </div>
              <div className="flex flex-col justify-center gap-3 lg:col-span-4 lg:items-end">
                <button
                  type="button"
                  onClick={onOpenBrowser}
                  className="inline-flex items-center justify-center gap-2 rounded-lg bg-sky-600 px-5 py-2.5 text-sm font-semibold text-white hover:bg-sky-500 transition-colors whitespace-nowrap shrink-0 cursor-pointer"
                >
                  <Layers className="h-4 w-4" aria-hidden="true" />
                  <span>Launch Browser Build</span>
                </button>
                <a
                  href="https://github.com/KDE/krita"
                  target="_blank"
                  rel="noreferrer"
                  className="inline-flex items-center justify-center gap-2 rounded-lg border border-slate-700 px-4 py-2 text-xs font-medium text-slate-300 hover:bg-slate-800 transition-colors whitespace-nowrap shrink-0"
                >
                  <Monitor className="h-3.5 w-3.5 text-slate-400" aria-hidden="true" />
                  <span>Upstream KDE/krita Repository</span>
                  <ExternalLink className="h-3 w-3 text-slate-500" aria-hidden="true" />
                </a>
              </div>
            </div>
          </div>
        </section>
      </main>

      <footer className="bg-[#0C0D10]">
        <div className="mx-auto flex max-w-[1200px] flex-col gap-4 px-6 py-8 text-xs text-slate-400 sm:flex-row sm:items-center sm:justify-between">
          <div>
            <span className="font-semibold text-slate-200">Krita WebAssembly</span>
            <span className="mx-2" aria-hidden="true">·</span>
            <span>Built from official KDE/krita C++ sources · GPL-3.0-or-later</span>
          </div>
          <div className="flex flex-wrap items-center gap-5">
            <a href="#architecture" className="hover:text-white">Architecture</a>
            <a href="#compatibility" className="hover:text-white">Compatibility</a>
            <a href="https://krita.org/en/download/" target="_blank" rel="noreferrer" className="hover:text-white">
              Desktop Krita
            </a>
            <a href="https://github.com/KDE/krita" target="_blank" rel="noreferrer" className="hover:text-white">
              Source Code
            </a>
          </div>
        </div>
      </footer>
    </div>
  );
};
