import { BaseSimulationPlugin as e, LogicStates as t, defaultRolePinName as n, normalizeManifest as r, normalizeVariantKey as i, resolveMappedRolePinName as a, resolvePluginIdentity as o } from "@wink-ai/unisim-sdk";
//#region builtin/buzzer/1.0.0/src/simulation.ts
var s = o(import.meta.url, "buzzer", "1.0.0", "output"), c = {
	passive_pwm: {
		displayName: "Passive Piezo Buzzer (PWM)",
		pins: [{
			name: "1",
			pinType: "digital_in",
			catalogType: "pwm",
			role: "pwm",
			aliases: [
				"1",
				"sig",
				"signal",
				"pwm",
				"anode",
				"pos"
			],
			required: !0
		}, {
			name: "2",
			pinType: "gnd",
			role: "gnd",
			aliases: [
				"2",
				"gnd",
				"ground",
				"cathode",
				"neg"
			],
			required: !1
		}]
	},
	active_gpio: {
		displayName: "Active Buzzer (GPIO)",
		pins: [{
			name: "1",
			pinType: "digital_in",
			role: "signal",
			aliases: [
				"1",
				"sig",
				"signal",
				"gpio",
				"anode",
				"pos"
			],
			required: !0
		}, {
			name: "2",
			pinType: "gnd",
			role: "gnd",
			aliases: [
				"2",
				"gnd",
				"ground",
				"cathode",
				"neg"
			],
			required: !1
		}]
	}
};
function l(e) {
	let t = i(e);
	return t && t in c ? t : "passive_pwm";
}
function u(e = "passive_pwm") {
	let t = l(e), n = c[t] ?? c.passive_pwm;
	return r({
		type: s.type,
		version: s.version,
		category: s.category,
		displayName: n.displayName,
		description: "Acoustic buzzer component supporting passive PWM, active GPIO, and auto-detecting periodic square wave tone synthesis",
		timingModel: "event-driven",
		pins: n.pins,
		properties: {
			variant: {
				type: "string",
				default: t,
				enum: ["passive_pwm", "active_gpio"]
			},
			defaultFreqHz: {
				type: "number",
				default: 2e3,
				min: 20,
				max: 8e3,
				unit: "Hz"
			},
			pwmChannel: {
				type: "number",
				default: 0,
				min: 0,
				max: 15
			},
			activeHigh: {
				type: "boolean",
				default: !0
			},
			label: {
				type: "string",
				default: "Buzzer"
			}
		},
		stateChannels: {
			hasSignal: {
				type: "boolean",
				default: !1,
				description: "Whether buzzer is currently buzzing or receiving driving signal"
			},
			frequency: {
				type: "number",
				default: 0,
				unit: "Hz",
				description: "Current sound frequency in Hz (0 = quiet)"
			},
			duty: {
				type: "number",
				default: 0,
				unit: "%",
				description: "Current PWM duty cycle percentage"
			}
		},
		events: {
			PLAY_TONE: {
				description: "Play tone at specified frequency in Hz",
				params: { freqHz: {
					type: "number",
					required: !1,
					unit: "Hz"
				} }
			},
			STOP_TONE: {
				description: "Stop tone and silence buzzer",
				params: {}
			},
			SET_SIGNAL: {
				description: "Set buzzer sounding state",
				params: { hasSignal: {
					type: "boolean",
					required: !0
				} }
			}
		}
	});
}
var d = u("passive_pwm"), f = (e) => u(l(e)), p = class extends e {
	manifest = d;
	static manifest = d;
	hasSignal = !1;
	frequency = 0;
	duty = 0;
	signalPinName = "1";
	signalMcuPin = -1;
	edgeCountInQuantum = 0;
	currentPinActive = !1;
	silenceQuantaCount = 0;
	driveMode = "quiet";
	accumulatedEdges = 0;
	accumulatedUs = 0;
	get type() {
		return this.manifest.type;
	}
	onBound(e, t, r) {
		let i = a(this.manifest, "pwm", t) ?? a(this.manifest, "signal", t) ?? n(this.manifest, "pwm") ?? n(this.manifest, "signal") ?? "1";
		this.signalPinName = i;
		let o = t?.["1"] ?? t?.sig ?? t?.signal ?? t?.pwm ?? t?.[i];
		return this.signalMcuPin = o === void 0 ? -1 : typeof o == "number" ? o : parseInt(String(o), 10), console.log("[Buzzer Sim] onBound with pinMapping:", t, "signalMcuPin:", this.signalMcuPin), this.hasSignal = !1, this.frequency = 0, this.duty = 0, this.driveMode = "quiet", this.edgeCountInQuantum = 0, this.currentPinActive = !1, this.silenceQuantaCount = 0, this.accumulatedEdges = 0, this.accumulatedUs = 0, this.ctx?.publish("hasSignal", !1), this.ctx?.publish("frequency", 0), this.ctx?.publish("duty", 0), {
			hasSignal: !1,
			frequency: 0,
			duty: 0
		};
	}
	updateSoundState(e, t, n) {
		let r = Math.abs(this.frequency - t);
		(this.hasSignal !== e || e && (this.frequency === 0 || r > 25) || !e && this.frequency !== 0 || this.duty !== n) && (this.hasSignal = e, this.frequency = t, this.duty = n, console.log("[Buzzer Sim] State change -> hasSignal:", e, "freq:", t, "duty:", n), this.ctx?.publish("hasSignal", this.hasSignal), this.ctx?.publish("frequency", this.frequency), this.ctx?.publish("duty", this.duty));
	}
	onDutyChange(e, t) {
		l(this.properties?.variant) === "passive_pwm" && e === Number(this.properties?.pwmChannel ?? 0) && (t > 0 ? (this.driveMode = "pwm", this.updateSoundState(!0, Number(this.properties?.defaultFreqHz ?? 2e3), t)) : (this.driveMode = "quiet", this.updateSoundState(!1, 0, 0)));
	}
	onPinChange(e, n, r) {
		if (this.signalMcuPin >= 0 && e !== this.signalMcuPin) return;
		let i = n === t.HIGH || n === !0, a = this.properties?.activeHigh !== !1;
		this.currentPinActive = a ? i : !i, this.edgeCountInQuantum++;
	}
	onStep(e, t) {
		let n = this.edgeCountInQuantum;
		this.edgeCountInQuantum = 0;
		let r = Number(t) > 0 ? Number(t) : 1e3;
		if (this.driveMode === "gpio_dc" && !this.currentPinActive) {
			this.driveMode = "quiet", this.accumulatedEdges = 0, this.accumulatedUs = 0, this.silenceQuantaCount = 0, this.updateSoundState(!1, 0, 0);
			return;
		}
		if (n === 1 && this.driveMode === "quiet" && this.currentPinActive && l(this.properties?.variant) === "active_gpio") {
			this.driveMode = "gpio_dc", this.accumulatedEdges = 0, this.accumulatedUs = 0, this.silenceQuantaCount = 0, this.updateSoundState(!0, Number(this.properties?.defaultFreqHz ?? 2e3), 100);
			return;
		}
		if (n > 0) {
			this.accumulatedEdges += n, this.accumulatedUs += r, this.driveMode = "gpio_pulse_train", this.silenceQuantaCount = 0;
			let e = this.accumulatedEdges * 1e6 / (2 * this.accumulatedUs), t = Math.round(e);
			Math.abs(t - 1e4) <= 600 ? t = 1e4 : Math.abs(t - 2e3) <= 150 ? t = 2e3 : Math.abs(t - 4e3) <= 250 && (t = 4e3), this.updateSoundState(!0, t, 50), this.accumulatedUs >= 4e3 && (this.accumulatedEdges = 0, this.accumulatedUs = 0);
		} else this.driveMode === "gpio_pulse_train" ? (this.silenceQuantaCount++, this.silenceQuantaCount >= 15 && (this.driveMode = "quiet", this.accumulatedEdges = 0, this.accumulatedUs = 0, this.updateSoundState(!1, 0, 0))) : this.driveMode === "gpio_dc" ? this.currentPinActive || (this.driveMode = "quiet", this.updateSoundState(!1, 0, 0)) : this.driveMode === "quiet" && this.currentPinActive && l(this.properties?.variant) === "active_gpio" && (this.driveMode = "gpio_dc", this.updateSoundState(!0, Number(this.properties?.defaultFreqHz ?? 2e3), 100));
	}
	_playTone(e) {
		let t = 0;
		typeof e == "number" ? t = e : e && typeof e == "object" && (t = e.freqHz ?? e.frequency ?? 0), t ||= Number(this.properties?.defaultFreqHz ?? 2e3), t > 0 ? (this.driveMode = "pwm", this.updateSoundState(!0, t, 50)) : (this.driveMode = "quiet", this.updateSoundState(!1, 0, 0));
	}
	_stopTone() {
		this.driveMode = "quiet", this.updateSoundState(!1, 0, 0);
	}
	_signal(e) {
		let t = !0;
		typeof e == "boolean" ? t = e : e && typeof e == "object" && "hasSignal" in e && (t = !!e.hasSignal), t ? (this.driveMode = "gpio_dc", this.updateSoundState(!0, Number(this.properties?.defaultFreqHz ?? 2e3), 50)) : (this.driveMode = "quiet", this.updateSoundState(!1, 0, 0));
	}
	onDestroy() {
		this.ctx?.gpio ? this.ctx.gpio.releasePin(this.signalPinName) : this.ctx?.releasePin(this.signalPinName), super.onDestroy();
	}
}, m = {
	manifest: d,
	manifestFactory: f,
	PluginClass: p
};
//#endregion
export { c as BUZZER_PIN_VARIANTS, p as BuzzerPlugin, d as buzzerManifest, f as buzzerManifestFactory, u as createBuzzerManifest, m as default };
