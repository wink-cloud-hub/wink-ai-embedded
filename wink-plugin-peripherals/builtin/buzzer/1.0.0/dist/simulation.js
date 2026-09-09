import { BaseSimulationPlugin, LogicStates, defaultRolePinName, normalizeManifest, normalizeVariantKey, resolveMappedRolePinName, resolvePluginIdentity } from "@wink-ai/unisim";

const identity = resolvePluginIdentity(import.meta.url, "buzzer", "1.0.0", "output");

export const BUZZER_PIN_VARIANTS = {
  passive_pwm: {
    displayName: "Passive Piezo Buzzer (PWM)",
    pins: [
      {
        name: "1",
        pinType: "digital_in",
        role: "pwm",
        aliases: ["1", "sig", "signal", "pwm", "anode", "pos"],
        required: true,
      },
      {
        name: "2",
        pinType: "gnd",
        role: "gnd",
        aliases: ["2", "gnd", "ground", "cathode", "neg"],
        required: false,
      },
    ],
  },
  active_gpio: {
    displayName: "Active Buzzer (GPIO)",
    pins: [
      {
        name: "1",
        pinType: "digital_in",
        role: "signal",
        aliases: ["1", "sig", "signal", "gpio", "anode", "pos"],
        required: true,
      },
      {
        name: "2",
        pinType: "gnd",
        role: "gnd",
        aliases: ["2", "gnd", "ground", "cathode", "neg"],
        required: false,
      },
    ],
  },
};

function resolveBuzzerVariant(raw) {
  const key = normalizeVariantKey(raw);
  return key && key in BUZZER_PIN_VARIANTS ? key : "passive_pwm";
}

export function createBuzzerManifest(variantName = "passive_pwm") {
  const resolvedVariant = resolveBuzzerVariant(variantName);
  const row = BUZZER_PIN_VARIANTS[resolvedVariant] ?? BUZZER_PIN_VARIANTS.passive_pwm;
  return normalizeManifest({
    type: identity.type,
    version: identity.version,
    category: identity.category,
    displayName: row.displayName,
    description: "Acoustic buzzer component supporting passive PWM, active GPIO, and auto-detecting periodic square wave tone synthesis",
    timingModel: "event-driven",
    pins: row.pins,
    properties: {
      variant: {
        type: "string",
        default: resolvedVariant,
        enum: ["passive_pwm", "active_gpio"],
      },
      defaultFreqHz: {
        type: "number",
        default: 2000,
        min: 20,
        max: 8000,
        unit: "Hz",
      },
      pwmChannel: {
        type: "number",
        default: 0,
        min: 0,
        max: 15,
      },
      activeHigh: {
        type: "boolean",
        default: true,
      },
      label: {
        type: "string",
        default: "Buzzer",
      },
    },
    stateChannels: {
      hasSignal: {
        type: "boolean",
        default: false,
        description: "Whether buzzer is currently buzzing or receiving driving signal",
      },
      frequency: {
        type: "number",
        default: 0,
        unit: "Hz",
        description: "Current sound frequency in Hz (0 = quiet)",
      },
      duty: {
        type: "number",
        default: 0,
        unit: "%",
        description: "Current PWM duty cycle percentage",
      },
    },
    events: {
      PLAY_TONE: {
        description: "Play tone at specified frequency in Hz",
        params: {
          freqHz: { type: "number", required: false, unit: "Hz" },
        },
      },
      STOP_TONE: {
        description: "Stop tone and silence buzzer",
        params: {},
      },
      SET_SIGNAL: {
        description: "Set buzzer sounding state",
        params: {
          hasSignal: { type: "boolean", required: true },
        },
      },
    },
  });
}

export const buzzerManifest = createBuzzerManifest("passive_pwm");
export const buzzerManifestFactory = (variant) => createBuzzerManifest(resolveBuzzerVariant(variant));

export class BuzzerPlugin extends BaseSimulationPlugin {
  manifest = buzzerManifest;
  static manifest = buzzerManifest;

  hasSignal = false;
  frequency = 0;
  duty = 0;
  signalPinName = "1";
  signalMcuPin = -1;

  edgeCountInQuantum = 0;
  currentPinActive = false;
  silenceQuantaCount = 0;
  driveMode = "quiet";

  get type() {
    return this.manifest.type;
  }

  onBound(_ctx, pinMapping, _props) {
    const rawPinName =
      resolveMappedRolePinName(this.manifest, "pwm", pinMapping) ??
      resolveMappedRolePinName(this.manifest, "signal", pinMapping) ??
      defaultRolePinName(this.manifest, "pwm") ??
      defaultRolePinName(this.manifest, "signal") ??
      "1";

    this.signalPinName = rawPinName;

    const mappedPin =
      pinMapping?.["1"] ??
      pinMapping?.["sig"] ??
      pinMapping?.["signal"] ??
      pinMapping?.["pwm"] ??
      pinMapping?.[rawPinName];

    if (mappedPin !== undefined) {
      this.signalMcuPin = typeof mappedPin === "number" ? mappedPin : parseInt(String(mappedPin), 10);
    } else {
      this.signalMcuPin = -1;
    }

    console.log("[Buzzer Sim] onBound with pinMapping:", pinMapping, "signalMcuPin:", this.signalMcuPin);

    this.hasSignal = false;
    this.frequency = 0;
    this.duty = 0;
    this.driveMode = "quiet";
    this.edgeCountInQuantum = 0;
    this.currentPinActive = false;
    this.silenceQuantaCount = 0;

    this.ctx?.publish("hasSignal", false);
    this.ctx?.publish("frequency", 0);
    this.ctx?.publish("duty", 0);

    return {
      hasSignal: false,
      frequency: 0,
      duty: 0,
    };
  }

  updateSoundState(hasSignal, frequency, duty) {
    const changed =
      this.hasSignal !== hasSignal ||
      this.frequency !== frequency ||
      this.duty !== duty;

    this.hasSignal = hasSignal;
    this.frequency = frequency;
    this.duty = duty;

    if (changed) {
      console.log("[Buzzer Sim] State change -> hasSignal:", hasSignal, "freq:", frequency, "duty:", duty);
      this.ctx?.publish("hasSignal", this.hasSignal);
      this.ctx?.publish("frequency", this.frequency);
      this.ctx?.publish("duty", this.duty);
    }
  }

  onDutyChange(channel, dutyPercent) {
    if (resolveBuzzerVariant(this.properties?.variant) !== "passive_pwm") return;
    if (channel !== Number(this.properties?.pwmChannel ?? 0)) return;

    if (dutyPercent > 0) {
      this.driveMode = "pwm";
      this.updateSoundState(true, Number(this.properties?.defaultFreqHz ?? 2000), dutyPercent);
    } else {
      this.driveMode = "quiet";
      this.updateSoundState(false, 0, 0);
    }
  }

  onPinChange(pin, level, atUs) {
    if (this.signalMcuPin >= 0 && pin !== this.signalMcuPin) {
      return;
    }

    const isHigh = level === LogicStates.HIGH || level === true;
    const activeHigh = this.properties?.activeHigh !== false;
    this.currentPinActive = activeHigh ? isHigh : !isHigh;
    this.edgeCountInQuantum++;
  }

  onStep(nowUs, dtUs) {
    const edges = this.edgeCountInQuantum;
    this.edgeCountInQuantum = 0;
    const dtUsNum = Number(dtUs) > 0 ? Number(dtUs) : 1000;

    if (edges >= 2) {
      const measuredFreq = Math.round((edges * 1000000) / (2 * dtUsNum));
      this.driveMode = "gpio_pulse_train";
      this.silenceQuantaCount = 0;
      this.updateSoundState(true, measuredFreq, 50);
    } else if (edges === 1) {
      this.driveMode = "gpio_pulse_train";
      this.silenceQuantaCount = 0;
      if (!this.hasSignal) {
        this.updateSoundState(true, Number(this.properties?.defaultFreqHz ?? 2000), 50);
      }
    } else {
      if (this.driveMode === "gpio_pulse_train") {
        this.silenceQuantaCount++;
        if (this.silenceQuantaCount >= 3) {
          console.log("[Buzzer Sim] Pulse train ended -> quiet");
          this.driveMode = "quiet";
          this.updateSoundState(false, 0, 0);
        }
      } else if (this.driveMode === "gpio_dc") {
        if (!this.currentPinActive) {
          this.driveMode = "quiet";
          this.updateSoundState(false, 0, 0);
        }
      } else if (this.driveMode === "quiet") {
        if (this.currentPinActive && resolveBuzzerVariant(this.properties?.variant) === "active_gpio") {
          this.driveMode = "gpio_dc";
          this.updateSoundState(true, Number(this.properties?.defaultFreqHz ?? 2000), 100);
        }
      }
    }
  }

  _playTone(args) {
    let targetFreq = 0;
    if (typeof args === "number") targetFreq = args;
    else if (args && typeof args === "object") targetFreq = args.freqHz ?? args.frequency ?? 0;
    targetFreq = targetFreq || Number(this.properties?.defaultFreqHz ?? 2000);
    if (targetFreq > 0) {
      this.driveMode = "pwm";
      this.updateSoundState(true, targetFreq, 50);
    } else {
      this.driveMode = "quiet";
      this.updateSoundState(false, 0, 0);
    }
  }

  _stopTone() {
    this.driveMode = "quiet";
    this.updateSoundState(false, 0, 0);
  }

  _signal(args) {
    let targetState = true;
    if (typeof args === "boolean") targetState = args;
    else if (args && typeof args === "object" && "hasSignal" in args) targetState = Boolean(args.hasSignal);
    if (targetState) {
      this.driveMode = "gpio_dc";
      this.updateSoundState(true, Number(this.properties?.defaultFreqHz ?? 2000), 50);
    } else {
      this.driveMode = "quiet";
      this.updateSoundState(false, 0, 0);
    }
  }

  onDestroy() {
    if (this.ctx?.gpio) {
      this.ctx.gpio.releasePin(this.signalPinName);
    } else {
      this.ctx?.releasePin?.(this.signalPinName);
    }
    super.onDestroy();
  }
}

export default {
  manifest: buzzerManifest,
  manifestFactory: buzzerManifestFactory,
  PluginClass: BuzzerPlugin,
};
