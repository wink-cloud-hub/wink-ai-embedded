<script setup lang="ts">
import { ref, watch, onBeforeUnmount } from 'vue';

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

const isAudioMuted = ref(true);

let audioCtx: AudioContext | null = null;
let oscillator: OscillatorNode | null = null;
let gainNode: GainNode | null = null;

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

  const validFreq = Math.max(20, Math.min(8000, freq > 0 ? freq : 2000));

  if (!oscillator) {
    oscillator = ctx.createOscillator();
    gainNode = ctx.createGain();

    oscillator.type = 'square';
    oscillator.frequency.setValueAtTime(validFreq, ctx.currentTime);

    // Safe low gain to prevent ear fatigue
    gainNode.gain.setValueAtTime(0.04, ctx.currentTime);

    oscillator.connect(gainNode);
    gainNode.connect(ctx.destination);
    oscillator.start();
  } else {
    oscillator.frequency.setValueAtTime(validFreq, ctx.currentTime);
  }
}

function stopSound() {
  if (oscillator) {
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

function toggleAudio() {
  isAudioMuted.value = !isAudioMuted.value;
  if (isAudioMuted.value) {
    stopSound();
  } else if (props.hasSignal) {
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
  padding: 10px 12px;
  background: rgba(15, 23, 42, 0.88);
  border: 1px solid rgba(148, 163, 184, 0.2);
  border-radius: 8px;
  color: #f8fafc;
  font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
  min-width: 170px;
  box-shadow: 0 4px 12px rgba(0, 0, 0, 0.35);
  transition: all 0.2s ease;
}

.buzzer-world-widget.is-active {
  border-color: rgba(245, 158, 11, 0.65);
  box-shadow: 0 0 12px rgba(245, 158, 11, 0.25);
}

.widget-header {
  display: flex;
  align-items: center;
  justify-content: space-between;
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
  font-family: monospace;
  color: #f1f5f9;
}

.metric-mode {
  font-size: 10px;
  color: #38bdf8;
  font-family: monospace;
}

.audio-control {
  display: flex;
  justify-content: flex-end;
}

.sound-toggle-btn {
  width: 100%;
  font-size: 10px;
  font-weight: 500;
  padding: 4px 8px;
  border-radius: 4px;
  border: 1px solid rgba(148, 163, 184, 0.25);
  background: rgba(30, 41, 59, 0.8);
  color: #94a3b8;
  cursor: pointer;
  display: flex;
  align-items: center;
  justify-content: center;
  gap: 4px;
  transition: all 0.15s ease;
}

.sound-toggle-btn:hover {
  background: rgba(51, 65, 85, 0.9);
  color: #f8fafc;
}

.sound-toggle-btn.is-unmuted {
  background: rgba(245, 158, 11, 0.18);
  color: #f59e0b;
  border-color: rgba(245, 158, 11, 0.5);
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
