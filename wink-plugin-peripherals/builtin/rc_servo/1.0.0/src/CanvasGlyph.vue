<script setup lang="ts">
import '@wokwi/elements';

defineProps<{
  id: string;
  label?: string;
  pwmChannel?: number;
  angle: number;
}>();
</script>

<template>
  <div class="servo-container">
    <wokwi-servo :angle="angle" />
    <span class="label">{{ label || id }} ({{ Math.round(angle) }}°)</span>
  </div>
</template>

<style scoped>
.servo-container {
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  width: 100%;
  height: 100%;
}
.label {
  font-size: 9px;
  color: #94a3b8;
  margin-top: 2px;
  text-align: center;
}
</style>
