<script setup lang="ts">
import { computed } from 'vue';
import '@wokwi/elements';

const props = defineProps<{
  id: string;
  label?: string;
  pwmChannel?: number;
  angle: number;
  rotation?: number;
}>();

const isTransposed = computed(() => {
  const r = Math.abs((props.rotation ?? 0) % 180);
  return r === 90;
});

const labelStyle = computed(() => {
  const offset = isTransposed.value ? '48px' : '38px';
  const fallbackDeg = props.rotation ?? 0;
  return {
    position: 'absolute' as const,
    left: '50%',
    top: '50%',
    transformOrigin: '0 0',
    transform: `rotate(calc(-1 * var(--rot, ${fallbackDeg}deg))) translateY(${offset}) translateX(-50%)`,
    fontSize: '9px',
    color: '#94a3b8',
    whiteSpace: 'nowrap' as const,
    pointerEvents: 'none' as const,
    userSelect: 'none' as const,
    textAlign: 'center' as const,
    transition: 'transform 0.15s ease',
  };
});
</script>

<template>
  <div
    class="servo-container"
    style="position: relative; display: flex; align-items: center; justify-content: center; width: 100%; height: 100%;"
  >
    <wokwi-servo :angle="angle" />
    <span
      class="label"
      :class="{ 'is-transposed': isTransposed }"
      :style="labelStyle"
    >
      {{ label || id }} ({{ Math.round(angle) }}°)
    </span>
  </div>
</template>

<style scoped>
.servo-container {
  position: relative;
  display: flex;
  align-items: center;
  justify-content: center;
  width: 100%;
  height: 100%;
}
.label {
  position: absolute;
  left: 50%;
  top: 50%;
  transform-origin: 0 0;
  transform: rotate(calc(-1 * var(--rot, 0deg))) translateY(38px) translateX(-50%);
  font-size: 9px;
  color: #94a3b8;
  white-space: nowrap;
  pointer-events: none;
  user-select: none;
  text-align: center;
  transition: transform 0.15s ease;
}
.label.is-transposed {
  transform: rotate(calc(-1 * var(--rot, 0deg))) translateY(48px) translateX(-50%);
}
</style>
