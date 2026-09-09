import { BaseSimulationPlugin as e, LogicStates as t, defaultRolePinName as n, normalizeManifest as r, normalizeVariantKey as i, resolveMappedRolePinName as a, resolvePluginIdentity as o } from "@wink-ai/unisim";
//#region builtin/buzzer/1.0.0/src/simulation.ts
var s = o(import.meta.url, "buzzer", "1.0.0", "output"), c = {
	passive_pwm: {
		displayName: "Passive Piezo Buzzer (PWM)",
		pins: [{
			name: "1",
			pinType: "pwm",
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
	lastEdgeUs = 0n;
	recentIntervalsUs = [];
	watchdogGen = 0;
	lastTransitionAtUs = 0n;
	driveMode = "quiet";
	get type() {
		return this.manifest.type;
	}
	onBound(e, t, r) {
		let i = a(this.manifest, "pwm", t) ?? a(this.manifest, "signal", t) ?? n(this.manifest, "pwm") ?? n(this.manifest, "signal") ?? "1";
		if (this.signalPinName = i, t && t[i] !== void 0) {
			let e = t[i];
			this.signalMcuPin = typeof e == "number" ? e : parseInt(String(e), 10);
		} else this.signalMcuPin = -1;
		return this.hasSignal = !1, this.frequency = 0, this.duty = 0, this.driveMode = "quiet", this.lastEdgeUs = 0n, this.recentIntervalsUs = [], this.watchdogGen = 0, this.lastTransitionAtUs = 0n, this.ctx?.publish("hasSignal", !1), this.ctx?.publish("frequency", 0), this.ctx?.publish("duty", 0), {
			hasSignal: !1,
			frequency: 0,
			duty: 0
		};
	}
	updateSoundState(e, t, n) {
		let r = this.hasSignal !== e || this.frequency !== t || this.duty !== n;
		this.hasSignal = e, this.frequency = t, this.duty = n, r && (this.ctx?.publish("hasSignal", this.hasSignal), this.ctx?.publish("frequency", this.frequency), this.ctx?.publish("duty", this.duty));
	}
	onDutyChange(e, t) {
		l(this.properties?.variant) === "passive_pwm" && e === Number(this.properties?.pwmChannel ?? 0) && (t > 0 ? (this.driveMode = "pwm", this.updateSoundState(!0, Number(this.properties?.defaultFreqHz ?? 2e3), t)) : (this.driveMode = "quiet", this.updateSoundState(!1, 0, 0)));
	}
	onPinChange(e, n, r) {
		if (this.signalMcuPin >= 0 && e !== this.signalMcuPin) return;
		let i = n === t.HIGH || n === !0, a = this.properties?.activeHigh === !1 ? !i : i;
		if (this.lastEdgeUs > 0n && r >= this.lastEdgeUs) {
			let e = Number(r - this.lastEdgeUs);
			if (e >= 25 && e <= 25e3) {
				if (this.recentIntervalsUs.push(e), this.recentIntervalsUs.length > 5 && this.recentIntervalsUs.shift(), this.recentIntervalsUs.length >= 3) {
					let e = this.recentIntervalsUs.reduce((e, t) => e + t, 0) / this.recentIntervalsUs.length;
					if (this.recentIntervalsUs.every((t) => Math.abs(t - e) <= Math.max(10, e * .3)) && e > 0) {
						let t = e * 2, n = Math.round(1e6 / t);
						this.driveMode = "gpio_pulse_train", this.updateSoundState(!0, n, 50);
					}
				}
			} else e > 25e3 && (this.recentIntervalsUs = [], a ? (this.driveMode = "gpio_dc", this.updateSoundState(!0, Number(this.properties?.defaultFreqHz ?? 2e3), 100)) : (this.driveMode = "quiet", this.updateSoundState(!1, 0, 0)));
		} else this.recentIntervalsUs = [], a ? (this.driveMode = "gpio_dc", this.updateSoundState(!0, Number(this.properties?.defaultFreqHz ?? 2e3), 100)) : (this.driveMode = "quiet", this.updateSoundState(!1, 0, 0));
		if (this.lastEdgeUs = r, this.lastTransitionAtUs = r, this.driveMode === "gpio_pulse_train") {
			let e = ++this.watchdogGen, t = this.recentIntervalsUs.length > 0 ? this.recentIntervalsUs[this.recentIntervalsUs.length - 1] : 50, n = Math.max(25e3, t * 4), r = this.ctx;
			typeof r?.deferUs == "function" && r.deferUs(BigInt(n), () => {
				this.watchdogGen === e && this.driveMode === "gpio_pulse_train" && (this.driveMode = "quiet", this.recentIntervalsUs = [], this.updateSoundState(!1, 0, 0));
			});
		}
	}
	onStep(e, t) {
		this.driveMode === "gpio_pulse_train" && this.lastTransitionAtUs > 0n && e - this.lastTransitionAtUs > 35000n && (this.driveMode = "quiet", this.recentIntervalsUs = [], this.updateSoundState(!1, 0, 0));
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
