<script setup lang="ts">
import '@wokwi/elements';
import type { PinConnectionValue } from '@wink-ai/unisim-ui';

defineProps<{
  pluginInstanceId?: string;
  pinConnections?: Record<string, PinConnectionValue>;
}>();
</script>

<template>
  <div class="ultrasonic-widget">
    <wokwi-hc-sr04 />
  </div>
</template>

<style scoped>
.ultrasonic-widget {
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 8px;
}
.distance-control {
  display: flex;
  flex-direction: column;
  align-items: center;
  width: 100%;
}
.distance-label {
  font-size: 11px;
  color: #94a3b8;
  font-weight: 500;
}
.distance-slider {
  width: 120px;
  cursor: pointer;
}
</style>
