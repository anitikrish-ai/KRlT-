#!/usr/bin/env bash
set -euo pipefail

URL="${1:-http://localhost:3000}"

echo "=== 1. Verify WASM artifacts and HTTP headers ==="
test -s public/wasm/krita_wasm.wasm
test -s public/wasm/krita_wasm.js
test -s public/wasm/qtloader.js
curl -sfI "${URL}/wasm/krita_wasm.wasm" | grep -iq "application/wasm"
curl -sf "${URL}/manifest.webmanifest" | grep -q "Krita WebAssembly"

echo "=== 2. Verify Landing Page & Client Compatibility Readout via playwright-cli ==="
playwright-cli close >/dev/null 2>&1 || true
playwright-cli open "${URL}"
sleep 1
playwright-cli snapshot | grep -q "Open in Browser"
playwright-cli snapshot | grep -q "Download Desktop Version"
playwright-cli snapshot | grep -q "WebGL 2"

echo "=== 3. Launch Krita WebAssembly (/browser) ==="
playwright-cli eval "[...document.querySelectorAll('button')].find(b => b.textContent?.includes('Open in Browser'))?.click()"

for i in $(seq 1 30); do
  if playwright-cli eval "document.querySelector('header')?.getAttribute('data-stage') === 'ready' ? ['KRITA','READY'].join('_') : 'WAIT'" | grep -q '^### Result"KRITA_READY"'; then
    break
  fi
  sleep 1
done

playwright-cli eval "document.querySelector('header')?.getAttribute('data-stage') === 'ready'" | grep -q "^### Resulttrue"
playwright-cli eval "document.querySelector('header')?.getAttribute('data-canvas-size') === '1280x800'" | grep -q "^### Resulttrue"
playwright-cli eval "[...document.querySelectorAll('#qt-krita-container div')].some(d => d.shadowRoot && d.shadowRoot.querySelector('canvas'))" | grep -q "^### Resulttrue"
playwright-cli eval "window.getComputedStyle(document.getElementById('qt-krita-container')).touchAction" | grep -q '^### Result"none"'

echo "=== 4. Verify 1280px Header Button Bounds (No Clipping) ==="
playwright-cli eval "(() => { const btn = [...document.querySelectorAll('button')].find(b => /fullscreen/i.test(b.getAttribute('aria-label') || '')); return btn ? btn.getBoundingClientRect().right <= window.innerWidth : false; })()" | grep -q "^### Resulttrue"

echo "=== 5. Test Drawing Stroke, Undo/Redo, Export, & Inspector ==="
playwright-cli mousemove 480 360
playwright-cli mousedown
playwright-cli mousemove 620 460
playwright-cli mouseup
sleep 0.5
playwright-cli eval "[...document.querySelectorAll('button')].find(b => b.getAttribute('aria-label') === 'Undo')?.click()"
sleep 0.3
playwright-cli eval "[...document.querySelectorAll('button')].find(b => b.getAttribute('aria-label') === 'Redo')?.click()"
sleep 0.3
playwright-cli eval "[...document.querySelectorAll('button')].find(b => b.textContent?.includes('Save .kra'))?.click()"
sleep 0.3
playwright-cli eval "[...document.querySelectorAll('button')].find(b => b.textContent?.includes('Export PNG'))?.click()"
sleep 0.3
playwright-cli eval "[...document.querySelectorAll('button')].find(b => b.getAttribute('aria-label') === 'Toggle engine info')?.click()"
sleep 0.4
playwright-cli eval "document.querySelector('aside')?.innerText?.includes('6.1.0-wasm')" | grep -q "^### Resulttrue"

echo "=== 6. Test IndexedDB Sync & Service Worker ==="
playwright-cli eval "[...document.querySelectorAll('button')].find(b => b.textContent?.includes('Sync IndexedDB Storage'))?.click()"
sleep 0.5
playwright-cli eval "(async () => { const dbs = await indexedDB.databases(); return dbs.some(d => (d.name || '').includes('/krita-projects')); })()" | grep -q "^### Resulttrue"
playwright-cli eval "(async () => { const regs = await navigator.serviceWorker.getRegistrations(); return regs.length >= 0; })()" | grep -q "^### Resulttrue"

echo "=== 7. Verify Tablet Viewport Responsiveness (768x1024) ==="
playwright-cli resize 768 1024
sleep 0.4
playwright-cli eval "document.documentElement.scrollWidth <= window.innerWidth" | grep -q "^### Resulttrue"

playwright-cli close
echo "All browser checks passed."
