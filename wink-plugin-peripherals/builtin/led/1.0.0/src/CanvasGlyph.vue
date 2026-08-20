<script setup lang="ts">
import '@wokwi/elements';
import type { PinConnectionValue } from '@wink-ai/unisim-ui';

defineProps<{
  pinConnections?: Record<string, PinConnectionValue>;
  color: string;
  brightness: number;
  label: string;
  flip: boolean;
  level: boolean;
}>();
</script>

<template>
  <wokwi-led
    :pin="typeof pinConnections?.A === 'number' ? pinConnections.A : 1"
    :color="color"
    :value="level"
    :brightness="brightness"
    :label="label"
    :flip="flip"
  />
</template>
