import { definePeripheral, pinsFromBinderVariant, resolvePluginInstanceId } from "@wink-ai/unisim-ui";
import { resolvePluginIdentity } from "@wink-ai/unisim";
import {
  defineComponent,
  ref,
  watch,
  onMounted,
  onBeforeUnmount,
  openBlock,
  createElementBlock,
  createElementVNode,
  createCommentVNode,
  toDisplayString,
  normalizeClass
} from "vue";
import "@wokwi/elements";

const identity = resolvePluginIdentity(import.meta.url, "buzzer", "1.0.0", "output");

// =============================================================================
// CanvasGlyph Component
// =============================================================================
const CanvasGlyph = defineComponent({
  name: "CanvasGlyph",
  props: {
    id: { type: String, default: "" },
    hasSignal: { type: Boolean, default: false },
    frequency: { type: Number, default: 0 },
    duty: { type: Number, default: 0 },
    label: { type: String, default: "Buzzer" },
    pinConnections: { type: Object, default: () => ({}) },
  },
  setup(props) {
    const isAudioMuted = ref(false);
    let audioCtx = null;
    let oscillator = null;
    let gainNode = null;

    function ensureAudioContext() {
      if (typeof window === "undefined") return null;
      const AudioContextClass =
        window.AudioContext || window.webkitAudioContext;
      if (!AudioContextClass) {
        console.warn("[Buzzer Glyph] Web Audio API not supported in this browser");
        return null;
      }

      if (!audioCtx) {
        audioCtx = new AudioContextClass();
        console.log("[Buzzer Glyph] AudioContext created, initial state:", audioCtx.state);
      }
      if (audioCtx.state === "suspended") {
        audioCtx.resume().then(() => {
          console.log("[Buzzer Glyph] AudioContext resumed successfully, state:", audioCtx?.state);
        }).catch(err => {
          console.warn("[Buzzer Glyph] AudioContext resume waiting for user interaction:", err);
        });
      }
      return audioCtx;
    }

    function startSound(freq) {
      console.log("[Buzzer Glyph] startSound called with freq:", freq, "isMuted:", isAudioMuted.value);
      if (isAudioMuted.value) return;
      const ctx = ensureAudioContext();
      if (!ctx) return;

      const validFreq = Math.max(20, Math.min(20000, freq > 0 ? freq : 2000));

      if (!oscillator) {
        try {
          oscillator = ctx.createOscillator();
          gainNode = ctx.createGain();

          oscillator.type = "square";
          oscillator.frequency.setValueAtTime(validFreq, ctx.currentTime);

          // Safe moderate volume (0.06)
          gainNode.gain.setValueAtTime(0.06, ctx.currentTime);

          oscillator.connect(gainNode);
          gainNode.connect(ctx.destination);
          oscillator.start();
          console.log("[Buzzer Glyph] Oscillator started successfully at", validFreq, "Hz, audioCtx state:", ctx.state);
        } catch (err) {
          console.error("[Buzzer Glyph] Failed to start oscillator:", err);
        }
      } else {
        try {
          oscillator.frequency.setValueAtTime(validFreq, ctx.currentTime);
          console.log("[Buzzer Glyph] Oscillator frequency updated to", validFreq, "Hz");
        } catch (err) {
          console.warn("[Buzzer Glyph] Failed to update frequency:", err);
        }
      }
    }

    function stopSound() {
      if (oscillator) {
        console.log("[Buzzer Glyph] stopSound called");
        try {
          oscillator.stop();
          oscillator.disconnect();
        } catch {}
        oscillator = null;
      }
      if (gainNode) {
        try {
          gainNode.disconnect();
        } catch {}
        gainNode = null;
      }
    }

    function toggleMute(e) {
      if (e) e.stopPropagation();
      isAudioMuted.value = !isAudioMuted.value;
      console.log("[Buzzer Glyph] Audio mute toggled to:", isAudioMuted.value);
      ensureAudioContext();
      if (isAudioMuted.value) {
        stopSound();
      } else if (props.hasSignal) {
        startSound(props.frequency ?? 0);
      }
    }

    function unlockAudioOnInteraction() {
      if (audioCtx && audioCtx.state === "suspended") {
        audioCtx.resume().then(() => {
          console.log("[Buzzer Glyph] AudioContext unlocked by user interaction, state:", audioCtx?.state);
        }).catch(() => {});
      }
    }

    onMounted(() => {
      console.log("[Buzzer Glyph] Component mounted, id:", props.id, "props:", props);
      if (typeof window !== "undefined") {
        window.addEventListener("click", unlockAudioOnInteraction, { capture: true, passive: true });
        window.addEventListener("keydown", unlockAudioOnInteraction, { capture: true, passive: true });
      }
    });

    watch(
      () => [props.hasSignal, props.frequency, isAudioMuted.value],
      ([hasSignal, freq, muted]) => {
        console.log("[Buzzer Glyph] watch trigger -> hasSignal:", hasSignal, "freq:", freq, "muted:", muted);
        if (hasSignal && !muted) {
          startSound(freq ?? 0);
        } else {
          stopSound();
        }
      },
      { immediate: true }
    );

    onBeforeUnmount(() => {
      if (typeof window !== "undefined") {
        window.removeEventListener("click", unlockAudioOnInteraction, { capture: true });
        window.removeEventListener("keydown", unlockAudioOnInteraction, { capture: true });
      }
      stopSound();
      if (audioCtx) {
        audioCtx.close().catch(() => {});
        audioCtx = null;
      }
    });

    return (ctx, _cache) => {
      return (
        openBlock(),
        createElementBlock(
          "div",
          {
            class: normalizeClass(["buzzer-glyph-wrapper", { "is-active": props.hasSignal }]),
            onClick: ensureAudioContext,
            style: "display:flex;flex-direction:column;align-items:center;justify-content:center;padding:4px;user-select:none;cursor:pointer;position:relative;"
          },
          [
            createElementVNode(
              "wokwi-buzzer",
              { hasSignal: Boolean(props.hasSignal) },
              null,
              8,
              ["hasSignal"]
            ),
            createElementVNode(
              "div",
              { class: "buzzer-badge-area", style: "display:flex;flex-direction:column;align-items:center;gap:2px;margin-top:2px;" },
              [
                createElementVNode(
                  "div",
                  { class: "buzzer-info-row", style: "display:flex;align-items:center;gap:4px;font-size:11px;" },
                  [
                    createElementVNode("span", null, toDisplayString(props.label || "Buzzer"), 1),
                    createElementVNode(
                      "button",
                      {
                        type: "button",
                        class: "audio-toggle-btn",
                        title: isAudioMuted.value ? "点击开启声音" : "点击静音",
                        onClick: toggleMute,
                        style: "border:none;background:rgba(0,0,0,0.06);border-radius:4px;padding:1px 3px;cursor:pointer;font-size:12px;"
                      },
                      toDisplayString(isAudioMuted.value ? "🔇" : "🔊"),
                      9,
                      ["title"]
                    )
                  ]
                ),
                props.hasSignal && props.frequency
                  ? (
                      openBlock(),
                      createElementBlock(
                        "span",
                        {
                          key: 0,
                          class: "freq-tag",
                          style: "font-size:10px;font-family:monospace;background:#f59e0b;color:#fff;border-radius:3px;padding:0 3px;"
                        },
                        toDisplayString(Math.round(props.frequency)) + "Hz",
                        1
                      )
                    )
                  : createCommentVNode("", true)
              ]
            )
          ],
          2
        )
      );
    };
  }
});

// =============================================================================
// WorldWidget Component
// =============================================================================
const WorldWidget = defineComponent({
  name: "WorldWidget",
  props: {
    id: { type: String, default: "" },
    label: { type: String, default: "Buzzer" },
    hasSignal: { type: Boolean, default: false },
    frequency: { type: Number, default: 0 },
    duty: { type: Number, default: 0 },
    variant: { type: String, default: "passive_pwm" }
  },
  setup(props) {
    return (ctx, _cache) => {
      return (
        openBlock(),
        createElementBlock(
          "div",
          {
            class: normalizeClass(["buzzer-world-widget", { "is-active": props.hasSignal }]),
            style: "padding:8px;border-radius:6px;background:rgba(255,255,255,0.05);font-size:12px;"
          },
          [
            createElementVNode("div", null, [
              createElementVNode("span", null, "🔔 " + toDisplayString(props.label || "Buzzer"), 1),
              createElementVNode(
                "span",
                { style: "margin-left:8px;font-weight:bold;color:" + (props.hasSignal ? "#22c55e" : "#888") },
                toDisplayString(props.hasSignal ? "SOUNDING" : "QUIET"),
                1
              )
            ]),
            createElementVNode("div", { style: "margin-top:4px;font-family:monospace;font-size:11px;" }, [
              createElementVNode(
                "span",
                null,
                "Freq: " + toDisplayString(props.hasSignal && props.frequency ? Math.round(props.frequency) + " Hz" : "0 Hz"),
                1
              )
            ])
          ],
          2
        )
      );
    };
  }
});

// =============================================================================
// Overlay & Pin Bindings
// =============================================================================
const BUZZER_OVERLAY = Object.freeze({
  "1": Object.freeze({
    relX: 30,
    relY: 82,
    wireNet: "primary",
    defaultConnection: null,
    required: true,
  }),
  "2": Object.freeze({
    relX: 34,
    relY: 82,
    wireNet: "gnd",
    defaultConnection: "GND",
    required: false,
  }),
});

const BUZZER_PASSIVE_PINS = Object.freeze([
  {
    name: "1",
    direction: "sink",
    signal: "digital",
    catalogType: "gpio",
    simRole: "pwm",
    aliases: ["1", "sig", "signal", "pwm", "anode", "pos"],
    required: true,
  },
  {
    name: "2",
    direction: "ground",
    signal: "power",
    catalogType: "power",
    simRole: "gnd",
    aliases: ["2", "gnd", "ground", "cathode", "neg"],
    required: false,
  },
]);

const BUZZER_ACTIVE_PINS = Object.freeze([
  {
    name: "1",
    direction: "sink",
    signal: "digital",
    catalogType: "gpio",
    simRole: "signal",
    aliases: ["1", "sig", "signal", "gpio", "anode", "pos"],
    required: true,
  },
  {
    name: "2",
    direction: "ground",
    signal: "power",
    catalogType: "power",
    simRole: "gnd",
    aliases: ["2", "gnd", "ground", "cathode", "neg"],
    required: false,
  },
]);

function getBuzzerPins(variant) {
  try {
    const pins = pinsFromBinderVariant("buzzer", variant);
    if (pins && pins.length > 0) return pins;
  } catch {}
  return variant === "active_gpio" ? BUZZER_ACTIVE_PINS : BUZZER_PASSIVE_PINS;
}

export const BUZZER_TOPOLOGIES = Object.freeze({
  passive_pwm: Object.freeze({
    variant: "passive_pwm",
    getPins: () => getBuzzerPins("passive_pwm"),
    pinsOverlay: BUZZER_OVERLAY,
    defaultAppearanceId: "buzzer_passive",
  }),
  active_gpio: Object.freeze({
    variant: "active_gpio",
    getPins: () => getBuzzerPins("active_gpio"),
    pinsOverlay: BUZZER_OVERLAY,
    defaultAppearanceId: "buzzer_active",
  }),
});

function resolveBuzzerChannel(comp, ctx) {
  const resolvedId = resolvePluginInstanceId(comp, identity.type);
  const ch =
    ctx.pluginChannels?.[comp.id] ??
    ctx.pluginChannels?.[resolvedId] ??
    ctx.pluginChannels?.[`${identity.type}:0`] ??
    ctx.pluginChannels?.[identity.type] ??
    {};
  return ch;
}

// =============================================================================
// Peripheral Definition
// =============================================================================
export const buzzerDefinition = definePeripheral({
  type: identity.type,
  size: { width: 75, height: 85 },
  wireColor: "#f59e0b",
  pinsOverlay: BUZZER_TOPOLOGIES.passive_pwm.pinsOverlay,
  props: {
    variant: {
      type: "string",
      default: "passive_pwm",
      options: ["passive_pwm", "active_gpio"],
      description: "Buzzer topology variant",
    },
    appearanceId: {
      type: "string",
      default: "buzzer_passive",
      description: "Display appearance id",
    },
    defaultFreqHz: {
      type: "number",
      default: 2000,
      range: { min: 20, max: 8000, step: 10 },
      description: "Default sounding frequency in Hz",
    },
    pwmChannel: {
      type: "number",
      default: 0,
      range: { min: 0, max: 15, step: 1 },
      description: "PWM channel (for passive_pwm)",
    },
    activeHigh: {
      type: "boolean",
      default: true,
      description: "Trigger polarity (active-high vs active-low)",
    },
    label: {
      type: "string",
      default: "Buzzer",
      description: "Label text",
    },
  },
  canvas: CanvasGlyph,
  world: WorldWidget,
  ui: {
    canvasProps: (comp, ctx) => {
      const ch = resolveBuzzerChannel(comp, ctx);
      return {
        id: comp.id,
        hasSignal: Boolean(ch.hasSignal),
        frequency: typeof ch.frequency === "number" ? ch.frequency : 0,
        duty: typeof ch.duty === "number" ? ch.duty : 0,
        label: comp.props?.label ?? "Buzzer",
        pinConnections: comp.pinConnections,
      };
    },
    worldProps: (comp, ctx) => {
      const ch = resolveBuzzerChannel(comp, ctx);
      return {
        id: comp.id,
        hasSignal: Boolean(ch.hasSignal),
        frequency: typeof ch.frequency === "number" ? ch.frequency : 0,
        duty: typeof ch.duty === "number" ? ch.duty : 0,
        label: comp.props?.label ?? "Buzzer",
        variant: comp.props?.variant ?? "passive_pwm",
      };
    },
  },
});

export default buzzerDefinition;
