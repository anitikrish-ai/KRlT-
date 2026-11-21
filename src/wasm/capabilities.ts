import type {BrowserCapabilityReport} from './types';

function summarizeBrowser(ua: string): string {
  if (/Edg\//.test(ua)) return 'Microsoft Edge';
  if (/Chrome\//.test(ua) && !/Chromium\//.test(ua)) return 'Google Chrome';
  if (/Firefox\//.test(ua)) return 'Mozilla Firefox';
  if (/Safari\//.test(ua) && !/Chrome\//.test(ua)) return 'Apple Safari';
  return 'Web Browser';
}

export function detectBrowserCapabilities(): BrowserCapabilityReport {
  const wasmSupported = typeof WebAssembly === 'object' && typeof WebAssembly.instantiate === 'function';
  const wasmStreamingSupported = wasmSupported && typeof WebAssembly.compileStreaming === 'function';

  let webgl2Supported = false;
  let webgl1Supported = false;
  let glRenderer = 'Unavailable';
  let glVendor = 'Unavailable';
  let maxTextureSize = 0;

  if (typeof document !== 'undefined') {
    const canvas = document.createElement('canvas');
    const gl2 = canvas.getContext('webgl2', {
      alpha: false,
      antialias: false,
      powerPreference: 'high-performance',
    }) as WebGL2RenderingContext | null;

    if (gl2) {
      webgl2Supported = true;
      webgl1Supported = true;
      maxTextureSize = Number(gl2.getParameter(gl2.MAX_TEXTURE_SIZE) || 0);
      const dbg = gl2.getExtension('WEBGL_debug_renderer_info');
      if (dbg) {
        glRenderer = String(gl2.getParameter(dbg.UNMASKED_RENDERER_WEBGL) || gl2.getParameter(gl2.RENDERER));
        glVendor = String(gl2.getParameter(dbg.UNMASKED_VENDOR_WEBGL) || gl2.getParameter(gl2.VENDOR));
      } else {
        glRenderer = String(gl2.getParameter(gl2.RENDERER) || 'WebGL 2.0');
        glVendor = String(gl2.getParameter(gl2.VENDOR) || 'Browser WebGL');
      }
      const loseCtx = gl2.getExtension('WEBGL_lose_context');
      loseCtx?.loseContext();
    } else {
      const gl1 = canvas.getContext('webgl') as WebGLRenderingContext | null;
      if (gl1) {
        webgl1Supported = true;
        maxTextureSize = Number(gl1.getParameter(gl1.MAX_TEXTURE_SIZE) || 0);
        glRenderer = String(gl1.getParameter(gl1.RENDERER) || 'WebGL 1.0');
        glVendor = String(gl1.getParameter(gl1.VENDOR) || 'Browser WebGL');
        const loseCtx = gl1.getExtension('WEBGL_lose_context');
        loseCtx?.loseContext();
      }
    }
  }

  const nav = typeof navigator !== 'undefined' ? navigator : undefined;
  const storage = nav?.storage as (StorageManager & {getDirectory?: () => Promise<FileSystemDirectoryHandle>}) | undefined;

  return {
    wasmSupported,
    wasmStreamingSupported,
    webgl2Supported,
    webgl1Supported,
    glRenderer,
    glVendor,
    maxTextureSize,
    pointerEventsSupported: typeof window !== 'undefined' && 'PointerEvent' in window,
    indexedDbSupported: typeof window !== 'undefined' && 'indexedDB' in window,
    opfsSupported: Boolean(storage && typeof storage.getDirectory === 'function'),
    sharedArrayBufferSupported: typeof SharedArrayBuffer !== 'undefined',
    crossOriginIsolated: typeof window !== 'undefined' && Boolean(window.crossOriginIsolated),
    deviceMemoryGb: nav && 'deviceMemory' in nav ? Number((nav as Navigator & {deviceMemory?: number}).deviceMemory) : null,
    hardwareConcurrency: nav?.hardwareConcurrency ?? 2,
    serviceWorkerSupported: Boolean(nav && 'serviceWorker' in nav),
    userAgentSummary: summarizeBrowser(nav?.userAgent ?? ''),
  };
}
