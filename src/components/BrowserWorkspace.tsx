import React, {useCallback, useEffect, useMemo, useRef, useState} from 'react';
import {
  AlertCircle,
  ArrowLeft,
  Check,
  Database,
  Download,
  FilePlus2,
  FolderOpen,
  Info,
  Maximize2,
  Minimize2,
  Redo2,
  RefreshCw,
  Trash2,
  Undo2,
  X,
} from 'lucide-react';
import {WASM_APPLICATIONS} from '../wasm/apps';
import {detectBrowserCapabilities} from '../wasm/capabilities';
import {launchWasmApplication} from '../wasm/runtimeLoader';
import type {
  KritaEngineState,
  WasmDownloadProgress,
  WasmHostSession,
  WasmLaunchStage,
} from '../wasm/types';
import {PWAInstallButton} from './PWAInstallButton';

interface BrowserWorkspaceProps {
  onBackToLanding: () => void;
}

const STAGE_STEPS: {key: WasmLaunchStage; label: string}[] = [
  {key: 'checking', label: 'Verifying WebAssembly & WebGL 2'},
  {key: 'downloading', label: 'Fetching Krita WebAssembly runtime'},
  {key: 'initializing_gl', label: 'Initializing OpenGL ES 3.0 / WebGL 2 context'},
  {key: 'mounting_fs', label: 'Mounting IndexedDB local project filesystem'},
  {key: 'starting', label: 'Bootstrapping KDE Krita pigment & store engine'},
];

function formatMegabytes(bytes: number): string {
  return (bytes / (1024 * 1024)).toFixed(1);
}

export const BrowserWorkspace: React.FC<BrowserWorkspaceProps> = ({onBackToLanding}) => {
  const qtContainerRef = useRef<HTMLDivElement | null>(null);
  const fileInputRef = useRef<HTMLInputElement | null>(null);
  const sessionRef = useRef<WasmHostSession | null>(null);

  const [stage, setStage] = useState<WasmLaunchStage>('checking');
  const [downloadProgress, setDownloadProgress] = useState<WasmDownloadProgress>({
    loadedBytes: 0,
    totalBytes: 0,
    percent: 0,
  });
  const [errorMessage, setErrorMessage] = useState<string | null>(null);
  const [engineState, setEngineState] = useState<KritaEngineState | null>(null);
  const [showInspector, setShowInspector] = useState(false);
  const [isFullscreen, setIsFullscreen] = useState(false);
  const [notice, setNotice] = useState<string | null>(null);
  const [bootCounter, setBootCounter] = useState(0);

  const capabilities = useMemo(() => detectBrowserCapabilities(), []);
  const manifest = WASM_APPLICATIONS.krita;

  const showNotice = useCallback((msg: string) => {
    setNotice(msg);
    setTimeout(() => {
      setNotice((prev) => (prev === msg ? null : prev));
    }, 3500);
  }, []);

  useEffect(() => {
    const container = qtContainerRef.current;
    if (!container) return;

    let cancelled = false;
    setErrorMessage(null);
    setStage('checking');
    setDownloadProgress({loadedBytes: 0, totalBytes: 0, percent: 0});

    launchWasmApplication({
      manifest,
      containerElement: container,
      onStageChange: (nextStage) => {
        if (!cancelled) setStage(nextStage);
      },
      onDownloadProgress: (progress) => {
        if (!cancelled) setDownloadProgress(progress);
      },
      onEngineState: (nextState) => {
        if (!cancelled) setEngineState(nextState);
      },
      onRequestOpenFile: () => {
        fileInputRef.current?.click();
      },
      onExit: (status) => {
        if (cancelled) return;
        if (status.crashed) {
          setStage('error');
          setErrorMessage(
            status.text ||
              'The Krita WebAssembly runtime aborted unexpectedly. Try reloading or reducing canvas dimensions if browser memory is constrained.'
          );
        }
      },
    })
      .then((session) => {
        if (cancelled) {
          session.destroy();
          return;
        }
        sessionRef.current = session;
      })
      .catch((err: unknown) => {
        if (cancelled) return;
        const msg = err instanceof Error ? err.message : String(err);
        setStage('error');
        setErrorMessage(msg);
      });

    return () => {
      cancelled = true;
      sessionRef.current?.destroy();
      sessionRef.current = null;
    };
  }, [manifest, bootCounter]);

  useEffect(() => {
    const onFsChange = () => {
      setIsFullscreen(Boolean(document.fullscreenElement));
    };
    document.addEventListener('fullscreenchange', onFsChange);
    return () => document.removeEventListener('fullscreenchange', onFsChange);
  }, []);

  useEffect(() => {
    const id = window.setTimeout(() => {
      window.dispatchEvent(new Event('resize'));
    }, 30);
    return () => window.clearTimeout(id);
  }, [showInspector]);

  const handleOpenFileChange = async (e: React.ChangeEvent<HTMLInputElement>) => {
    const file = e.target.files?.[0];
    e.target.value = '';
    if (!file || !sessionRef.current) return;

    const ok = await sessionRef.current.openLocalFile(file);
    if (ok) {
      const updated = sessionRef.current.getEngineInfo();
      if (updated) setEngineState(updated);
      showNotice(`Loaded ${file.name} locally into Krita WebAssembly`);
    } else {
      showNotice(`Could not decode ${file.name}. Supported formats: .kra, .png, .jpg, .webp`);
    }
  };

  const handleSaveIndexedDB = async () => {
    if (!sessionRef.current) return;
    await sessionRef.current.syncPersistentStorage();
    showNotice('Flushed /krita-projects to browser IndexedDB storage');
  };

  const handleClearStorage = async () => {
    if (!sessionRef.current) return;
    await sessionRef.current.clearPersistentProjects();
    showNotice('Cleared saved autosave projects from IndexedDB');
  };

  const toggleFullscreen = async () => {
    try {
      if (!document.fullscreenElement) {
        await document.documentElement.requestFullscreen();
      } else {
        await document.exitFullscreen();
      }
    } catch {
      // Ignore fullscreen denial inside embedded iframes
    }
  };

  const currentStageIndex = STAGE_STEPS.findIndex((s) => s.key === stage);

  return (
    <div className="flex h-screen w-screen flex-col overflow-hidden bg-[#15171C] text-slate-100 select-none">
      {/* Compact Application Shell Top Bar */}
      <header
        data-stage={stage}
        data-canvas-size={engineState ? `${engineState.width}x${engineState.height}` : ''}
        className="flex items-center justify-between gap-2 border-b border-slate-800 bg-[#1B1E24] px-3 py-2 text-xs"
      >
        <div className="flex items-center gap-2.5 min-w-0">
          <button
            type="button"
            onClick={onBackToLanding}
            className="inline-flex items-center gap-1.5 rounded-md border border-slate-700 bg-[#232730] px-2.5 py-1.5 font-medium text-slate-200 hover:bg-slate-700 transition-colors whitespace-nowrap shrink-0 cursor-pointer"
          >
            <ArrowLeft className="h-3.5 w-3.5" aria-hidden="true" />
            <span>Overview</span>
          </button>

          <span className="font-display text-sm font-semibold text-white whitespace-nowrap shrink-0">
            Krita WebAssembly
          </span>

          {engineState && (
            <span className="hidden 2xl:inline-block font-mono text-xs text-slate-400 truncate max-w-[280px]">
              {engineState.width}×{engineState.height} px · {engineState.colorSpaceName} · {engineState.layerCount} layers
            </span>
          )}
        </div>

        {/* Browser Host File / Persistence / Quick Actions */}
        <div className="flex items-center gap-1.5 overflow-x-auto shrink-0">
          <input
            ref={fileInputRef}
            type="file"
            accept=".kra,.png,.jpg,.jpeg,.webp,image/*"
            onChange={handleOpenFileChange}
            className="hidden"
          />

          <button
            type="button"
            disabled={stage !== 'ready'}
            onClick={() => sessionRef.current?.newDocument(1280, 800)}
            title="New 1280×800 Canvas"
            className="inline-flex items-center gap-1.5 rounded-md border border-slate-700 bg-[#232730] px-2.5 py-1.5 font-medium text-slate-200 hover:bg-slate-700 disabled:opacity-40 whitespace-nowrap shrink-0 cursor-pointer"
          >
            <FilePlus2 className="h-3.5 w-3.5" aria-hidden="true" />
            <span className="hidden sm:inline">New</span>
          </button>

          <button
            type="button"
            disabled={stage !== 'ready'}
            onClick={() => fileInputRef.current?.click()}
            title="Open local .kra, PNG, JPG, or WebP file"
            className="inline-flex items-center gap-1.5 rounded-md border border-slate-700 bg-[#232730] px-2.5 py-1.5 font-medium text-slate-200 hover:bg-slate-700 disabled:opacity-40 whitespace-nowrap shrink-0 cursor-pointer"
          >
            <FolderOpen className="h-3.5 w-3.5 text-sky-400" aria-hidden="true" />
            <span>Open File</span>
          </button>

          <button
            type="button"
            disabled={stage !== 'ready'}
            onClick={() => sessionRef.current?.exportKra('artwork.kra')}
            title="Download .kra archive"
            className="inline-flex items-center gap-1.5 rounded-md border border-slate-700 bg-[#232730] px-2.5 py-1.5 font-medium text-slate-200 hover:bg-slate-700 disabled:opacity-40 whitespace-nowrap shrink-0 cursor-pointer"
          >
            <Download className="h-3.5 w-3.5" aria-hidden="true" />
            <span>Save .kra</span>
          </button>

          <button
            type="button"
            disabled={stage !== 'ready'}
            onClick={() => sessionRef.current?.exportPng('artwork.png')}
            title="Export flattened PNG"
            className="inline-flex items-center gap-1.5 rounded-md border border-slate-700 bg-[#232730] px-2.5 py-1.5 font-medium text-slate-200 hover:bg-slate-700 disabled:opacity-40 whitespace-nowrap shrink-0 cursor-pointer"
          >
            <Download className="h-3.5 w-3.5" aria-hidden="true" />
            <span className="hidden md:inline">Export PNG</span>
          </button>

          <button
            type="button"
            disabled={stage !== 'ready'}
            onClick={() => sessionRef.current?.undo()}
            aria-label="Undo"
            title="Undo (Ctrl+Z)"
            className="inline-flex items-center rounded-md border border-slate-700 bg-[#232730] p-1.5 text-slate-200 hover:bg-slate-700 disabled:opacity-40 cursor-pointer"
          >
            <Undo2 className="h-3.5 w-3.5" />
          </button>

          <button
            type="button"
            disabled={stage !== 'ready'}
            onClick={() => sessionRef.current?.redo()}
            aria-label="Redo"
            title="Redo (Ctrl+Shift+Z)"
            className="inline-flex items-center rounded-md border border-slate-700 bg-[#232730] p-1.5 text-slate-200 hover:bg-slate-700 disabled:opacity-40 cursor-pointer"
          >
            <Redo2 className="h-3.5 w-3.5" />
          </button>

          <button
            type="button"
            onClick={() => setShowInspector((v) => !v)}
            aria-label="Toggle engine info"
            title="Runtime & Storage Diagnostics"
            className={`inline-flex items-center gap-1.5 rounded-md border px-2.5 py-1.5 font-medium transition-colors whitespace-nowrap shrink-0 cursor-pointer ${
              showInspector
                ? 'border-sky-500/60 bg-sky-600/20 text-sky-200'
                : 'border-slate-700 bg-[#232730] text-slate-200 hover:bg-slate-700'
            }`}
          >
            <Info className="h-3.5 w-3.5" aria-hidden="true" />
            <span className="hidden sm:inline">Engine</span>
          </button>

          <PWAInstallButton variant="compact" />

          <button
            type="button"
            onClick={toggleFullscreen}
            aria-label={isFullscreen ? 'Exit fullscreen' : 'Enter fullscreen'}
            className="inline-flex items-center rounded-md border border-slate-700 bg-[#232730] p-1.5 text-slate-200 hover:bg-slate-700 cursor-pointer"
          >
            {isFullscreen ? <Minimize2 className="h-3.5 w-3.5" /> : <Maximize2 className="h-3.5 w-3.5" />}
          </button>
        </div>
      </header>

      {/* Main Viewport + Optional Diagnostics Drawer */}
      <div className="relative flex flex-1 min-h-0 overflow-hidden">
        {/* Qt 6 WebAssembly Host Container: lock touch-action to avoid browser scrolling during stylus strokes */}
        <div
          ref={qtContainerRef}
          id="qt-krita-container"
          onContextMenu={(e) => e.preventDefault()}
          className="relative h-full w-full flex-1 overflow-hidden bg-[#1D2026] outline-none"
          style={{touchAction: 'none'}}
        />

        {/* Real Loading Overlay */}
        {stage !== 'ready' && stage !== 'error' && (
          <div
            role="status"
            aria-live="polite"
            className="absolute inset-0 z-30 flex items-center justify-center bg-[#0C0D10]/95 p-6 backdrop-blur-sm"
          >
            <div className="w-full max-w-lg rounded-xl border border-slate-800 bg-[#14171D] p-6 shadow-2xl sm:p-8">
              <div className="flex items-center justify-between gap-4">
                <h1 className="font-display text-xl font-bold text-white">Loading Krita</h1>
                <span className="font-mono text-xs text-sky-400">
                  {downloadProgress.totalBytes > 0
                    ? `${formatMegabytes(downloadProgress.loadedBytes)} / ${formatMegabytes(downloadProgress.totalBytes)} MB`
                    : `${downloadProgress.percent}%`}
                </span>
              </div>

              <p className="mt-2 text-sm text-slate-300">
                Initializing official KDE/krita WebAssembly modules and WebGL 2 canvas surface locally in your browser.
              </p>

              <div className="mt-5 h-2 w-full overflow-hidden rounded-full bg-slate-800">
                <div
                  className="h-full bg-sky-500 transition-[width] duration-150"
                  style={{
                    width: `${
                      stage === 'downloading'
                        ? Math.max(8, downloadProgress.percent)
                        : stage === 'checking'
                        ? 5
                        : stage === 'initializing_gl'
                        ? 88
                        : stage === 'mounting_fs'
                        ? 94
                        : 98
                    }%`,
                  }}
                />
              </div>

              <ul className="mt-6 space-y-2.5 text-xs">
                {STAGE_STEPS.map((step, idx) => {
                  const done = currentStageIndex > idx;
                  const active = currentStageIndex === idx;
                  return (
                    <li
                      key={step.key}
                      className={`flex items-center gap-2.5 ${
                        done ? 'text-emerald-400' : active ? 'text-white font-medium' : 'text-slate-500'
                      }`}
                    >
                      {done ? (
                        <Check className="h-3.5 w-3.5 shrink-0" aria-hidden="true" />
                      ) : (
                        <span className="inline-block h-1.5 w-1.5 rounded-full bg-current mx-1 shrink-0" aria-hidden="true" />
                      )}
                      <span>{step.label}</span>
                    </li>
                  );
                })}
              </ul>
            </div>
          </div>
        )}

        {/* Error & Recovery Screen */}
        {stage === 'error' && (
          <div
            role="alert"
            className="absolute inset-0 z-30 flex items-center justify-center bg-[#0C0D10]/95 p-6"
          >
            <div className="w-full max-w-lg rounded-xl border border-rose-500/40 bg-[#16181F] p-6 shadow-2xl sm:p-8">
              <div className="flex items-start gap-3">
                <AlertCircle className="mt-0.5 h-5 w-5 text-rose-400 shrink-0" aria-hidden="true" />
                <div>
                  <h2 className="font-display text-lg font-bold text-white">
                    Unable to Start Krita WebAssembly
                  </h2>
                  <p className="mt-2 text-sm leading-relaxed text-slate-300">
                    {errorMessage || 'An unexpected WebAssembly initialization error occurred.'}
                  </p>
                </div>
              </div>

              <div className="mt-5 rounded-lg border border-slate-800 bg-[#101218] p-4 text-xs text-slate-400 space-y-1">
                <p>WebGL 2 detected: {capabilities.webgl2Supported ? capabilities.glRenderer : 'No'}</p>
                <p>Browser: {capabilities.userAgentSummary}</p>
                <p>
                  Tip: Ensure hardware acceleration is enabled in browser settings and close heavy background tabs if linear memory allocation failed.
                </p>
              </div>

              <div className="mt-6 flex flex-wrap items-center gap-3">
                <button
                  type="button"
                  onClick={() => setBootCounter((c) => c + 1)}
                  className="inline-flex items-center gap-2 rounded-lg bg-sky-600 px-4 py-2 text-xs font-semibold text-white hover:bg-sky-500 transition-colors cursor-pointer"
                >
                  <RefreshCw className="h-3.5 w-3.5" aria-hidden="true" />
                  <span>Retry Launch</span>
                </button>
                <button
                  type="button"
                  onClick={onBackToLanding}
                  className="inline-flex items-center gap-2 rounded-lg border border-slate-700 px-4 py-2 text-xs font-medium text-slate-200 hover:bg-slate-800 transition-colors cursor-pointer"
                >
                  <ArrowLeft className="h-3.5 w-3.5" aria-hidden="true" />
                  <span>Return to Overview</span>
                </button>
              </div>
            </div>
          </div>
        )}

        {/* Engine & Storage Inspector Drawer */}
        {showInspector && (
          <aside
            aria-label="Krita WebAssembly engine inspector"
            className="z-20 w-80 shrink-0 overflow-y-auto border-l border-slate-800 bg-[#14171D] p-4 text-xs"
          >
            <div className="flex items-center justify-between border-b border-slate-800 pb-3">
              <h2 className="font-semibold text-white">Runtime &amp; Storage Inspector</h2>
              <button
                type="button"
                onClick={() => setShowInspector(false)}
                aria-label="Close inspector"
                className="rounded p-1 text-slate-400 hover:bg-slate-800 hover:text-white cursor-pointer"
              >
                <X className="h-4 w-4" />
              </button>
            </div>

            <dl className="mt-4 space-y-2.5">
              <div>
                <dt className="text-slate-400">Upstream Engine Version</dt>
                <dd className="font-mono text-slate-100">{engineState?.kritaVersion ?? manifest.version}</dd>
              </div>
              <div>
                <dt className="text-slate-400">Compiled Pigment Architecture</dt>
                <dd className="font-mono text-slate-100">{engineState?.arch ?? 'wasm32 scalar fallback'}</dd>
              </div>
              <div>
                <dt className="text-slate-400">Active Color Space &amp; Profile</dt>
                <dd className="font-mono text-slate-100">
                  {engineState ? `${engineState.colorSpaceName} (${engineState.profileName})` : 'Pending'}
                </dd>
              </div>
              <div>
                <dt className="text-slate-400">WebGL 2 Renderer</dt>
                <dd className="font-mono text-slate-100 break-words">
                  {engineState?.glRenderer || capabilities.glRenderer}
                </dd>
              </div>
              <div>
                <dt className="text-slate-400">Shortcuts</dt>
                <dd className="mt-1 space-y-1 text-slate-300">
                  <p><kbd className="font-mono text-sky-300">B</kbd> Brush · <kbd className="font-mono text-sky-300">E</kbd> Eraser</p>
                  <p><kbd className="font-mono text-sky-300">[ / ]</kbd> Brush Size · <kbd className="font-mono text-sky-300">Space+Drag</kbd> Pan</p>
                  <p><kbd className="font-mono text-sky-300">Wheel</kbd> Zoom · <kbd className="font-mono text-sky-300">Ctrl+Z</kbd> Undo</p>
                </dd>
              </div>
            </dl>

            <div className="mt-6 space-y-2 border-t border-slate-800 pt-4">
              <button
                type="button"
                disabled={stage !== 'ready'}
                onClick={handleSaveIndexedDB}
                className="flex w-full items-center justify-center gap-2 rounded-md border border-slate-700 bg-[#1D222B] px-3 py-2 font-medium text-slate-200 hover:bg-slate-800 disabled:opacity-40 cursor-pointer"
              >
                <Database className="h-3.5 w-3.5 text-sky-400" aria-hidden="true" />
                <span>Sync IndexedDB Storage</span>
              </button>
              <button
                type="button"
                disabled={stage !== 'ready'}
                onClick={handleClearStorage}
                className="flex w-full items-center justify-center gap-2 rounded-md border border-rose-500/30 bg-rose-500/10 px-3 py-2 font-medium text-rose-200 hover:bg-rose-500/20 disabled:opacity-40 cursor-pointer"
              >
                <Trash2 className="h-3.5 w-3.5" aria-hidden="true" />
                <span>Reset Saved Projects</span>
              </button>
            </div>
          </aside>
        )}

        {/* Transient Toast Notification */}
        {notice && (
          <div
            role="status"
            aria-live="polite"
            className="pointer-events-none absolute bottom-4 right-4 z-30 rounded-lg border border-slate-700 bg-[#181B22]/95 px-4 py-2 text-xs font-medium text-slate-100 shadow-lg"
          >
            {notice}
          </div>
        )}
      </div>
    </div>
  );
};
