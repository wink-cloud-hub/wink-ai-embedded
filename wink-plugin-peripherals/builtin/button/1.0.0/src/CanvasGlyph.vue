<script setup lang="ts">
import '@wokwi/elements';
import { ref, onBeforeUnmount } from 'vue';

const props = withDefaults(
  defineProps<{
    color: string;
    label: string;
    xray: boolean;
    readonly?: boolean;
  }>(),
  {
    readonly: true,
  },
);

const emit = defineEmits<{
  buttonPress: [];
  buttonRelease: [];
}>();

const isPressed = ref(false);
const isSticky = ref(false);

let cleanupGlobalUp: (() => void) | null = null;

function onPointerDown(e: PointerEvent) {
  if (e.button !== 0) return;
  // In circuit edit mode (readonly === false), let pointerdown bubble to component drag
  if (props.readonly === false) return;

  e.preventDefault();
  e.stopPropagation();

  // If previously latched via Ctrl-click, clicking it again unlatches and releases it
  if (isSticky.value) {
    isSticky.value = false;
    isPressed.value = false;
    emit('buttonRelease');
    return;
  }

  isPressed.value = true;
  emit('buttonPress');

  const handleGlobalUp = (upEvent: PointerEvent) => {
    if (cleanupGlobalUp) {
      cleanupGlobalUp();
      cleanupGlobalUp = null;
    }

    if (!isPressed.value) return;

    if (upEvent.ctrlKey || upEvent.metaKey) {
      isSticky.value = true;
      return;
    }

    isPressed.value = false;
    emit('buttonRelease');
  };

  window.addEventListener('pointerup', handleGlobalUp);
  window.addEventListener('pointercancel', handleGlobalUp);
  cleanupGlobalUp = () => {
    window.removeEventListener('pointerup', handleGlobalUp);
    window.removeEventListener('pointercancel', handleGlobalUp);
  };
}

onBeforeUnmount(() => {
  if (cleanupGlobalUp) {
    cleanupGlobalUp();
    cleanupGlobalUp = null;
  }
});
</script>

<template>
  <div
    class="button-glyph-wrapper"
    draggable="false"
    @pointerdown="onPointerDown"
    @dragstart.prevent
    @selectstart.prevent
    @contextmenu.prevent
  >
    <wokwi-pushbutton
      :color="color"
      :label="''"
      :xray="xray"
      :pressed="isPressed"
      style="pointer-events: none"
    />
  </div>
</template>

<style scoped>
.button-glyph-wrapper {
  display: inline-block;
  vertical-align: top;
  cursor: pointer;
  touch-action: none;
  user-select: none;
  -webkit-user-select: none;
  -webkit-user-drag: none;
}
</style>
