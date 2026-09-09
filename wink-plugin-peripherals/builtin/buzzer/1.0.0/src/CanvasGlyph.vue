<script setup lang="ts">
import { ref, watch, onBeforeUnmount, onMounted } from 'vue';
import '@wokwi/elements';
import type { PinConnectionValue } from '@wink-ai/unisim-ui';

const props = defineProps<{
  id?: string;
  hasSignal?: boolean;
  frequency?: number;
  duty?: number;
  label?: string;
  pinConnections?: Record<string, PinConnectionValue>;
}>();

// Default to unmuted so buzzer sounds immediately when simulation runs
const isAudioMuted = ref(false);

let audioCtx: AudioContext | null = null;
let oscillator: OscillatorNode | null = null;
let gainNode: GainNode | null = null;

function ensureAudioContext(): AudioContext | null {
  if (typeof window === 'undefined') return null;
  const AudioContextClass =
    window.AudioContext ||
    (window as unknown as { webkitAudioContext: typeof AudioContext }).webkitAudioContext;
  if (!AudioContextClass) {
    console.warn('[Buzzer Glyph] Web Audio API not supported in this browser environment');
    return null;
  }

  if (!audioCtx) {
    audioCtx = new AudioContextClass();
    console.log('[Buzzer Glyph] AudioContext created, initial state:', audioCtx.state);
  }
  if (audioCtx.state === 'suspended') {
    audioCtx.resume().then(() => {
      console.log('[Buzzer Glyph] AudioContext resumed, state:', audioCtx?.state);
    }).catch(err => {
      console.warn('[Buzzer Glyph] AudioContext resume failed (waiting for user gesture):', err);
    });
  }
  return audioCtx;
}

function startSound(freq: number) {
  console.log('[Buzzer Glyph] startSound called with freq:', freq, 'isMuted:', isAudioMuted.value);
  if (isAudioMuted.value) return;
  const ctx = ensureAudioContext();
  if (!ctx) return;

  // Support audible frequency range up to 20kHz (CMS8S78xx buzzer is 10kHz)
  const validFreq = Math.max(20, Math.min(20000, freq > 0 ? freq : 2000));

  if (!oscillator) {
    try {
      oscillator = ctx.createOscillator();
      gainNode = ctx.createGain();

      oscillator.type = 'square';
      oscillator.frequency.setValueAtTime(validFreq, ctx.currentTime);

      // Safe moderate volume (0.06) to prevent clipping/ear fatigue
      gainNode.gain.setValueAtTime(0.06, ctx.currentTime);

      oscillator.connect(gainNode);
      gainNode.connect(ctx.destination);
      oscillator.start();
      console.log('[Buzzer Glyph] Oscillator started at', validFreq, 'Hz, audioCtx state:', ctx.state);
    } catch (err) {
      console.error('[Buzzer Glyph] Failed to start oscillator:', err);
    }
  } else {
    try {
      oscillator.frequency.setValueAtTime(validFreq, ctx.currentTime);
      console.log('[Buzzer Glyph] Oscillator frequency updated to', validFreq, 'Hz');
    } catch (err) {
      console.warn('[Buzzer Glyph] Failed to update oscillator frequency:', err);
    }
  }
}

function stopSound() {
  if (oscillator) {
    console.log('[Buzzer Glyph] stopSound called');
    try {
      oscillator.stop();
      oscillator.disconnect();
    } catch {
      // ignore
    }
    oscillator = null;
  }
  if (gainNode) {
    try {
      gainNode.disconnect();
    } catch {
      // ignore
    }
    gainNode = null;
  }
}

function toggleMute(e?: Event) {
  if (e) e.stopPropagation();
  isAudioMuted.value = !isAudioMuted.value;
  console.log('[Buzzer Glyph] Audio mute toggled to:', isAudioMuted.value);
  ensureAudioContext();
  if (isAudioMuted.value) {
    stopSound();
  } else if (props.hasSignal) {
    startSound(props.frequency ?? 0);
  }
}

function unlockAudioOnInteraction() {
  if (audioCtx && audioCtx.state === 'suspended') {
    audioCtx.resume().then(() => {
      console.log('[Buzzer Glyph] AudioContext unlocked by interaction, state:', audioCtx?.state);
    }).catch(() => {});
  }
}

onMounted(() => {
  console.log('[Buzzer Glyph] mounted. props:', props);
  if (typeof window !== 'undefined') {
    window.addEventListener('click', unlockAudioOnInteraction, { capture: true, passive: true });
    window.addEventListener('keydown', unlockAudioOnInteraction, { capture: true, passive: true });
  }
});

watch(
  () => [props.hasSignal, props.frequency, isAudioMuted.value] as const,
  ([hasSignal, freq, muted]) => {
    console.log('[Buzzer Glyph] watch triggered:', { id: props.id, hasSignal, freq, muted });
    if (hasSignal && !muted) {
      startSound(freq ?? 0);
    } else {
      stopSound();
    }
  },
  { immediate: true },
);

onBeforeUnmount(() => {
  if (typeof window !== 'undefined') {
    window.removeEventListener('click', unlockAudioOnInteraction, { capture: true });
    window.removeEventListener('keydown', unlockAudioOnInteraction, { capture: true });
  }
  stopSound();
  if (audioCtx) {
    audioCtx.close().catch(() => {});
    audioCtx = null;
  }
});
</script>

<template>
  <div class="buzzer-glyph-wrapper" :class="{ 'is-active': hasSignal }" @click="ensureAudioContext">
    <wokwi-buzzer :hasSignal="Boolean(hasSignal)" />
    <div class="buzzer-badge-area">
      <div class="buzzer-info-row">
        <span class="buzzer-label">{{ label || 'Buzzer' }}</span>
        <button
          type="button"
          class="audio-toggle-btn"
          :title="isAudioMuted ? '点击开启声音' : '点击静音'"
          @click="toggleMute"
        >
          {{ isAudioMuted ? '🔇' : '🔊' }}
        </button>
      </div>
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
  cursor: pointer;
}

.buzzer-glyph-wrapper.is-active {
  filter: drop-shadow(0 0 8px rgba(245, 158, 11, 0.55));
}

.buzzer-badge-area {
  display: flex;
  flex-direction: column;
  align-items: center;
  margin-top: 2px;
  gap: 2px;
}

.buzzer-info-row {
  display: flex;
  align-items: center;
  gap: 4px;
}

.buzzer-label {
  font-size: 10px;
  font-weight: 500;
  color: #94a3b8;
  white-space: nowrap;
  line-height: 1.2;
}

.audio-toggle-btn {
  background: none;
  border: none;
  cursor: pointer;
  padding: 0;
  font-size: 11px;
  line-height: 1;
  opacity: 0.8;
  transition: opacity 0.15s, transform 0.15s;
}

.audio-toggle-btn:hover {
  opacity: 1;
  transform: scale(1.15);
}

.freq-tag {
  font-size: 9px;
  font-family: ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, monospace;
  font-weight: 600;
  color: #f59e0b;
  background: rgba(245, 158, 11, 0.15);
  border: 1px solid rgba(245, 158, 11, 0.35);
  border-radius: 3px;
  padding: 0 4px;
  line-height: 1.2;
}
</style>
