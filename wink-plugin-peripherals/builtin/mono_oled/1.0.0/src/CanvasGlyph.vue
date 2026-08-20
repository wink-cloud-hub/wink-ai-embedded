<script setup lang="ts">
import '@wokwi/elements';
import { ref, watch } from 'vue';
import type { DisplayFrame } from '@wink-ai/unisim-ui';
import type { OledElementLike } from './paintFramebuffer';
import { paintOledFramebuffer } from './paintFramebuffer';

const props = defineProps<{
  displayFrame?: DisplayFrame | null;
  framebuffer?: Uint8Array | null;
}>();

const oledEl = ref<OledElementLike | null>(null);

watch(
  () =>
    [props.displayFrame?.seq, props.displayFrame ? null : props.framebuffer, oledEl.value] as const,
  ([, , el]: readonly [unknown, unknown, OledElementLike | null]) => {
    if (!el) return;
    const newFb = props.displayFrame?.fb ?? props.framebuffer ?? null;
    paintOledFramebuffer(el, newFb ?? null);
  },
  { immediate: true },
);
</script>

<template>
  <wokwi-ssd1306 ref="oledEl" />
</template>
