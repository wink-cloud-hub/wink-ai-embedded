<script setup lang="ts">
import { computed } from 'vue';
import '@wokwi/elements';

/**
 * M2-T9-U: NTC canvas glyph with a live temperature badge. The value is bound
 * from the plugin's own published `temperature` channel (`pluginChannels`) with
 * the design-time prop as fallback.
 */
const props = withDefaults(
  defineProps<{
    temperature?: number;
    label?: string;
  }>(),
  { temperature: 25, label: '' },
);

const displayTemp = computed(() =>
  Number.isFinite(Number(props.temperature)) ? Number(props.temperature) : 25,
);

const badgeText = computed(() => `${displayTemp.value.toFixed(1)}°C`);

/** Heat tint: cool blue → warm red, clamped to the sensor range. */
const heatColor = computed(() => {
  const t = Math.min(120, Math.max(-20, displayTemp.value));
  const ratio = (t + 20) / 140;
  const hue = Math.round(210 - ratio * 210);
  return `hsl(${hue}, 85%, 55%)`;
});
</script>

<template>
  <div class="ntc-glyph">
    <wokwi-ntc-temperature-sensor />
    <span
      class="ntc-badge"
      :style="{ borderColor: heatColor, color: heatColor }"
      :title="label || 'NTC'"
    >
      {{ badgeText }}
    </span>
  </div>
</template>

<style scoped>
.ntc-glyph {
  position: relative;
  display: inline-block;
  line-height: 0;
}

.ntc-badge {
  position: absolute;
  top: -9px;
  left: 50%;
  transform: translateX(-50%);
  padding: 0 4px;
  border: 1px solid currentColor;
  border-radius: 8px;
  background: rgba(10, 18, 34, 0.85);
  font-family: 'JetBrains Mono', monospace;
  font-size: 9px;
  line-height: 14px;
  white-space: nowrap;
  pointer-events: none;
}
</style>
