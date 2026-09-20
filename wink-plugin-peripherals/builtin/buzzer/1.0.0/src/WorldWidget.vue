<script setup lang="ts">
import { ref, watch, onBeforeUnmount, onMounted } from 'vue';

const props = withDefaults(
  defineProps<{
    id?: string;
    label?: string;
    hasSignal?: boolean;
    frequency?: number;
    duty?: number;
    variant?: string;
  }>(),
  {
    hasSignal: false,
    frequency: 0,
    duty: 0,
    variant: 'passive_pwm',
  },
);

// Default to muted in World Widget to avoid duplicate audio playback with CanvasGlyph
const isAudioMuted = ref(true);

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
  if (!AudioContextClass) return null;

  if (!audioCtx) {
    audioCtx = new AudioContextClass();
  }
  if (audioCtx.state === 'suspended') {
    audioCtx.resume().catch(() => {});
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
        // High frequency acoustic model:
        mainOsc.type = 'sine';
        mainOsc.frequency.setValueAtTime(validFreq, now);
        mainGain.gain.setValueAtTime(0.24, now);

        // Helmholtz cavity resonance component:
        cavityOsc.type = 'sine';
        cavityOsc.frequency.setValueAtTime(2400, now);
        cavityGain.gain.setValueAtTime(0.035, now);
      } else {
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
    } catch {
      // ignore
    }
  } else {
    if (Math.abs(currentFreq - validFreq) > 5) {
      if (validFreq >= 3000) {
        mainOsc.type = 'sine';
        mainOsc.frequency.setValueAtTime(validFreq, now);
        mainGain.gain.setValueAtTime(0.24, now);

        cavityOsc?.frequency.setValueAtTime(2400, now);
        cavityGain?.gain.setValueAtTime(0.035, now);
      } else {
        mainOsc.type = 'square';
        mainOsc.frequency.setValueAtTime(validFreq, now);
        mainGain.gain.setValueAtTime(0.12, now);

        cavityGain?.gain.setValueAtTime(0, now);
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

function toggleAudio() {
  isAudioMuted.value = !isAudioMuted.value;
  ensureAudioContext();
  if (isAudioMuted.value) {
    stopSound();
  } else if (props.hasSignal) {
    startSound(props.frequency);
  }
}

function unlockAudioOnInteraction() {
  if (audioCtx && audioCtx.state === 'suspended') {
    audioCtx.resume().catch(() => {});
  }
}

onMounted(() => {
  if (typeof window !== 'undefined') {
    window.addEventListener('click', unlockAudioOnInteraction, { capture: true, passive: true });
    window.addEventListener('keydown', unlockAudioOnInteraction, { capture: true, passive: true });
    document.addEventListener('visibilitychange', onVisibilityChange);
  }
});

function onVisibilityChange() {
  if (typeof document === 'undefined') return;
  if (document.hidden) {
    stopSound();
  } else if (props.hasSignal && !isAudioMuted.value) {
    startSound(props.frequency);
  }
}

watch(
  () => [props.hasSignal, props.frequency, isAudioMuted.value] as const,
  ([hasSignal, freq, muted]) => {
    if (hasSignal && !muted) {
      startSound(freq);
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
  <div class="buzzer-world-widget" :class="{ 'is-active': hasSignal }">
    <div class="widget-header">
      <div class="widget-title">
        <span class="icon">🔔</span>
        <span>{{ label || 'Buzzer' }}</span>
      </div>
      <span class="state-pill" :class="hasSignal ? 'state-on' : 'state-off'">
        {{ hasSignal ? 'SOUNDING' : 'QUIET' }}
      </span>
    </div>

    <div class="metrics-grid">
      <div class="metric-item">
        <span class="metric-label">Frequency</span>
        <span class="metric-val">
          {{ hasSignal && frequency ? `${Math.round(frequency)} Hz` : '0 Hz' }}
        </span>
      </div>
      <div class="metric-item">
        <span class="metric-label">Duty</span>
        <span class="metric-val">
          {{ hasSignal ? `${Math.round(duty)}%` : '0%' }}
        </span>
      </div>
      <div class="metric-item full-width">
        <span class="metric-label">Mode</span>
        <span class="metric-mode">{{ variant }}</span>
      </div>
    </div>

    <div class="audio-control">
      <button
        class="sound-toggle-btn"
        :class="{ 'is-unmuted': !isAudioMuted }"
        type="button"
        @click="toggleAudio"
      >
        <span>{{ isAudioMuted ? '🔇 Audio Muted' : '🔊 Audio Active' }}</span>
      </button>
    </div>
  </div>
</template>

<style scoped>
.buzzer-world-widget {
  min-width: 170px;
  padding: 10px 12px;
  background: rgba(15, 23, 42, 0.88);
  border: 1px solid rgba(148, 163, 184, 0.2);
  border-radius: 8px;
  color: #f8fafc;
  font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
  box-shadow: 0 4px 12px rgba(0, 0, 0, 0.35);
  transition: all 0.2s ease;
}

.buzzer-world-widget.is-active {
  border-color: rgba(245, 158, 11, 0.65);
  box-shadow: 0 0 12px rgba(245, 158, 11, 0.25);
}

.widget-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin-bottom: 8px;
  gap: 8px;
}

.widget-title {
  display: flex;
  align-items: center;
  gap: 4px;
  font-size: 12px;
  font-weight: 600;
  color: #e2e8f0;
}

.state-pill {
  font-size: 9px;
  font-weight: 700;
  padding: 2px 6px;
  border-radius: 9999px;
  text-transform: uppercase;
  letter-spacing: 0.05em;
}

.state-on {
  background: rgba(245, 158, 11, 0.2);
  color: #fbbf24;
  border: 1px solid rgba(245, 158, 11, 0.6);
  animation: pulse-border 1s infinite alternate;
}

.state-off {
  background: rgba(100, 116, 139, 0.2);
  color: #94a3b8;
  border: 1px solid rgba(100, 116, 139, 0.4);
}

.metrics-grid {
  display: grid;
  grid-template-columns: 1fr 1fr;
  gap: 6px;
  background: rgba(30, 41, 59, 0.5);
  padding: 6px 8px;
  border-radius: 6px;
  margin-bottom: 8px;
}

.metric-item {
  display: flex;
  flex-direction: column;
}

.metric-item.full-width {
  grid-column: span 2;
  border-top: 1px solid rgba(148, 163, 184, 0.1);
  padding-top: 4px;
}

.metric-label {
  font-size: 9px;
  color: #64748b;
  text-transform: uppercase;
}

.metric-val {
  font-size: 11px;
  font-weight: 600;
  color: #f1f5f9;
  font-family: ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, monospace;
}

.metric-mode {
  font-size: 10px;
  color: #38bdf8;
  font-family: ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, monospace;
}

.audio-control {
  display: flex;
  justify-content: flex-end;
}

.sound-toggle-btn {
  width: 100%;
  display: flex;
  align-items: center;
  justify-content: center;
  gap: 4px;
  font-size: 10px;
  font-weight: 500;
  padding: 4px 8px;
  background: rgba(30, 41, 59, 0.8);
  border: 1px solid rgba(148, 163, 184, 0.25);
  border-radius: 4px;
  color: #94a3b8;
  cursor: pointer;
  transition: all 0.15s ease;
}

.sound-toggle-btn:hover {
  background: rgba(51, 65, 85, 0.9);
  color: #f8fafc;
}

.sound-toggle-btn.is-unmuted {
  background: rgba(245, 158, 11, 0.18);
  border-color: rgba(245, 158, 11, 0.5);
  color: #f59e0b;
}

@keyframes pulse-border {
  from {
    box-shadow: 0 0 2px rgba(245, 158, 11, 0.3);
  }
  to {
    box-shadow: 0 0 8px rgba(245, 158, 11, 0.7);
  }
}
</style>
