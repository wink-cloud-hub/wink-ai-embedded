import { BaseSimulationPlugin as e, normalizeManifest as t, resolvePluginIdentity as n } from "@wink-ai/unisim-sdk";
//#region builtin/ntc/1.0.0/src/simulation.ts
var r = n(import.meta.url, "ntc", "1.0.0", "sensor");
function i(e = "default") {
	return t({
		type: r.type,
		version: r.version,
		category: r.category,
		displayName: "NTC Temperature Sensor",
		description: "NTC thermistor analog temperature sensor module with pull-up divider network",
		timingModel: "event-driven",
		pins: [
			{
				name: "OUT",
				direction: "source",
				signal: "analog",
				role: "signal",
				aliases: [
					"out",
					"AO",
					"ao",
					"SIG",
					"sig"
				],
				required: !0
			},
			{
				name: "VCC",
				pinType: "vcc",
				role: "power",
				aliases: ["5v", "vcc"],
				required: !1
			},
			{
				name: "GND",
				pinType: "gnd",
				role: "ground",
				aliases: ["gnd"],
				required: !1
			}
		],
		properties: {
			temperature: {
				type: "number",
				default: 25,
				min: -40,
				max: 200
			},
			r25: {
				type: "number",
				default: 1e4
			},
			bValue: {
				type: "number",
				default: 3950
			},
			pullUpResistor: {
				type: "number",
				default: 1e4
			},
			variant: {
				type: "string",
				default: e
			},
			label: {
				type: "string",
				default: ""
			},
			vref: {
				type: "number",
				default: 3,
				description: "Nominal reference voltage in volts"
			},
			vrefNoisePct: {
				type: "number",
				default: 0,
				min: 0,
				max: 5,
				description: "Vref random noise percentage (e.g. 0.1 for +/-0.1%)"
			},
			lineRegulationPct: {
				type: "number",
				default: 0,
				description: "LDO line regulation drift percentage (delta_line)"
			},
			tempDriftPpm: {
				type: "number",
				default: 0,
				description: "LDO temperature coefficient in ppm/degC (delta_temp)"
			},
			gndBouncePct: {
				type: "number",
				default: 0,
				description: "Ground bounce perturbation percentage when heater active (delta_gnd_bounce)"
			},
			heaterActive: {
				type: "boolean",
				default: !1,
				description: "Heater active state causing ground bounce"
			},
			prngSeed: {
				type: "number",
				default: 42,
				description: "Deterministic PRNG seed for noise generation"
			}
		},
		stateChannels: {
			temperature: {
				type: "number",
				default: 25,
				min: -40,
				max: 200
			},
			voltageRatio: {
				type: "number",
				default: .5,
				min: 0,
				max: 1
			},
			resistanceOhm: {
				type: "number",
				default: 1e4
			},
			vrefEffective: {
				type: "number",
				default: 3
			}
		},
		events: {
			SET_TEMPERATURE: { params: { temperature: {
				type: "number",
				default: 25,
				min: -40,
				max: 200,
				unit: "°C"
			} } },
			SET_VREF_PERTURBATION: { params: {
				vref_noise_pct: { type: "number" },
				line_regulation_pct: { type: "number" },
				temp_drift_ppm: { type: "number" },
				gnd_bounce_pct: { type: "number" },
				heater_active: { type: "boolean" }
			} },
			SET_HEATER_ACTIVE: { params: { active: {
				type: "boolean",
				required: !0
			} } }
		}
	});
}
var a = i(), o = (e) => i(e || "default"), s = class extends e {
	get type() {
		return r.type;
	}
	manifest = a;
	static manifest = a;
	_outPinName = "OUT";
	_tempC = 25;
	_r25 = 1e4;
	_bValue = 3950;
	_pullUpResistor = 1e4;
	_vrefNominal = 3;
	_vrefNoisePct = 0;
	_lineRegulationPct = 0;
	_tempDriftPpm = 0;
	_gndBouncePct = 0;
	_heaterOn = !1;
	_prngSeed = 42;
	_prngState = 42;
	onBound(e, t, n) {
		return super.onBound?.(e, t, n), "OUT" in t ? this._outPinName = "OUT" : "out" in t ? this._outPinName = "out" : "AO" in t ? this._outPinName = "AO" : "ao" in t ? this._outPinName = "ao" : "SIG" in t ? this._outPinName = "SIG" : "sig" in t && (this._outPinName = "sig"), this._readProps(n), this.updateTemperature(this._tempC);
	}
	onDestroy() {}
	_readProps(e) {
		e?.r25 !== void 0 && (this._r25 = Number(e.r25) || 1e4), e?.bValue !== void 0 && (this._bValue = Number(e.bValue) || 3950), e?.pullUpResistor !== void 0 && (this._pullUpResistor = Number(e.pullUpResistor) || 1e4), e?.temperature !== void 0 && (this._tempC = Number(e.temperature)), e?.vref !== void 0 && (this._vrefNominal = Number(e.vref) || 3);
		let t = e?.vref_noise_pct ?? e?.vrefNoisePct;
		t !== void 0 && (this._vrefNoisePct = Number(t) || 0);
		let n = e?.line_regulation_pct ?? e?.lineRegulationPct;
		n !== void 0 && (this._lineRegulationPct = Number(n) || 0);
		let r = e?.temp_drift_ppm ?? e?.tempDriftPpm;
		r !== void 0 && (this._tempDriftPpm = Number(r) || 0);
		let i = e?.gnd_bounce_pct ?? e?.gndBouncePct;
		i !== void 0 && (this._gndBouncePct = Number(i) || 0);
		let a = e?.heater_active ?? e?.heaterActive;
		a !== void 0 && (this._heaterOn = !!a);
		let o = e?.prng_seed ?? e?.prngSeed ?? e?.seed;
		o !== void 0 && (this._prngSeed = Number(o) || 42, this._prngState = this._prngSeed);
	}
	_prngNext() {
		return this._prngState = this._prngState * 16807 % 2147483647, (this._prngState - 1) / 2147483646 * 2 - 1;
	}
	updateTemperature(e) {
		this._tempC = Number(e);
		let t = this._tempC + 273.15, n = this._r25 * Math.exp(this._bValue * (1 / t - 1 / 298.15)), r = Math.max(0, Math.min(1, n / (n + this._pullUpResistor))), i = this._lineRegulationPct / 100, a = (this._tempC - 25) * (this._tempDriftPpm * 1e-6), o = this._heaterOn ? this._gndBouncePct / 100 : 0, s = this._vrefNoisePct > 0 ? this._prngNext() * (this._vrefNoisePct / 100) : 0, c = i + a + o + s, l = this._vrefNominal * (1 + c), u = Math.max(0, Math.min(1, r / (1 + c)));
		return this._outPinName && this.ctx && (this.ctx.adc ? this.ctx.adc.writeNorm(this._outPinName, u) : typeof this.ctx.analogWrite == "function" && this.ctx.analogWrite(this._outPinName, u), this.ctx.publish("temperature", this._tempC), this.ctx.publish("voltageRatio", u), this.ctx.publish("resistanceOhm", n), this.ctx.publish("vrefEffective", l)), {
			temperature: this._tempC,
			voltageRatio: u,
			resistanceOhm: n,
			vrefEffective: l
		};
	}
	onPropertyChange(e, t, n) {
		e === "temperature" ? this.updateTemperature(Number(n)) : (e === "r25" || e === "bValue" || e === "pullUpResistor" || e === "vref" || e === "vref_noise_pct" || e === "vrefNoisePct" || e === "line_regulation_pct" || e === "lineRegulationPct" || e === "temp_drift_ppm" || e === "tempDriftPpm" || e === "gnd_bounce_pct" || e === "gndBouncePct" || e === "heater_active" || e === "heaterActive") && (this._readProps({ [e]: n }), this.updateTemperature(this._tempC));
	}
	onEvent(e, t) {
		if (e === "SET_TEMPERATURE") {
			let e = t.temperature ?? t.value ?? t.temp;
			e !== void 0 && this.updateTemperature(Number(e));
		} else if (e === "SET_VREF_PERTURBATION") this._readProps(t), this.updateTemperature(this._tempC);
		else if (e === "SET_HEATER_ACTIVE") {
			let e = t.active ?? t.heaterActive ?? t.heater_active;
			e !== void 0 && (this._heaterOn = !!e, this.updateTemperature(this._tempC));
		}
	}
	_temperature(e) {
		let t = typeof e == "object" && e ? e.temperature ?? e.value ?? e.temp : e;
		this.updateTemperature(Number(t));
	}
	_vrefPerturbation(e) {
		typeof e == "object" && e && (this._readProps(e), this.updateTemperature(this._tempC));
	}
	_heaterActive(e) {
		let t = typeof e == "object" && e ? e.active ?? e.heater_active ?? e.heaterActive : e;
		t !== void 0 && (this._heaterOn = !!t, this.updateTemperature(this._tempC));
	}
	onPropsUpdated(e) {
		this._readProps(e);
		let t = e?.temperature;
		t === void 0 ? this.updateTemperature(this._tempC) : this.updateTemperature(Number(t));
	}
}, c = {
	manifest: a,
	manifestFactory: o,
	PluginClass: s
};
//#endregion
export { s as NtcPlugin, i as createNtcManifest, c as default, a as ntcManifest, o as ntcManifestFactory };
