import { BaseSimulationPlugin as e, LogicStates as t, normalizeManifest as n, normalizeVariantKey as r, resolvePluginIdentity as i } from "@wink-ai/unisim";
//#region builtin/ultrasonic/1.0.0/src/physics/distance-echo-us.ts
function a(e, t) {
	let n = t || 343;
	return Math.round(e * 2e4 / n);
}
//#endregion
//#region builtin/ultrasonic/1.0.0/src/simulation.ts
var o = i(import.meta.url, "ultrasonic", "1.0.0", "sensor"), s = { default: {
	displayName: "HC-SR04 Ultrasonic Sensor",
	pins: [
		{
			name: "TRIG",
			pinType: "digital_in",
			required: !0
		},
		{
			name: "ECHO",
			pinType: "digital_out",
			required: !0
		},
		{
			name: "VCC",
			pinType: "vcc",
			required: !1
		},
		{
			name: "GND",
			pinType: "gnd",
			required: !1
		}
	]
} };
function c(e) {
	let t = r(e);
	return t && t in s ? t : "default";
}
function l(e = "default") {
	let t = s[e] ?? s.default;
	return n({
		type: o.type,
		version: o.version,
		category: o.category,
		displayName: t.displayName,
		description: "4-pin ultrasonic distance sensor (2cm-400cm range, ~1cm resolution)",
		timingModel: "event-driven",
		pins: t.pins,
		properties: {
			variant: {
				type: "string",
				default: "default"
			},
			backend: {
				type: "string",
				default: "auto"
			},
			maxDistanceCm: {
				type: "number",
				default: 400,
				min: 2,
				max: 400,
				unit: "cm"
			},
			minDistanceCm: {
				type: "number",
				default: 2,
				min: 0,
				max: 10,
				unit: "cm"
			},
			speedOfSoundMps: {
				type: "number",
				default: 343,
				min: 300,
				max: 400,
				unit: "m/s"
			},
			useRmt: {
				type: "boolean",
				default: !1
			},
			autoPollMs: {
				type: "number",
				default: 50,
				min: 0,
				max: 1e4,
				unit: "ms"
			}
		},
		stateChannels: {
			distanceCm: {
				type: "number",
				default: 100,
				unit: "cm",
				min: 2,
				max: 400,
				description: "Measured distance in centimeters"
			},
			echoUs: {
				type: "number",
				default: 0,
				unit: "us",
				description: "Echo pulse width in microseconds"
			}
		},
		events: { SET_DISTANCE_CM: {
			description: "Set the simulated distance for testing",
			params: { cm: {
				type: "number",
				required: !0,
				unit: "cm",
				min: 2,
				max: 400,
				default: 100
			} }
		} }
	});
}
var u = l("default"), d = (e) => l(c(e)), f = class extends e {
	manifest = u;
	static manifest = u;
	get type() {
		return this.manifest.type;
	}
	trigState = t.LOW;
	distanceCm = 100;
	alive = !0;
	onBound(e, t, n) {
		this.alive = !0, this.pinMapping = t;
		let r = Math.min(100, n.maxDistanceCm);
		return this.distanceCm = r, {
			distanceCm: r,
			echoUs: 0
		};
	}
	onDestroy() {
		this.alive = !1, this.ctx?.gpio ? this.ctx.gpio.releasePin("ECHO") : this.ctx?.releasePin("ECHO");
	}
	_echoGeneration = 1;
	onPinChange(e, n, r) {
		if (!this.alive || !this.ctx || !this.pinMapping) return;
		let i = typeof e == "object" && e ? e.pin : e, o = typeof e == "object" && e ? e.state : n;
		if ((typeof i == "number" ? i : parseInt(String(i), 10) || i) !== this.pinMapping.TRIG) return;
		let s = this.trigState === t.LOW, c = o === t.HIGH;
		if (this.trigState = o, s && c) {
			let e = a(this.distanceCm, this.properties?.speedOfSoundMps ?? 343);
			this.ctx.publish("echoUs", e);
			let n = this.pinMapping.ECHO;
			if (n === void 0) return;
			let r = this.ctx?.getWasmExports?.(), i = r?.pal_wasm_push_pin_event ?? r?._pal_wasm_push_pin_event;
			if (typeof i == "function") try {
				i(n, 0n, 1), i(n, BigInt(e), 0);
			} catch {
				i(n, 0, 1), i(n, e, 0);
			}
			else {
				let n = this.ctx.nowUs?.() ?? this.ctx.system?.time?.nowUs?.() ?? 0n, r = {
					edges: [{
						tUs: n,
						level: t.HIGH
					}, {
						tUs: n + BigInt(e),
						level: t.LOW
					}],
					generation: this._echoGeneration++
				};
				typeof this.ctx.injectWaveform == "function" ? this.ctx.injectWaveform("ECHO", r) : this.ctx.gpio && this.ctx.gpio.injectWaveform("ECHO", r);
			}
		}
	}
	_distanceCm(e) {
		this.distanceCm = e, this.ctx?.publish("distanceCm", e);
	}
}, p = {
	manifest: u,
	manifestFactory: d,
	PluginClass: f
};
//#endregion
export { s as ULTRASONIC_PIN_VARIANTS, f as UltrasonicPlugin, l as createUltrasonicManifest, p as default, u as ultrasonicManifest, d as ultrasonicManifestFactory };
