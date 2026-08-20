<script setup lang="ts">
import '@wokwi/elements';
import { computed } from 'vue';
import type { PinConnectionValue } from '@wink-ai/unisim-ui';

const props = defineProps<{
  pluginInstanceId?: string;
  pinConnections: Record<string, PinConnectionValue>;
  color: 'red' | 'green' | 'blue' | 'yellow' | 'white' | 'black';
  label: string;
  xray: boolean;
  activeLow: boolean;
}>();

const emit = defineEmits<{
  (e: 'buttonPress'): void;
  (e: 'buttonRelease'): void;
}>();

const pinLabel = computed(() => {
  const left1 = props.pinConnections ? props.pinConnections['1.l'] : undefined;
  const left2 = props.pinConnections ? props.pinConnections['2.l'] : undefined;
  const right1 = props.pinConnections ? props.pinConnections['1.r'] : undefined;
  const right2 = props.pinConnections ? props.pinConnections['2.r'] : undefined;
  return `1.l:${left1}, 2.l:${left2}, 1.r:${right1}, 2.r:${right2}`;
});

function handlePress() {
  emit('buttonPress');
}

function handleRelease() {
  emit('buttonRelease');
}
</script>

<template>
  <div class="virtual-button">
    <div class="component-label">Button ({{ pinLabel }})</div>
    <div class="btn-wrapper">
      <wokwi-pushbutton
        :color="color"
        :label="label"
        :xray="xray"
        @button-press="handlePress"
        @button-release="handleRelease"
      />
    </div>
  </div>
</template>

<style scoped>
.virtual-button {
  display: flex;
  flex-direction: column;
  align-items: center;
  padding: 10px;
  background: rgba(255, 255, 255, 0.02);
  border: 1px solid rgba(255, 255, 255, 0.08);
  border-radius: 8px;
  width: 100px;
  backdrop-filter: blur(4px);
  box-shadow: 0 4px 6px rgba(0, 0, 0, 0.1);
  transition: border-color 0.2s;
}
.virtual-button:hover {
  border-color: rgba(0, 255, 136, 0.3);
}
.component-label {
  font-size: 11px;
  color: #8fa0a8;
  margin-bottom: 8px;
  font-weight: 500;
}
.btn-wrapper {
  display: flex;
  justify-content: center;
  align-items: center;
  height: 50px;
}
</style>
