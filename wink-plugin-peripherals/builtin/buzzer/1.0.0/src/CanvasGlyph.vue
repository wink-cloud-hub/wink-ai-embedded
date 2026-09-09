<script setup lang="ts">
import '@wokwi/elements';
import type { PinConnectionValue } from '@wink-ai/unisim-ui';

defineProps<{
  id?: string;
  hasSignal?: boolean;
  frequency?: number;
  duty?: number;
  label?: string;
  pinConnections?: Record<string, PinConnectionValue>;
}>();
</script>

<template>
  <div class="buzzer-glyph-wrapper" :class="{ 'is-active': hasSignal }">
    <wokwi-buzzer :hasSignal="Boolean(hasSignal)" />
    <div class="buzzer-badge-area">
      <span class="buzzer-label">{{ label || 'Buzzer' }}</span>
      <span v-if="hasSignal && frequency" class="freq-tag">
        {{ Math.round(frequency) }}Hz
      </span>
    </div>
  </div>
</template>

<style scoped>
.buzzer-glyph-wrapper {
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  width: 100%;
  height: 100%;
  position: relative;
  user-select: none;
  transition: transform 0.2s ease;
}

.buzzer-glyph-wrapper.is-active {
  filter: drop-shadow(0 0 6px rgba(245, 158, 11, 0.45));
}

.buzzer-badge-area {
  display: flex;
  flex-direction: column;
  align-items: center;
  margin-top: 2px;
  gap: 2px;
  pointer-events: none;
}

.buzzer-label {
  font-size: 10px;
  font-weight: 500;
  color: #94a3b8;
  white-space: nowrap;
  line-height: 1.2;
}

.freq-tag {
  font-size: 8px;
  font-weight: 600;
  font-family: monospace;
  background: rgba(245, 158, 11, 0.2);
  color: #f59e0b;
  border: 1px solid rgba(245, 158, 11, 0.5);
  border-radius: 4px;
  padding: 0 4px;
  line-height: 12px;
  animation: pulse-freq 1s infinite alternate ease-in-out;
}

@keyframes pulse-freq {
  from {
    opacity: 0.8;
  }
  to {
    opacity: 1;
    box-shadow: 0 0 4px rgba(245, 158, 11, 0.6);
  }
}
</style>
