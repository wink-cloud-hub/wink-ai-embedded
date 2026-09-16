<script setup lang="ts">
import { computed } from 'vue';
import '@wokwi/elements';
import type { PinConnectionValue } from '@wink-ai/unisim-ui';

const props = defineProps<{
  pinConnections?: Record<string, PinConnectionValue>;
  color: string;
  brightness: number;
  label: string;
  flip: boolean;
  level: boolean;
}>();

/**
 * M2-T9-U: lit-glow halo (e.g. the health-pot heater plate radiates a warm glow
 * while energised). Generic for every LED-type output.
 */
const glowStyle = computed(() =>
  props.level
    ? { filter: `drop-shadow(0 0 9px ${props.color}) drop-shadow(0 0 3px ${props.color})` }
    : undefined,
);
</script>

<template>
  <div class="led-glyph" :style="glowStyle">
    <wokwi-led
      :pin="typeof pinConnections?.A === 'number' ? pinConnections.A : 1"
      :color="color"
      :value="level"
      :brightness="brightness"
      :label="''"
      :flip="flip"
    />
  </div>
</template>

<style scoped>
.led-glyph {
  display: inline-block;
  line-height: 0;
  transition: filter 0.25s ease;
}
</style>
