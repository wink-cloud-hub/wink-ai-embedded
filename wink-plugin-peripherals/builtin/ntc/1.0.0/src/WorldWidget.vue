<script setup lang="ts">
import '@wokwi/elements';
import { ref, watch } from 'vue';
import type { PinConnectionValue } from '@wink-ai/unisim-ui';

const props = withDefaults(
  defineProps<{
    pluginInstanceId?: string;
    pinConnections?: Record<string, PinConnectionValue>;
    temperature?: number;
    minTemp?: number;
    maxTemp?: number;
  }>(),
  {
    temperature: 25,
    minTemp: -20,
    maxTemp: 120,
  },
);

const emit = defineEmits<{
  (e: 'update:temperature', val: number): void;
  (e: 'prop-change', key: string, val: unknown): void;
}>();

const currentTemp = ref(props.temperature);

watch(
  () => props.temperature,
  val => {
    if (val !== undefined && val !== currentTemp.value) {
      currentTemp.value = val;
    }
  },
);

function onInput(evt: Event) {
  const target = evt.target as HTMLInputElement;
  const val = Number(target.value);
  currentTemp.value = val;
  emit('update:temperature', val);
  emit('prop-change', 'temperature', val);
}
</script>

<template>
  <div class="ntc-widget">
    <wokwi-ntc-temperature-sensor />
    <div class="temp-control">
      <div class="temp-label">{{ currentTemp }} °C</div>
      <input
        type="range"
        class="temp-slider"
        :min="minTemp"
        :max="maxTemp"
        step="1"
        :value="currentTemp"
        @input="onInput"
      />
    </div>
  </div>
</template>

<style scoped>
.ntc-widget {
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 6px;
  user-select: none;
}
.temp-control {
  display: flex;
  flex-direction: column;
  align-items: center;
  width: 100%;
}
.temp-label {
  font-size: 11px;
  font-weight: 600;
  color: #38bdf8;
  font-variant-numeric: tabular-nums;
}
.temp-slider {
  width: 110px;
  cursor: pointer;
  accent-color: #38bdf8;
}
</style>
