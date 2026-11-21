export type WasmLaunchStage =
  | 'idle'
  | 'checking'
  | 'downloading'
  | 'initializing_gl'
  | 'mounting_fs'
  | 'starting'
  | 'ready'
  | 'error';

export interface WasmDownloadProgress {
  loadedBytes: number;
  totalBytes: number;
  percent: number;
}

export interface KritaEngineState {
  kritaVersion: string;
  arch: string;
  width: number;
  height: number;
  colorSpace: string;
  colorSpaceName: string;
  profileName: string;
  layerCount: number;
  undoCount: number;
  webgl: boolean;
  glRenderer: string;
}

export interface BrowserCapabilityReport {
  wasmSupported: boolean;
  wasmStreamingSupported: boolean;
  webgl2Supported: boolean;
  webgl1Supported: boolean;
  glRenderer: string;
  glVendor: string;
  maxTextureSize: number;
  pointerEventsSupported: boolean;
  indexedDbSupported: boolean;
  opfsSupported: boolean;
  sharedArrayBufferSupported: boolean;
  crossOriginIsolated: boolean;
  deviceMemoryGb: number | null;
  hardwareConcurrency: number;
  serviceWorkerSupported: boolean;
  userAgentSummary: string;
}

export interface WasmApplicationManifest {
  id: string;
  name: string;
  version: string;
  license: string;
  qtLoaderPath: string;
  entryScriptPath: string;
  wasmBinaryPath: string;
  entryGlobalName: string;
  persistentDirs: string[];
  estimatedDownloadMb: number;
}

export interface WasmHostSession {
  appId: string;
  openLocalFile: (file: File) => Promise<boolean>;
  exportKra: (filename?: string) => void;
  exportPng: (filename?: string) => void;
  newDocument: (width: number, height: number) => void;
  undo: () => void;
  redo: () => void;
  syncPersistentStorage: () => Promise<void>;
  clearPersistentProjects: () => Promise<void>;
  getEngineInfo: () => KritaEngineState | null;
  destroy: () => void;
}
