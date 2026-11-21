import type {WasmApplicationManifest} from './types';

export const WASM_APPLICATIONS: Record<string, WasmApplicationManifest> = {
  krita: {
    id: 'krita',
    name: 'Krita WebAssembly',
    version: '6.0.0-prealpha (KDE/krita master)',
    license: 'GPL-3.0-or-later',
    qtLoaderPath: '/wasm/qtloader.js',
    entryScriptPath: '/wasm/krita_wasm.js',
    wasmBinaryPath: '/wasm/krita_wasm.wasm',
    entryGlobalName: 'krita_wasm_entry',
    persistentDirs: ['/krita-projects', '/krita-data/config'],
    estimatedDownloadMb: 18,
  },
};
