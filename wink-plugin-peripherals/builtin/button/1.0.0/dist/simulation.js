import { SimpleGpioPlugin as e, normalizeManifest as t, normalizeVariantKey as n, resolvePluginIdentity as r } from "@wink-ai/unisim-sdk";
//#region builtin/button/1.0.0/src/simulation.ts
var i = r(import.meta.url, "button", "1.0.0", "input"), a = { default: {
	displayName: "GPIO Push Button",
	pins: [
		{
			name: "1.l",
			pinType: "digital_out",
			role: "signal",
			aliases: ["signal", "gpio"],
			required: !1
		},
		{
			name: "2.l",
			pinType: "gnd",
			required: !1
		},
		{
			name: "1.r",
			pinType: "digital_out",
			role: "signal",
			required: !1
		},
		{
			name: "2.r",
			pinType: "gnd",
			required: !1
		}
	]
} };
function o(e) {
	let t = n(e);
	return t && t in a ? t : "default";
}
function s(e = "default") {
	let n = a[e] ?? a.default;
	return t({
		type: i.type,
		version: i.version,
		category: i.category,
		displayName: n.displayName,
		description: "Momentary push button with dual-side package pins",
		timingModel: "event-driven",
		pins: n.pins,
		properties: {
			variant: {
				type: "string",
				default: "default"
			},
			color: {
				type: "string",
				default: "red",
				enum: [
					"red",
					"green",
					"blue",
					"yellow",
					"white",
					"black"
				]
			},
			label: {
				type: "string",
				default: ""
			},
			activeLow: {
				type: "boolean",
				default: !0
			},
			autoPollMs: {
				type: "number",
				default: 10,
				min: 0,
				max: 1e4,
				unit: "ms"
			},
			debounceMs: {
				type: "number",
				default: 20,
				min: 0,
				max: 1e3,
				unit: "ms"
			},
			longPressMs: {
				type: "number",
				default: 3e3,
				min: 0,
				max: 6e4,
				unit: "ms"
			},
			isrCounter: {
				type: "boolean",
				default: !1
			}
		},
		stateChannels: { pressed: {
			type: "boolean",
			default: !1,
			description: "Pressed state"
		} },
		events: {
			SET_PRESSED: {
				description: "Set button pressed state (atomic press/release waveform pair in timing mode)",
				params: {
					pressed: {
						type: "boolean",
						required: !0
					},
					pressDurationUs: {
						type: "number",
						default: 0,
						min: 0,
						max: 6e7,
						unit: "us",
						description: "Explicit nominal pulse width; when > 0 the press/release atomic pair is injected at press time (ADR-0068, min 30ms). 0 keeps the event-driven release"
					},
					bounceUs: {
						type: "number",
						default: 0,
						min: 0,
						max: 1e4,
						unit: "us",
						description: "Contact bounce window at the leading edge (0 = clean press)"
					},
					bounceCount: {
						type: "number",
						default: 6,
						min: 0,
						max: 64
					},
					bounceModel: {
						type: "string",
						default: "deterministic_train",
						enum: ["deterministic_train", "stochastic_fretting"],
						description: "Contact bounce dynamics model"
					},
					chatterDurationRangeUs: {
						type: "array",
						description: "[minUs, maxUs] range for non-symmetric chatter duration"
					},
					contactResistanceRangeOhm: {
						type: "array",
						description: "[minOhm, maxOhm] range for dynamic contact resistance"
					},
					pullupResistanceOhm: {
						type: "number",
						default: 32e3,
						description: "MCU internal weak pull-up resistance in Ohms"
					},
					seed: {
						type: "number",
						description: "Deterministic PRNG seed for chatter generation"
					}
				}
			},
			PRESS: {
				description: "Press button",
				params: {}
			},
			RELEASE: {
				description: "Release button",
				params: {}
			}
		}
	});
}
var c = s("default"), l = (e) => s(o(e)), u = class t extends e {
	manifest = c;
	static manifest = c;
	static MIN_PRESS_US = 30000n;
	static DEFAULT_BOUNCE_COUNT = 6;
	_pressedState = !1;
	_activeLow = !0;
	_signalPinName = "1.l";
	_waveGeneration = 0;
	_pressStartUs = 0n;
	_scheduledReleaseUs = 0n;
	onBound(e, t, n) {
		let r = super.onBound(e, t, n);
		this._activeLow = !!(n.activeLow ?? !0);
		let i = (Array.isArray(this.manifest.pins) ? this.manifest.pins : []).find((e) => (e.simRole ?? e.name).toLowerCase() === "signal")?.name ?? "1.l";
		this._signalPinName = i;
		let a = this._activeLow;
		return this.ctx?.writePin(this._signalPinName, a), this.ctx?.publish("pressed", !1), {
			...r ?? {},
			pressed: !1
		};
	}
	onReset() {
		this._waveGeneration++;
		let e = this.ctx;
		typeof e?.cancelWaveform == "function" && e.cancelWaveform(this._signalPinName, this._waveGeneration - 1), this._pressedState = !1, this._pressStartUs = 0n, this._scheduledReleaseUs = 0n;
	}
	_pressed(e) {
		let n = typeof e == "object" && e ? e : null, r = n ? !!n.pressed : !!(e ?? !0);
		this._setState(r, {
			pressDurationUs: m(n?.pressDurationUs),
			bounceUs: m(n?.bounceUs),
			bounceCount: typeof n?.bounceCount == "number" ? n.bounceCount : t.DEFAULT_BOUNCE_COUNT,
			bounceModel: n?.bounceModel,
			chatterDurationRangeUs: Array.isArray(n?.chatterDurationRangeUs) && n.chatterDurationRangeUs.length >= 2 ? [Number(n.chatterDurationRangeUs[0]), Number(n.chatterDurationRangeUs[1])] : void 0,
			contactResistanceRangeOhm: Array.isArray(n?.contactResistanceRangeOhm) && n.contactResistanceRangeOhm.length >= 2 ? [Number(n.contactResistanceRangeOhm[0]), Number(n.contactResistanceRangeOhm[1])] : void 0,
			pullupResistanceOhm: typeof n?.pullupResistanceOhm == "number" ? n.pullupResistanceOhm : 32e3,
			seed: typeof n?.seed == "number" ? n.seed : void 0
		});
	}
	_press() {
		this._pressed(!0);
	}
	_release() {
		this._pressed(!1);
	}
	_setState(e, n) {
		this._pressedState = e, this.ctx?.publish("pressed", e);
		let r = this._activeLow, i = !r, a = e ? i : r, o = +!!r, s = +!!i, c = this.ctx;
		if (!(c && typeof c.injectWaveform == "function" && typeof c.nowUs == "function" && c.accuracyMode !== "behavioral")) {
			c?.writePin?.(this._signalPinName, a);
			return;
		}
		if (e) {
			let e = c.nowUs(), t = n.bounceUs && n.bounceUs > 0n ? n.bounceUs : 0n, r = n.pressDurationUs;
			if (this._pressStartUs = e, this._waveGeneration++, n.bounceModel === "stochastic_fretting") {
				let t = d(n.seed ?? Number((e ^ 1515870810n) & 4294967295n)), i = n.chatterDurationRangeUs ?? [2e3, 8e3], a = BigInt(Math.max(500, Math.floor(i[0] + t() * (i[1] - i[0])))), l = f(e, a, t, o, s, !0, n.contactResistanceRangeOhm, n.pullupResistanceOhm);
				if (r !== void 0) {
					this._scheduledReleaseUs = e + a + r;
					let i = n.chatterDurationRangeUs ?? [5e3, 18e3], c = BigInt(Math.max(500, Math.floor(i[0] + t() * (i[1] - i[0])))), u = f(this._scheduledReleaseUs, c, t, o, s, !1, n.contactResistanceRangeOhm, n.pullupResistanceOhm);
					l.push(...u);
				} else this._scheduledReleaseUs = 0n;
				c.injectWaveform(this._signalPinName, {
					edges: l,
					generation: this._waveGeneration
				});
			} else if (r !== void 0 || t > 0n) {
				let i = p(e, t, n.bounceCount, o, s);
				this._scheduledReleaseUs = e + t + (r ?? 0n), i.push({
					tUs: this._scheduledReleaseUs,
					level: o
				}), c.injectWaveform(this._signalPinName, {
					edges: i,
					generation: this._waveGeneration
				});
			} else this._scheduledReleaseUs = 0n, c.injectWaveform(this._signalPinName, {
				edges: [{
					tUs: e,
					level: s
				}],
				generation: this._waveGeneration
			});
		} else {
			let e = this._pressStartUs + t.MIN_PRESS_US, r = c.nowUs() > e ? c.nowUs() : e;
			if (this._scheduledReleaseUs === 0n || r < this._scheduledReleaseUs) {
				if (this._waveGeneration++, n.bounceModel === "stochastic_fretting") {
					let e = d(n.seed ?? Number((r ^ 2779096485n) & 4294967295n)), t = n.chatterDurationRangeUs ?? [5e3, 18e3], i = f(r, BigInt(Math.max(500, Math.floor(t[0] + e() * (t[1] - t[0])))), e, o, s, !1, n.contactResistanceRangeOhm, n.pullupResistanceOhm);
					c.injectWaveform(this._signalPinName, {
						edges: i,
						generation: this._waveGeneration
					});
				} else c.injectWaveform(this._signalPinName, {
					edges: [{
						tUs: r,
						level: o
					}],
					generation: this._waveGeneration
				});
			}
		}
	}
};
function d(e) {
	let t = e >>> 0;
	return () => {
		t = t + 1831565813 | 0;
		let e = Math.imul(t ^ t >>> 15, 1 | t);
		return e = e + Math.imul(e ^ e >>> 7, 61 | e) ^ e, ((e ^ e >>> 14) >>> 0) / 4294967296;
	};
}
function f(e, t, n, r, i, a, o = [500, 5e3], s = 32e3) {
	let c = [], l = a ? i : r, u = a ? r : i;
	if (t <= 0n) return c.push({
		tUs: e,
		level: l
	}), c;
	let d = 6 + Math.floor(n() * 9), f = [];
	for (let e = 0; e < d; e++) f.push(.2 + n() * 1.8);
	let p = f.reduce((e, t) => e + t, 0), m = u, h = 0n, g = Number(t);
	for (let l = 0; l < d - 1; l++) {
		let u = f[l] / p, d = BigInt(Math.max(20, Math.floor(u * g)));
		if (h += d, h >= t) break;
		let _ = e + h, v = l % 2 == +!a, y = m;
		if (v) {
			let [e, t] = o, a = e + n() * (t - e), c = a / (a + s);
			c <= .3 ? y = i : c >= .7 && (y = r);
		} else y = r;
		y !== m && (c.push({
			tUs: _,
			level: y
		}), m = y);
	}
	let _ = e + t;
	return (m !== l || c.length === 0) && c.push({
		tUs: _,
		level: l
	}), c;
}
function p(e, t, n, r, i) {
	let a = [];
	if (t <= 0n) return a.push({
		tUs: e,
		level: i
	}), a;
	let o = Math.max(1, Math.min(64, Math.floor(n)));
	for (let n = 1; n <= o; n++) {
		let s = e + t * BigInt(n) / BigInt(o + 1), c = n % 2 == 1 ? r : i;
		a.push({
			tUs: s,
			level: c
		});
	}
	return a.push({
		tUs: e + t,
		level: i
	}), a;
}
function m(e) {
	if (e !== void 0) {
		if (typeof e == "number") return Number.isFinite(e) && e > 0 ? BigInt(Math.floor(e)) : void 0;
		try {
			let t = BigInt(e);
			return t > 0n ? t : void 0;
		} catch {
			return;
		}
	}
}
var h = {
	manifest: c,
	manifestFactory: l,
	PluginClass: u
};
//#endregion
export { a as BUTTON_PIN_VARIANTS, u as ButtonPlugin, p as buildPressEdges, f as buildStochasticFrettingEdges, c as buttonManifest, l as buttonManifestFactory, s as createButtonManifest, d as createMulberry32, h as default };
