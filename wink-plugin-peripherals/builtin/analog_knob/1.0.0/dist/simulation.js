import { BaseSimulationPlugin as e, normalizeManifest as t, resolvePluginIdentity as n } from "@wink-ai/unisim-sdk";
//#region src/simulation.ts
var r = n(import.meta.url, "analog_knob", "1.0.0", "input");
function i(e = "standard") {
	return t({
		type: r.type,
		version: r.version,
		category: r.category,
		displayName: e === "slide" ? "Slide Potentiometer" : "Analog Potentiometer Knob",
		description: "Analog potentiometer input knob or slider",
		timingModel: "event-driven",
		pins: [
			{
				name: "SIG",
				direction: "source",
				signal: "analog",
				role: "signal",
				aliases: ["sig", "out"],
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
			value: {
				type: "number",
				default: 0,
				min: 0,
				max: 100
			},
			variant: {
				type: "string",
				default: e
			}
		},
		stateChannels: { value: {
			type: "number",
			default: 0,
			min: 0,
			max: 100
		} },
		events: {}
	});
}
var a = i(), o = (e) => i(e || "standard"), s = class extends e {
	get type() {
		return r.type;
	}
	manifest = a;
	static manifest = a;
	_sigPinName = "SIG";
	onBound(e, t, n) {
		super.onBound?.(e, t, n), "SIG" in t ? this._sigPinName = "SIG" : "sig" in t ? this._sigPinName = "sig" : "out" in t && (this._sigPinName = "out");
		let r = Number(n?.position ?? n?.value ?? 0);
		return this.onSetPosition(r), { voltageRatio: Math.max(0, Math.min(100, r)) / 100 };
	}
	onDestroy() {}
	onSetPosition(e) {
		let t = Math.max(0, Math.min(100, Number(e))), n = t / 100;
		this._sigPinName && this.ctx && (this.ctx.adc ? this.ctx.adc.writeNorm(this._sigPinName, n) : typeof this.ctx.analogWrite == "function" && this.ctx.analogWrite(this._sigPinName, n), this.ctx.publish("voltageRatio", n), this.ctx.publish("value", t), this.ctx.publish("position", t));
	}
	onPropertyChange(e, t, n) {
		(e === "position" || e === "value") && this.onSetPosition(Number(n));
	}
	onPropsUpdated(e) {
		let t = e?.position ?? e?.value;
		t !== void 0 && this.onSetPosition(Number(t));
	}
}, c = {
	manifest: a,
	manifestFactory: o,
	PluginClass: s
};
//#endregion
export { s as AnalogKnobPlugin, a as analogKnobManifest, o as analogKnobManifestFactory, i as createAnalogKnobManifest, c as default };
