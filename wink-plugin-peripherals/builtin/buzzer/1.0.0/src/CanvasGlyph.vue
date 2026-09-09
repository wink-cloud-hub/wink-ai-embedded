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
let mainOsc: OscillatorNode | null = null;
let mainGain: GainNode | null = null;
let cavityOsc: OscillatorNode | null = null;
let cavityGain: GainNode | null = null;
let masterGain: GainNode | null = null;
let stopTimer: ReturnType<typeof setTimeout> | null = null;
let currentFreq = 0;

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
      console.warn('[Buzzer Glyph] AudioContext resume waiting for user interaction:', err);
    });
  }
  return audioCtx;
}

function startSound(freq: number) {
  if (isAudioMuted.value) return;
  const ctx = ensureAudioContext();
  if (!ctx) return;

  if (stopTimer) {
    clearTimeout(stopTimer);
    stopTimer = null;
  }

  // Support audible frequency range up to 20kHz (CMS8S78xx buzzer is 10kHz)
  const validFreq = Math.max(20, Math.min(20000, freq > 0 ? freq : 2000));
  const now = ctx.currentTime;

  if (!mainOsc || !mainGain || !masterGain) {
    try {
      mainOsc = ctx.createOscillator();
      mainGain = ctx.createGain();
      masterGain = ctx.createGain();

      cavityOsc = ctx.createOscillator();
      cavityGain = ctx.createGain();

      if (validFreq >= 3000) {
        // High-frequency physical acoustic model (e.g. 10kHz on CMS8S78xx):
        // 1. Primary fundamental tone: pure sine wave with ISO 226 equal-loudness boost (0.24)
        //    to overcome laptop/monitor micro-speaker roll-off and human high-frequency threshold,
        //    with ZERO Nyquist aliasing foldover ("no TV static sizzle").
        mainOsc.type = 'sine';
        mainOsc.frequency.setValueAtTime(validFreq, now);
        mainGain.gain.setValueAtTime(0.24, now);

        // 2. Physical Helmholtz cavity resonance component (~2400 Hz, -18dB):
        //    Simulates the mechanical acoustic resonance excited by step edges on a physical piezo buzzer.
        cavityOsc.type = 'sine';
        cavityOsc.frequency.setValueAtTime(2400, now);
        cavityGain.gain.setValueAtTime(0.035, now);
      } else {
        // Standard frequency (< 3000 Hz, e.g. 1kHz ~ 2.7kHz):
        mainOsc.type = 'square';
        mainOsc.frequency.setValueAtTime(validFreq, now);
        mainGain.gain.setValueAtTime(0.12, now);

        cavityGain.gain.setValueAtTime(0, now);
      }

      masterGain.gain.setValueAtTime(1.0, now);

      mainOsc.connect(mainGain).connect(masterGain);
      cavityOsc.connect(cavityGain).connect(masterGain);
      masterGain.connect(ctx.destination);

      mainOsc.start();
      cavityOsc.start();
      currentFreq = validFreq;
      console.log('[Buzzer Glyph] High-fidelity acoustic engine started at', validFreq, 'Hz');
    } catch (err) {
      console.error('[Buzzer Glyph] Failed to start audio graph:', err);
    }
  } else {
    // Smooth frequency update
    if (Math.abs(currentFreq - validFreq) > 5) {
      if (validFreq >= 3000) {
        mainOsc.type = 'sine';
        mainOsc.frequency.setValueAtTime(validFreq, now);
        mainGain.gain.setValueAtTime(0.24, now);

        cavityOsc.frequency.setValueAtTime(2400, now);
        cavityGain.gain.setValueAtTime(0.035, now);
      } else {
        mainOsc.type = 'square';
        mainOsc.frequency.setValueAtTime(validFreq, now);
        mainGain.gain.setValueAtTime(0.12, now);

        cavityGain.gain.setValueAtTime(0, now);
      }
      currentFreq = validFreq;
    }
    masterGain.gain.setValueAtTime(1.0, now);
  }
}

function stopSound() {
  if (masterGain && audioCtx) {
    const now = audioCtx.currentTime;
    masterGain.gain.setValueAtTime(0, now);

    if (stopTimer) clearTimeout(stopTimer);
    stopTimer = setTimeout(() => {
      if (mainOsc) {
        try { mainOsc.stop(); mainOsc.disconnect(); } catch {}
        mainOsc = null;
      }
      if (cavityOsc) {
        try { cavityOsc.stop(); cavityOsc.disconnect(); } catch {}
        cavityOsc = null;
      }
      if (mainGain) {
        try { mainGain.disconnect(); } catch {}
        mainGain = null;
      }
      if (cavityGain) {
        try { cavityGain.disconnect(); } catch {}
        cavityGain = null;
      }
      if (masterGain) {
        try { masterGain.disconnect(); } catch {}
        masterGain = null;
      }
      currentFreq = 0;
      stopTimer = null;
    }, 50);
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
    window.addEventListener('pointerdown', unlockAudioOnInteraction, { capture: true, passive: true });
    document.addEventListener('visibilitychange', onVisibilityChange);
  }
});

function onVisibilityChange() {
  if (typeof document === 'undefined') return;
  if (document.hidden) {
    stopSound();
  } else if (props.hasSignal && !isAudioMuted.value) {
    startSound(props.frequency ?? 0);
  }
}

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
    window.removeEventListener('pointerdown', unlockAudioOnInteraction, { capture: true });
    document.removeEventListener('visibilitychange', onVisibilityChange);
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
