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
				description: "Set button pressed state",
				params: { pressed: {
					type: "boolean",
					required: !0
				} }
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
var c = s("default"), l = (e) => s(o(e)), u = class extends e {
	manifest = c;
	static manifest = c;
	_pressedState = !1;
	_activeLow = !0;
	_signalPinName = "1.l";
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
	_pressed(e) {
		let t = typeof e == "object" && e ? !!e.pressed : !!(e ?? !0);
		this._pressedState = t, this.ctx?.publish("pressed", t);
		let n = this._activeLow ? !t : t;
		this.ctx?.writePin(this._signalPinName, n);
	}
	_press() {
		this._pressed(!0);
	}
	_release() {
		this._pressed(!1);
	}
}, d = {
	manifest: c,
	manifestFactory: l,
	PluginClass: u
};
//#endregion
export { a as BUTTON_PIN_VARIANTS, u as ButtonPlugin, c as buttonManifest, l as buttonManifestFactory, s as createButtonManifest, d as default };
