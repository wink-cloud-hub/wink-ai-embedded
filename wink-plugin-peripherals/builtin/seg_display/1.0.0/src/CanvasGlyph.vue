<script setup lang="ts">
import { computed, ref, watchEffect } from 'vue';
import '@wokwi/elements';

const props = withDefaults(
  defineProps<{
    pinConnections?: Record<string, unknown>;
    variant?: string;
    color?: string;
    brightness?: number;
    glow?: boolean;
    label?: string;
    flip?: boolean;
    bright?: Uint8Array | number[] | null;
    segMask?: number[] | string | null;
    text?: string;
    nDigits?: number;
    values?: number[];
  }>(),
  {
    variant: 'direct_gpio_4d',
    color: 'red',
    brightness: 1.0,
    glow: true,
    label: '',
    flip: false,
    bright: null,
    segMask: () => [],
    text: '',
    nDigits: 4,
    values: () => [],
  },
);

const segEl = ref<any>(null);

const effectiveNDigits = computed(() => {
  if (props.nDigits && props.nDigits > 0) return props.nDigits;
  if (props.variant === 'direct_gpio_8d') return 8;
  if (props.variant === 'direct_gpio_4d') return 4;
  if (props.variant === 'direct_gpio_2d') return 2;
  if (props.variant === 'direct_gpio_1d') return 1;
  return 4;
});

const segmentValues = computed<number[]>(() => {
  const dCount = effectiveNDigits.value;
  const total = dCount * 8;

  // 1. Explicit values passed from definition.ts resolveValues
  if (props.values && props.values.length >= total) {
    return props.values.slice(0, total);
  }

  // 2. From bright array (duty-cycle integrated brightness)
  const bright = props.bright;
  if (bright && (bright as any).length > 0) {
    const vals: number[] = [];
    const len = (bright as any).length;
    for (let i = 0; i < total; i++) {
      const b = i < len ? (bright as any)[i] : 0;
      vals.push(b >= 50 ? 1 : 0);
    }
    return vals;
  }

  // 3. From segMask
  let maskArr: number[] = [];
  if (Array.isArray(props.segMask)) {
    maskArr = props.segMask;
  } else if (typeof props.segMask === 'string') {
    try {
      maskArr = JSON.parse(props.segMask);
    } catch {}
  }

  if (maskArr.length > 0) {
    const vals: number[] = [];
    for (let d = 0; d < dCount; d++) {
      const m = maskArr[d] ?? 0;
      for (let s = 0; s < 8; s++) {
        vals.push((m >> s) & 1);
      }
    }
    return vals;
  }

  return new Array(total).fill(0);
});

// Explicitly ensure the custom element DOM property is assigned
watchEffect(() => {
  if (segEl.value) {
    segEl.value.values = segmentValues.value;
  }
});
</script>

<template>
  <div
    class="seg-display-canvas"
    :style="flip ? 'transform: rotate(180deg);' : undefined"
  >
    <wokwi-7segment
      ref="segEl"
      :digits="effectiveNDigits"
      :color="color || 'red'"
      :values="segmentValues"
      pins="top"
    />
    <span v-if="label" class="display-label">{{ label }}</span>
  </div>
</template>

<style scoped>
.seg-display-canvas {
  position: relative;
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  width: 100%;
  height: 100%;
  user-select: none;
}
.display-label {
  margin-top: 2px;
  font-size: 8px;
  color: #94a3b8;
  white-space: nowrap;
  pointer-events: none;
  user-select: none;
  text-align: center;
}
</style>
