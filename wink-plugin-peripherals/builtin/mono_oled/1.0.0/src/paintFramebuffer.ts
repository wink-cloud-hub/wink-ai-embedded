const OLED_WIDTH = 128;
const OLED_HEIGHT = 64;
const OLED_FB_BYTES = 1024;
const OLED_PAGE_COUNT = 8;

export interface OledElementLike {
  imageData?: ImageData;
  redraw?: () => void;
  shadowRoot?: ShadowRoot | null;
}

export function paintOledFramebuffer(oledEl: OledElementLike, newFb: Uint8Array | null): void {
  const imgData = new ImageData(OLED_WIDTH, OLED_HEIGHT);
  const px = imgData.data;

  if (newFb && newFb.length === OLED_FB_BYTES) {
    for (let page = 0; page < OLED_PAGE_COUNT; page++) {
      for (let col = 0; col < OLED_WIDTH; col++) {
        const byte = newFb[page * OLED_WIDTH + col]!;
        for (let bit = 0; bit < 8; bit++) {
          const row = page * 8 + bit;
          const lit = (byte >> bit) & 1;
          const idx = (row * OLED_WIDTH + col) * 4;

          px[idx] = lit ? 0 : 8;
          px[idx + 1] = lit ? 210 : 12;
          px[idx + 2] = lit ? 255 : 24;
          px[idx + 3] = 255;
        }
      }
    }
  } else {
    px.fill(0);
    for (let i = 3; i < px.length; i += 4) {
      px[i] = 255;
    }
  }

  oledEl.imageData = imgData;
  if (typeof oledEl.redraw === 'function') {
    oledEl.redraw();
  }

  const shadowCanvas = oledEl.shadowRoot?.querySelector('canvas') as HTMLCanvasElement | null;
  if (shadowCanvas) {
    const ctx = shadowCanvas.getContext('2d');
    ctx?.putImageData(imgData, 0, 0);
  }
}
