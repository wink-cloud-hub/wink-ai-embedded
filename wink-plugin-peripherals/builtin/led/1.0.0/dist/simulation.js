import { SimpleGpioPlugin as e, normalizeManifest as t, normalizeVariantKey as n, resolvePluginIdentity as r } from "@wink-ai/unisim-sdk";
//#region builtin/led/1.0.0/src/simulation.ts
var i = { default: {
	displayName: "GPIO LED",
	pins: [{
		name: "A",
		pinType: "digital_in",
		role: "anode",
		aliases: ["anode", "gpio"],
		required: !1
	}, {
		name: "C",
		pinType: "gnd",
		role: "cathode",
		aliases: ["cathode"],
		required: !1
	}]
} };
function a(e) {
	let t = n(e);
	return t && t in i ? t : "default";
}
var o = r(import.meta.url, "led", "1.0.0", "output");
function s(e = "default") {
	let n = i[e] ?? i.default;
	return t({
		type: o.type,
		version: o.version,
		category: o.category,
		displayName: n.displayName,
		description: "Discrete LED with anode/cathode package pins",
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
					"orange",
					"purple"
				]
			},
			brightness: {
				type: "number",
				default: 1,
				min: 0,
				max: 1
			},
			label: {
				type: "string",
				default: ""
			},
			activeHigh: {
				type: "boolean",
				default: !0
			},
			contactWelded: {
				type: "boolean",
				default: !1,
				description: "Relay/switch contact welded"
			},
			faultType: {
				type: "string",
				default: "NONE",
				enum: ["NONE", "CONTACT_WELDED"],
				description: "Fault injection type"
			},
			pullDownResistor: {
				type: "number",
				default: 0,
				description: "External pull-down resistor in ohms (e.g. 4700 for MCS-51 reset clamping)"
			}
		},
		stateChannels: {
			on: {
				type: "boolean",
				default: !1,
				description: "Lit / commanded state"
			},
			welded: {
				type: "boolean",
				default: !1,
				description: "Contact welded fault state"
			},
			contactClosed: {
				type: "boolean",
				default: !1,
				description: "Physical contact closed state"
			}
		},
		events: {
			INJECT_FAULT: {
				description: "Inject electrical or mechanical fault",
				params: { faultType: {
					type: "string",
					default: "CONTACT_WELDED"
				} }
			},
			CLEAR_FAULT: {
				description: "Clear active fault",
				params: {}
			}
		}
	});
}
var c = s("default"), l = (e) => s(a(e)), u = class extends e {
	manifest = c;
	static manifest = c;
	_isWelded = !1;
	_pinOn = !1;
	onBound(e, t, n) {
		let r = super.onBound(e, t, n) || {};
		return this._isWelded = !!(n?.welded ?? n?.contactWelded), this.ctx && (this.ctx.publish("welded", this._isWelded), this.ctx.publish("contactClosed", this._isWelded || this._pinOn)), {
			...r,
			welded: this._isWelded,
			contactClosed: this._isWelded || this._pinOn
		};
	}
	onPinChange(e, t, n) {
		let r = typeof e == "object" && e ? e.pin : e, i = typeof e == "object" && e ? e.state : t, a = typeof r == "number" ? r : parseInt(String(r), 10);
		if (this.signalMcuPin >= 0 && !isNaN(a) && a !== this.signalMcuPin) return;
		let o = i === 1 || i === !0, s = this.properties?.activeHigh ?? !0, c = this.properties?.activeLow ?? !1 ? !1 : s;
		this._pinOn = c ? o : !o, this.ctx && (this.ctx.publish("on", this._pinOn), this._isWelded && (this.ctx.publish("welded", !0), this.ctx.publish("contactClosed", !0)));
	}
	_injectFault(e) {
		let t = typeof e == "object" && e ? e.faultType ?? "CONTACT_WELDED" : e;
		(t === "CONTACT_WELDED" || t === !0) && (this._isWelded = !0, this.ctx && (this.ctx.publish("welded", !0), this.ctx.publish("contactClosed", !0)));
	}
	_clearFault() {
		this._isWelded = !1, this.ctx && (this.ctx.publish("welded", !1), this.ctx.publish("contactClosed", this._pinOn));
	}
}, d = {
	manifest: c,
	manifestFactory: l,
	PluginClass: u
};
//#endregion
export { i as LED_PIN_VARIANTS, u as LedPlugin, s as createLedManifest, d as default, c as ledManifest, l as ledManifestFactory };
