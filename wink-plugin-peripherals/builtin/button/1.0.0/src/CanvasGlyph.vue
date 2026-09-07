<script setup lang="ts">
import '@wokwi/elements';
import { ref } from 'vue';

defineProps<{
  color: string;
  label: string;
  xray: boolean;
}>();

const emit = defineEmits<{
  buttonPress: [];
  buttonRelease: [];
}>();

const isPressed = ref(false);
const isSticky = ref(false);

function onPointerDown(e: PointerEvent) {
  if (e.button !== 0) return;
  // If previously latched via Ctrl-click, clicking it again unlatches and releases it
  if (isSticky.value) {
    isSticky.value = false;
    isPressed.value = false;
    emit('buttonRelease');
    return;
  }

  try {
    (e.currentTarget as HTMLElement).setPointerCapture(e.pointerId);
  } catch {}

  isPressed.value = true;
  emit('buttonPress');
}

function onPointerUp(e: PointerEvent) {
  try {
    (e.currentTarget as HTMLElement).releasePointerCapture(e.pointerId);
  } catch {}

  if (!isPressed.value) return;

  // If Ctrl/Cmd was held during release, latch the button (sticky mode)
  if (e.ctrlKey || e.metaKey) {
    isSticky.value = true;
    return;
  }

  isPressed.value = false;
  emit('buttonRelease');
}

function onPointerCancel(e: PointerEvent) {
  try {
    (e.currentTarget as HTMLElement).releasePointerCapture(e.pointerId);
  } catch {}
  if (isPressed.value && !isSticky.value) {
    isPressed.value = false;
    emit('buttonRelease');
  }
}
</script>

<template>
  <div
    class="button-glyph-wrapper"
    @pointerdown="onPointerDown"
    @pointerup="onPointerUp"
    @pointercancel="onPointerCancel"
  >
    <wokwi-pushbutton
      :color="color"
      :label="label"
      :xray="xray"
      :pressed="isPressed"
      style="pointer-events: none;"
    />
  </div>
</template>

<style scoped>
.button-glyph-wrapper {
  display: inline-block;
  vertical-align: top;
  cursor: pointer;
  user-select: none;
  -webkit-user-select: none;
}
</style>
