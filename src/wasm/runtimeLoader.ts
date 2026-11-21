import type {
  KritaEngineState,
  WasmApplicationManifest,
  WasmDownloadProgress,
  WasmHostSession,
  WasmLaunchStage,
} from './types';

interface EmscriptenFS {
  mkdir: (path: string) => void;
  mount: (type: unknown, opts: Record<string, unknown>, mountpoint: string) => void;
  syncfs: (populate: boolean, callback: (err: unknown) => void) => void;
  writeFile: (path: string, data: Uint8Array) => void;
  readFile: (path: string) => Uint8Array;
  unlink: (path: string) => void;
  readdir: (path: string) => string[];
  stat: (path: string) => {mode: number};
  isFile: (mode: number) => boolean;
}

interface EmscriptenQtInstance {
  FS: EmscriptenFS;
  IDBFS: unknown;
  ccall: (ident: string, returnType: string | null, argTypes: string[], args: unknown[]) => unknown;
  cwrap: (ident: string, returnType: string | null, argTypes: string[]) => (...args: unknown[]) => unknown;
  callMain: (args: string[]) => void;
  abort?: (msg?: string) => void;
}

declare global {
  interface Window {
    qtLoad?: (config: Record<string, unknown>) => Promise<EmscriptenQtInstance>;
    krita_wasm_entry?: (config: Record<string, unknown>) => Promise<EmscriptenQtInstance>;
    __kritaWasmBridgeEvent?: (eventType: string, payloadJson: string) => void;
  }
}

export interface LaunchWasmOptions {
  manifest: WasmApplicationManifest;
  containerElement: HTMLElement;
  onStageChange: (stage: WasmLaunchStage) => void;
  onDownloadProgress: (progress: WasmDownloadProgress) => void;
  onEngineState: (state: KritaEngineState) => void;
  onRequestOpenFile: () => void;
  onExit: (reason: {code?: number; text?: string; crashed: boolean}) => void;
}

const loadedScripts = new Set<string>();

function loadScriptOnce(src: string): Promise<void> {
  if (loadedScripts.has(src)) {
    return Promise.resolve();
  }
  return new Promise((resolve, reject) => {
    const existing = document.querySelector<HTMLScriptElement>(`script[data-wasm-src="${src}"]`);
    if (existing && existing.dataset.loaded === 'true') {
      loadedScripts.add(src);
      resolve();
      return;
    }
    const script = document.createElement('script');
    script.src = src;
    script.async = true;
    script.dataset.wasmSrc = src;
    script.onload = () => {
      script.dataset.loaded = 'true';
      loadedScripts.add(src);
      resolve();
    };
    script.onerror = () => {
      script.remove();
      reject(new Error(`Failed to load runtime script: ${src}`));
    };
    document.head.appendChild(script);
  });
}

async function fetchAndCompileWasm(
  wasmUrl: string,
  onProgress: (p: WasmDownloadProgress) => void
): Promise<WebAssembly.Module> {
  const response = await fetch(wasmUrl, {cache: 'no-cache'});
  if (!response.ok) {
    throw new Error(`Unable to fetch WebAssembly binary (${response.status} ${response.statusText})`);
  }

  const contentLengthHeader = response.headers.get('content-length');
  const totalBytes = contentLengthHeader ? Number.parseInt(contentLengthHeader, 10) : 0;

  if (!response.body) {
    const buffer = await response.arrayBuffer();
    onProgress({
      loadedBytes: buffer.byteLength,
      totalBytes: buffer.byteLength,
      percent: 100,
    });
    return WebAssembly.compile(buffer);
  }

  const reader = response.body.getReader();
  const chunks: Uint8Array[] = [];
  let loadedBytes = 0;

  while (true) {
    const {done, value} = await reader.read();
    if (done) break;
    if (value) {
      chunks.push(value);
      loadedBytes += value.byteLength;
      const effectiveTotal = totalBytes > 0 ? totalBytes : Math.max(loadedBytes, 18 * 1024 * 1024);
      onProgress({
        loadedBytes,
        totalBytes: totalBytes > 0 ? totalBytes : loadedBytes,
        percent: Math.min(99, Math.round((loadedBytes / effectiveTotal) * 100)),
      });
    }
  }

  const combined = new Uint8Array(loadedBytes);
  let offset = 0;
  for (const chunk of chunks) {
    combined.set(chunk, offset);
    offset += chunk.byteLength;
  }

  onProgress({
    loadedBytes,
    totalBytes: loadedBytes,
    percent: 100,
  });

  return WebAssembly.compile(combined.buffer);
}

function triggerBrowserDownload(bytes: Uint8Array, filename: string, mimeType: string): void {
  const copy = new Uint8Array(bytes.byteLength);
  copy.set(bytes);
  const blob = new Blob([copy.buffer], {type: mimeType});
  const url = URL.createObjectURL(blob);
  const link = document.createElement('a');
  link.href = url;
  link.download = filename;
  link.style.display = 'none';
  document.body.appendChild(link);
  link.click();
  document.body.removeChild(link);
  setTimeout(() => URL.revokeObjectURL(url), 1000);
}

function ensureDir(FS: EmscriptenFS, dirPath: string): void {
  const parts = dirPath.split('/').filter(Boolean);
  let current = '';
  for (const part of parts) {
    current += '/' + part;
    try {
      FS.mkdir(current);
    } catch {
      // Directory already exists
    }
  }
}

export async function launchWasmApplication(options: LaunchWasmOptions): Promise<WasmHostSession> {
  const {
    manifest,
    containerElement,
    onStageChange,
    onDownloadProgress,
    onEngineState,
    onRequestOpenFile,
    onExit,
  } = options;

  onStageChange('checking');

  if (typeof WebAssembly !== 'object') {
    throw new Error('WebAssembly is not supported by this browser.');
  }

  // Probe WebGL 2 availability before allocating large WASM linear memory
  const probeCanvas = document.createElement('canvas');
  const gl2 = probeCanvas.getContext('webgl2');
  if (!gl2) {
    throw new Error('WebGL 2.0 context could not be initialized. Enable hardware acceleration or update your graphics drivers.');
  }
  gl2.getExtension('WEBGL_lose_context')?.loseContext();

  onStageChange('downloading');

  const [, , compiledModule] = await Promise.all([
    loadScriptOnce(manifest.qtLoaderPath),
    loadScriptOnce(manifest.entryScriptPath),
    fetchAndCompileWasm(manifest.wasmBinaryPath, onDownloadProgress),
  ]);

  if (typeof window.qtLoad !== 'function') {
    throw new Error('Qt WebAssembly loader (qtLoad) failed to register.');
  }

  const entryFn = (window as unknown as Record<string, unknown>)[manifest.entryGlobalName];
  if (typeof entryFn !== 'function') {
    throw new Error(`Emscripten module entry "${manifest.entryGlobalName}" was not found.`);
  }

  onStageChange('initializing_gl');

  let instance: EmscriptenQtInstance | null = null;
  let readyReceived = false;

  const syncIdbfs = async (populate: boolean): Promise<void> => {
    if (!instance) return;
    await new Promise<void>((resolve) => {
      try {
        instance!.FS.syncfs(populate, () => resolve());
      } catch {
        resolve();
      }
    });
  };

  window.__kritaWasmBridgeEvent = (eventType: string, payloadJson: string) => {
    let payload: Record<string, unknown> = {};
    try {
      payload = JSON.parse(payloadJson || '{}') as Record<string, unknown>;
    } catch {
      payload = {};
    }

    if (eventType === 'ready') {
      readyReceived = true;
      onStageChange('ready');
      onEngineState(payload as unknown as KritaEngineState);
    } else if (eventType === 'state') {
      onEngineState(payload as unknown as KritaEngineState);
    } else if (eventType === 'request_open_file') {
      onRequestOpenFile();
    } else if (eventType === 'download_file') {
      const path = String(payload.path || '');
      const filename = String(payload.filename || 'artwork.kra');
      if (instance && path) {
        try {
          const data = instance.FS.readFile(path);
          const mime = filename.endsWith('.png') ? 'image/png' : 'application/x-krita';
          triggerBrowserDownload(data, filename, mime);
        } catch (err) {
          console.error('Failed to read exported file from Emscripten VFS', err);
        }
      }
    } else if (eventType === 'sync_idbfs') {
      void syncIdbfs(false);
    }
  };

  instance = await window.qtLoad({
    noInitialRun: true,
    preRun: [
      (mod: EmscriptenQtInstance) => {
        ensureDir(mod.FS, '/tmp');
        for (const dir of manifest.persistentDirs) {
          ensureDir(mod.FS, dir);
          try {
            mod.FS.mount(mod.IDBFS, {}, dir);
          } catch {
            // Ignore mount collision on restart
          }
        }
      },
    ],
    qt: {
      entryFunction: entryFn,
      containerElements: [containerElement],
      module: Promise.resolve(compiledModule),
      onLoaded: () => {
        // Qt runtime initialized; main() will emit 'ready'
      },
      onExit: (status: {code?: number; text?: string; crashed: boolean}) => {
        onExit(status);
      },
    },
  });

  onStageChange('mounting_fs');
  await syncIdbfs(true);

  onStageChange('starting');
  try {
    instance.callMain([]);
  } catch (err) {
    // Emscripten throws 'unwind' intentionally when Qt app.exec() hands control to the browser event loop
    if (err !== 'unwind') {
      throw err;
    }
  }

  if (!readyReceived) {
    // Query engine state directly in case main() emitted synchronously
    try {
      const jsonStr = instance.ccall('krita_wasm_get_engine_info', 'string', [], []) as string;
      const parsed = JSON.parse(jsonStr) as KritaEngineState;
      onEngineState(parsed);
    } catch {
      // Ignore if not yet ready
    }
    onStageChange('ready');
  }

  const getEngineInfo = (): KritaEngineState | null => {
    if (!instance) return null;
    try {
      const jsonStr = instance.ccall('krita_wasm_get_engine_info', 'string', [], []) as string;
      return JSON.parse(jsonStr) as KritaEngineState;
    } catch {
      return null;
    }
  };

  return {
    appId: manifest.id,
    openLocalFile: async (file: File) => {
      if (!instance) return false;
      const safeName = file.name.replace(/[^a-zA-Z0-9._-]/g, '_');
      const vfsPath = `/tmp/${Date.now()}_${safeName}`;
      const buffer = new Uint8Array(await file.arrayBuffer());
      instance.FS.writeFile(vfsPath, buffer);
      const ok = Number(instance.ccall('krita_wasm_open_file', 'number', ['string'], [vfsPath])) === 1;
      return ok;
    },
    exportKra: (filename = 'artwork.kra') => {
      if (!instance) return;
      const vfsPath = '/tmp/export_shell.kra';
      const ok = Number(instance.ccall('krita_wasm_export_kra', 'number', ['string'], [vfsPath])) === 1;
      if (ok) {
        const bytes = instance.FS.readFile(vfsPath);
        triggerBrowserDownload(bytes, filename, 'application/x-krita');
      }
    },
    exportPng: (filename = 'artwork.png') => {
      if (!instance) return;
      const vfsPath = '/tmp/export_shell.png';
      const ok = Number(instance.ccall('krita_wasm_export_png', 'number', ['string'], [vfsPath])) === 1;
      if (ok) {
        const bytes = instance.FS.readFile(vfsPath);
        triggerBrowserDownload(bytes, filename, 'image/png');
      }
    },
    newDocument: (width: number, height: number) => {
      if (!instance) return;
      instance.ccall('krita_wasm_new_document', null, ['number', 'number'], [width, height]);
    },
    undo: () => {
      instance?.ccall('krita_wasm_undo', null, [], []);
    },
    redo: () => {
      instance?.ccall('krita_wasm_redo', null, [], []);
    },
    syncPersistentStorage: () => syncIdbfs(false),
    clearPersistentProjects: async () => {
      if (!instance) return;
      try {
        const files = instance.FS.readdir('/krita-projects');
        for (const f of files) {
          if (f === '.' || f === '..') continue;
          const full = `/krita-projects/${f}`;
          const st = instance.FS.stat(full);
          if (instance.FS.isFile(st.mode)) {
            instance.FS.unlink(full);
          }
        }
        await syncIdbfs(false);
      } catch {
        // Ignore missing directory
      }
    },
    getEngineInfo,
    destroy: () => {
      window.__kritaWasmBridgeEvent = undefined;
      containerElement.innerHTML = '';
      instance = null;
    },
  };
}
