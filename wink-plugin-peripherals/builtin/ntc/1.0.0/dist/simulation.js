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
			}
		},
		events: { SET_TEMPERATURE: { params: { temperature: {
			type: "number",
			default: 25,
			min: -40,
			max: 200,
			unit: "°C"
		} } } }
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
	onBound(e, t, n) {
		return super.onBound?.(e, t, n), "OUT" in t ? this._outPinName = "OUT" : "out" in t ? this._outPinName = "out" : "AO" in t ? this._outPinName = "AO" : "ao" in t ? this._outPinName = "ao" : "SIG" in t ? this._outPinName = "SIG" : "sig" in t && (this._outPinName = "sig"), this._readProps(n), this.updateTemperature(this._tempC);
	}
	onDestroy() {}
	_readProps(e) {
		e?.r25 !== void 0 && (this._r25 = Number(e.r25) || 1e4), e?.bValue !== void 0 && (this._bValue = Number(e.bValue) || 3950), e?.pullUpResistor !== void 0 && (this._pullUpResistor = Number(e.pullUpResistor) || 1e4), e?.temperature !== void 0 && (this._tempC = Number(e.temperature));
	}
	updateTemperature(e) {
		this._tempC = Number(e);
		let t = this._tempC + 273.15, n = this._r25 * Math.exp(this._bValue * (1 / t - 1 / 298.15)), r = Math.max(0, Math.min(1, n / (n + this._pullUpResistor)));
		return this._outPinName && this.ctx && (this.ctx.adc ? this.ctx.adc.writeNorm(this._outPinName, r) : typeof this.ctx.analogWrite == "function" && this.ctx.analogWrite(this._outPinName, r), this.ctx.publish("temperature", this._tempC), this.ctx.publish("voltageRatio", r), this.ctx.publish("resistanceOhm", n)), {
			temperature: this._tempC,
			voltageRatio: r,
			resistanceOhm: n
		};
	}
	onPropertyChange(e, t, n) {
		e === "temperature" ? this.updateTemperature(Number(n)) : (e === "r25" || e === "bValue" || e === "pullUpResistor") && (this._readProps({ [e]: n }), this.updateTemperature(this._tempC));
	}
	onEvent(e, t) {
		if (e === "SET_TEMPERATURE") {
			let e = t.temperature ?? t.value ?? t.temp;
			e !== void 0 && this.updateTemperature(Number(e));
		}
	}
	_temperature(e) {
		this.updateTemperature(Number(e));
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
