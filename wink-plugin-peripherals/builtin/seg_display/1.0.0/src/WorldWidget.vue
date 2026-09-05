<script setup lang="ts">
import { computed, ref, watchEffect } from 'vue';
import '@wokwi/elements';

const props = withDefaults(
  defineProps<{
    pinConnections?: Record<string, unknown>;
    color?: string;
    brightness?: number;
    label?: string;
    text?: string;
    variant?: string;
    nDigits?: number;
    bright?: Uint8Array | number[] | null;
    segMask?: number[] | string | null;
    values?: number[];
  }>(),
  {
    color: 'red',
    brightness: 1.0,
    label: '',
    text: '',
    variant: 'direct_gpio_4d',
    nDigits: 4,
    bright: null,
    segMask: () => [],
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

  if (props.values && props.values.length >= total) {
    return props.values.slice(0, total);
  }

  const bright = props.bright;
  if (bright && (bright as any).length > 0) {
    const vals: number[] = [];
    const len = (bright as any).length;
    for (let i = 0; i < total; i++) {
      const b = i < len ? (bright as any)[i] : 0;
      vals.push(b >= 30 ? 1 : 0);
    }
    return vals;
  }

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

watchEffect(() => {
  if (segEl.value) {
    segEl.value.values = segmentValues.value;
  }
});
</script>

<template>
  <div class="seg-display-world-widget">
    <wokwi-7segment
      ref="segEl"
      :digits="effectiveNDigits"
      :color="color || 'red'"
      :values="segmentValues"
      pins="none"
    />
    <div v-if="label" class="display-label">{{ label }}</div>
  </div>
</template>

<style scoped>
.seg-display-world-widget {
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  user-select: none;
}
.display-label {
  margin-top: 4px;
  font-size: 10px;
  color: #888890;
  text-align: center;
}
</style>
