<script setup lang="ts">
defineProps<{
  pinConnections?: Record<string, unknown>;
  color: string;
  brightness: number;
  label: string;
  text?: string;
}>();
</script>

<template>
  <div class="seg-display-world-widget">
    <div class="display-bezel">
      <div class="display-screen">
        <span class="display-text" :style="{ color }">
          {{ text || '--------' }}
        </span>
      </div>
      <div v-if="label" class="display-label">{{ label }}</div>
    </div>
  </div>
</template>

<style scoped>
.seg-display-world-widget {
  display: inline-block;
  user-select: none;
}
.display-bezel {
  background: #141416;
  border: 1.5px solid #2c2d33;
  border-radius: 4px;
  padding: 6px 12px;
  box-shadow: 0 2px 6px rgba(0, 0, 0, 0.5);
}
.display-screen {
  background: #090a0c;
  border: 1px solid #1c1d22;
  border-radius: 2px;
  padding: 4px 8px;
  min-width: 100px;
  text-align: center;
}
.display-text {
  font-family: 'Courier New', monospace;
  font-weight: bold;
  letter-spacing: 2px;
  font-size: 14px;
  text-shadow: 0 0 6px currentColor;
}
.display-label {
  margin-top: 4px;
  font-size: 10px;
  color: #888890;
  text-align: center;
}
</style>
