<script setup lang="ts">
import { computed } from 'vue';

const props = withDefaults(
  defineProps<{
    pinConnections?: Record<string, unknown>;
    variant?: string;
    color?: string;
    brightness?: number;
    glow?: boolean;
    label?: string;
    flip?: boolean;
    bright?: Uint8Array | null;
    segMask?: number[];
    text?: string;
    nDigits?: number;
  }>(),
  {
    variant: 'direct_gpio_8d',
    color: 'red',
    brightness: 1.0,
    glow: true,
    label: '',
    flip: false,
    bright: null,
    segMask: () => [],
    text: '',
    nDigits: 8,
  },
);

const COLOR_TABLE: Record<string, { lit: string; dim: string; glow: string }> = {
  red: { lit: '#ff2828', dim: '#380c0c', glow: '#ff2222' },
  green: { lit: '#00ff55', dim: '#053310', glow: '#00ff55' },
  blue: { lit: '#2299ff', dim: '#082038', glow: '#0088ff' },
  yellow: { lit: '#ffee00', dim: '#383300', glow: '#ffee00' },
  white: { lit: '#f5f5f5', dim: '#282828', glow: '#ffffff' },
  orange: { lit: '#ff8800', dim: '#381c00', glow: '#ff7700' },
  purple: { lit: '#cc33ff', dim: '#2b0838', glow: '#bb11ff' },
};

const activePalette = computed(() => {
  const c = props.color?.toLowerCase() ?? 'red';
  return COLOR_TABLE[c] ?? COLOR_TABLE.red;
});

const effectiveNDigits = computed(() => {
  if (props.nDigits && props.nDigits > 0) return props.nDigits;
  if (props.variant === 'direct_gpio_4d') return 4;
  if (props.variant === 'direct_gpio_2d') return 2;
  if (props.variant === 'direct_gpio_1d') return 1;
  return 8;
});

interface DigitLayout {
  x: number;
  y: number;
  scaleX: number;
  scaleY: number;
}

const digitLayouts = computed<DigitLayout[]>(() => {
  const n = effectiveNDigits.value;
  const layouts: DigitLayout[] = [];

  // Available window inside 210 x 96: width 190, height 70, centered
  const totalW = 190;
  const startY = 22;

  let digitW = 20;
  let gap = 4;
  let scaleY = 1.35;

  if (n === 8) {
    digitW = 19;
    gap = 4.5;
    scaleY = 1.35;
  } else if (n === 4) {
    digitW = 34;
    gap = 14;
    scaleY = 1.45;
  } else if (n === 2) {
    digitW = 56;
    gap = 26;
    scaleY = 1.5;
  } else if (n === 1) {
    digitW = 75;
    gap = 0;
    scaleY = 1.55;
  }

  const occupiedW = n * digitW + (n - 1) * gap;
  const startX = 10 + (totalW - occupiedW) / 2;
  const scaleX = digitW / 26;

  for (let i = 0; i < n; i++) {
    layouts.push({
      x: startX + i * (digitW + gap),
      y: startY,
      scaleX,
      scaleY,
    });
  }

  return layouts;
});

// Segment paths on standard 26 x 40 canvas
const SEGMENTS = [
  { id: 'a', points: '3,2  17,2  15,5  5,5' },
  { id: 'b', points: '17.5,3.5  19.5,5.5  19.5,17  17.5,19  15.5,17  15.5,5.5' },
  { id: 'c', points: '17.5,21  19.5,23  19.5,34.5  17.5,36.5  15.5,34.5  15.5,23' },
  { id: 'd', points: '5,35  15,35  17,38  3,38' },
  { id: 'e', points: '2.5,21  4.5,23  4.5,34.5  2.5,36.5  0.5,34.5  0.5,23' },
  { id: 'f', points: '2.5,3.5  4.5,5.5  4.5,17  2.5,19  0.5,17  0.5,5.5' },
  { id: 'g', points: '3.5,19.5  5,17.5  15,17.5  16.5,19.5  15,21.5  5,21.5' },
];

function getSegmentOpacity(digitIndex: number, segIndex: number): number {
  const brightArr = props.bright;
  const b = brightArr && brightArr.length > digitIndex * 8 + segIndex
    ? brightArr[digitIndex * 8 + segIndex]
    : 0;

  const userScale = Math.max(0, Math.min(1, props.brightness));
  // Inactive segments maintain a faint visible outline (~0.10 opacity)
  const norm = b / 255;
  return Math.min(1.0, 0.10 + norm * 0.90 * userScale);
}

function isSegmentLit(digitIndex: number, segIndex: number): boolean {
  const brightArr = props.bright;
  const b = brightArr && brightArr.length > digitIndex * 8 + segIndex
    ? brightArr[digitIndex * 8 + segIndex]
    : 0;
  return b >= 40;
}
</script>

<template>
  <svg
    class="seg-display-canvas"
    viewBox="0 0 210 96"
    width="210"
    height="96"
    role="img"
    :aria-label="text ? `Display: ${text}` : label || '7-Segment Display'"
    :style="flip ? 'transform: rotate(180deg);' : undefined"
  >
    <defs>
      <!-- Single glow filter for all lit segments -->
      <filter id="seg-glow-filter" x="-20%" y="-20%" width="140%" height="140%">
        <feGaussianBlur stdDeviation="1.8" result="blur" />
        <feMerge>
          <feMergeNode in="blur" />
          <feMergeNode in="SourceGraphic" />
        </feMerge>
      </filter>
    </defs>

    <!-- Outer PCB / module bezel -->
    <rect
      x="1"
      y="1"
      width="208"
      height="94"
      rx="6"
      ry="6"
      fill="#141416"
      stroke="#2c2d33"
      stroke-width="1.5"
    />

    <!-- Inner display recess window -->
    <rect
      x="8"
      y="12"
      width="194"
      height="72"
      rx="3"
      ry="3"
      fill="#090a0c"
      stroke="#1c1d22"
      stroke-width="1"
    />

    <!-- Digit render groups -->
    <g
      v-for="(layout, d) in digitLayouts"
      :key="d"
      class="seg-digit"
      :transform="`translate(${layout.x}, ${layout.y}) scale(${layout.scaleX}, ${layout.scaleY}) skewX(-7)`"
    >
      <!-- 7 Segments A..G -->
      <polygon
        v-for="(seg, s) in SEGMENTS"
        :key="seg.id"
        :points="seg.points"
        :fill="isSegmentLit(d, s) ? activePalette.lit : activePalette.dim"
        :fill-opacity="getSegmentOpacity(d, s)"
        :filter="glow && isSegmentLit(d, s) ? 'url(#seg-glow-filter)' : undefined"
      />

      <!-- Decimal Point DP (segment 7) -->
      <circle
        cx="23"
        cy="37"
        r="1.8"
        :fill="isSegmentLit(d, 7) ? activePalette.lit : activePalette.dim"
        :fill-opacity="getSegmentOpacity(d, 7)"
        :filter="glow && isSegmentLit(d, 7) ? 'url(#seg-glow-filter)' : undefined"
      />
    </g>

    <!-- Optional Label -->
    <text
      v-if="label"
      x="105"
      y="80"
      text-anchor="middle"
      fill="#888890"
      font-size="7"
      font-family="monospace"
    >
      {{ label }}
    </text>
  </svg>
</template>

<style scoped>
.seg-display-canvas {
  display: block;
  user-select: none;
  overflow: visible;
}
</style>
