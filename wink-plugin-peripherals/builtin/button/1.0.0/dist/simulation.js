import { SimpleGpioPlugin as e, normalizeManifest as t, normalizeVariantKey as n, resolvePluginIdentity as r } from "@wink-ai/unisim";
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
			pressDurationUs: f(n?.pressDurationUs),
			bounceUs: f(n?.bounceUs),
			bounceCount: typeof n?.bounceCount == "number" ? n.bounceCount : t.DEFAULT_BOUNCE_COUNT
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
			if (this._pressStartUs = e, this._waveGeneration++, r !== void 0 || t > 0n) {
				let i = d(e, t, n.bounceCount, o, s);
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
			let e = this._pressStartUs + t.MIN_PRESS_US, n = c.nowUs() > e ? c.nowUs() : e;
			(this._scheduledReleaseUs === 0n || n < this._scheduledReleaseUs) && (this._waveGeneration++, c.injectWaveform(this._signalPinName, {
				edges: [{
					tUs: n,
					level: o
				}],
				generation: this._waveGeneration
			}));
		}
	}
};
function d(e, t, n, r, i) {
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
function f(e) {
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
var p = {
	manifest: c,
	manifestFactory: l,
	PluginClass: u
};
//#endregion
export { a as BUTTON_PIN_VARIANTS, u as ButtonPlugin, d as buildPressEdges, c as buttonManifest, l as buttonManifestFactory, s as createButtonManifest, p as default };
