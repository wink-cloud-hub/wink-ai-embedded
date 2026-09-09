import { definePeripheral as e, pinsFromBinderVariant as t, resolvePluginInstanceId as n } from "@wink-ai/unisim-ui";
import { resolvePluginIdentity as r } from "@wink-ai/unisim";
import { createCommentVNode as i, createElementBlock as a, createElementVNode as o, defineComponent as s, normalizeClass as c, onBeforeUnmount as l, openBlock as u, ref as d, toDisplayString as f, watch as p } from "vue";
import "@wokwi/elements";
//#region builtin/buzzer/1.0.0/src/CanvasGlyph.vue?vue&type=script&setup=true&lang.ts
var m = ["hasSignal"], h = { class: "buzzer-badge-area" }, g = { class: "buzzer-label" }, _ = {
	key: 0,
	class: "freq-tag"
}, v = /*@__PURE__*/ s({
	__name: "CanvasGlyph",
	props: {
		id: {},
		hasSignal: { type: Boolean },
		frequency: {},
		duty: {},
		label: {},
		pinConnections: {}
	},
	setup(e) {
		return (t, n) => (u(), a("div", { class: c(["buzzer-glyph-wrapper", { "is-active": e.hasSignal }]) }, [o("wokwi-buzzer", { hasSignal: !!e.hasSignal }, null, 8, m), o("div", h, [o("span", g, f(e.label || "Buzzer"), 1), e.hasSignal && e.frequency ? (u(), a("span", _, f(Math.round(e.frequency)) + "Hz ", 1)) : i("", !0)])], 2));
	}
}), y = (e, t) => {
	let n = e.__vccOpts || e;
	for (let [e, r] of t) n[e] = r;
	return n;
}, b = /*#__PURE__*/ y(v, [["__scopeId", "data-v-6c51390d"]]), x = { class: "widget-header" }, S = { class: "widget-title" }, C = { class: "metrics-grid" }, w = { class: "metric-item" }, T = { class: "metric-val" }, E = { class: "metric-item" }, D = { class: "metric-val" }, O = { class: "metric-item full-width" }, k = { class: "metric-mode" }, A = { class: "audio-control" }, j = /*#__PURE__*/ y(/* @__PURE__ */ s({
	__name: "WorldWidget",
	props: {
		id: {},
		label: {},
		hasSignal: {
			type: Boolean,
			default: !1
		},
		frequency: { default: 0 },
		duty: { default: 0 },
		variant: { default: "passive_pwm" }
	},
	setup(e) {
		let t = e, n = d(!0), r = null, i = null, s = null;
		function m() {
			if (typeof window > "u") return null;
			let e = window.AudioContext || window.webkitAudioContext;
			return e ? (r ||= new e(), r.state === "suspended" && r.resume().catch(() => {}), r) : null;
		}
		function h(e) {
			if (n.value) return;
			let t = m();
			if (!t) return;
			let r = Math.max(20, Math.min(8e3, e > 0 ? e : 2e3));
			i ? i.frequency.setValueAtTime(r, t.currentTime) : (i = t.createOscillator(), s = t.createGain(), i.type = "square", i.frequency.setValueAtTime(r, t.currentTime), s.gain.setValueAtTime(.04, t.currentTime), i.connect(s), s.connect(t.destination), i.start());
		}
		function g() {
			if (i) {
				try {
					i.stop(), i.disconnect();
				} catch {}
				i = null;
			}
			if (s) {
				try {
					s.disconnect();
				} catch {}
				s = null;
			}
		}
		function _() {
			n.value = !n.value, n.value ? g() : t.hasSignal && h(t.frequency);
		}
		return p(() => [
			t.hasSignal,
			t.frequency,
			n.value
		], ([e, t, n]) => {
			e && !n ? h(t) : g();
		}, { immediate: !0 }), l(() => {
			g(), r &&= (r.close().catch(() => {}), null);
		}), (t, r) => (u(), a("div", { class: c(["buzzer-world-widget", { "is-active": e.hasSignal }]) }, [
			o("div", x, [o("div", S, [r[0] ||= o("span", { class: "icon" }, "🔔", -1), o("span", null, f(e.label || "Buzzer"), 1)]), o("span", { class: c(["state-pill", e.hasSignal ? "state-on" : "state-off"]) }, f(e.hasSignal ? "SOUNDING" : "QUIET"), 3)]),
			o("div", C, [
				o("div", w, [r[1] ||= o("span", { class: "metric-label" }, "Frequency", -1), o("span", T, f(e.hasSignal && e.frequency ? `${Math.round(e.frequency)} Hz` : "0 Hz"), 1)]),
				o("div", E, [r[2] ||= o("span", { class: "metric-label" }, "Duty", -1), o("span", D, f(e.hasSignal ? `${Math.round(e.duty)}%` : "0%"), 1)]),
				o("div", O, [r[3] ||= o("span", { class: "metric-label" }, "Mode", -1), o("span", k, f(e.variant), 1)])
			]),
			o("div", A, [o("button", {
				class: c(["sound-toggle-btn", { "is-unmuted": !n.value }]),
				type: "button",
				onClick: _
			}, [o("span", null, f(n.value ? "🔇 Audio Muted" : "🔊 Audio Active"), 1)], 2)])
		], 2));
	}
}), [["__scopeId", "data-v-5b461eb4"]]);
Object.freeze({});
var M = Object.freeze({
	1: Object.freeze({
		relX: 30,
		relY: 82,
		wireNet: "primary",
		defaultConnection: null,
		required: !0
	}),
	2: Object.freeze({
		relX: 34,
		relY: 82,
		wireNet: "gnd",
		defaultConnection: "GND",
		required: !1
	})
}), N = Object.freeze([{
	name: "1",
	direction: "sink",
	signal: "analog",
	catalogType: "pwm",
	simRole: "pwm",
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
	direction: "ground",
	signal: "power",
	catalogType: "power",
	simRole: "gnd",
	aliases: [
		"2",
		"gnd",
		"ground",
		"cathode",
		"neg"
	],
	required: !1
}]), P = Object.freeze([{
	name: "1",
	direction: "sink",
	signal: "digital",
	catalogType: "gpio",
	simRole: "signal",
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
	direction: "ground",
	signal: "power",
	catalogType: "power",
	simRole: "gnd",
	aliases: [
		"2",
		"gnd",
		"ground",
		"cathode",
		"neg"
	],
	required: !1
}]);
function F(e) {
	try {
		let n = t("buzzer", e);
		if (n && n.length > 0) return n;
	} catch {}
	return e === "active_gpio" ? P : N;
}
var I = Object.freeze({
	passive_pwm: Object.freeze({
		variant: "passive_pwm",
		getPins: () => F("passive_pwm"),
		pinsOverlay: M,
		defaultAppearanceId: "buzzer_passive"
	}),
	active_gpio: Object.freeze({
		variant: "active_gpio",
		getPins: () => F("active_gpio"),
		pinsOverlay: M,
		defaultAppearanceId: "buzzer_active"
	})
});
Object.freeze({
	buzzer_passive: Object.freeze({
		appearanceId: "buzzer_passive",
		variant: "passive_pwm",
		displayName: "Passive Piezo Buzzer (PWM)",
		searchAliases: Object.freeze([
			"buzzer",
			"passive_buzzer",
			"piezo",
			"speaker",
			"pwm",
			"passive_pwm",
			"default"
		])
	}),
	buzzer_active: Object.freeze({
		appearanceId: "buzzer_active",
		variant: "active_gpio",
		displayName: "Active Buzzer (GPIO)",
		searchAliases: Object.freeze([
			"buzzer",
			"active_buzzer",
			"beeper",
			"alarm",
			"gpio",
			"active_gpio"
		])
	})
});
//#endregion
//#region builtin/buzzer/1.0.0/src/definition.ts
var L = r(import.meta.url, "buzzer", "1.0.0", "output");
function R(e, t) {
	let r = n(e, L.type);
	return t.pluginChannels?.[r] || {};
}
var z = I.passive_pwm, B = e({
	type: L.type,
	size: {
		width: 75,
		height: 85
	},
	wireColor: "#f59e0b",
	pinsOverlay: z.pinsOverlay,
	props: {
		variant: {
			type: "string",
			default: "passive_pwm",
			options: ["passive_pwm", "active_gpio"],
			description: "Buzzer topology variant"
		},
		appearanceId: {
			type: "string",
			default: "buzzer_passive",
			description: "Display appearance id"
		},
		defaultFreqHz: {
			type: "number",
			default: 2e3,
			range: {
				min: 20,
				max: 8e3,
				step: 10
			},
			description: "Default sounding frequency in Hz"
		},
		pwmChannel: {
			type: "number",
			default: 0,
			range: {
				min: 0,
				max: 15,
				step: 1
			},
			description: "PWM channel (for passive_pwm)"
		},
		activeHigh: {
			type: "boolean",
			default: !0,
			description: "Trigger polarity (active-high vs active-low)"
		},
		label: {
			type: "string",
			default: "Buzzer",
			description: "Label text"
		}
	},
	canvas: b,
	world: j,
	ui: {
		canvasProps: (e, t) => {
			let n = R(e, t);
			return {
				id: e.id,
				hasSignal: !!n.hasSignal,
				frequency: typeof n.frequency == "number" ? n.frequency : 0,
				duty: typeof n.duty == "number" ? n.duty : 0,
				label: e.props?.label ?? "Buzzer",
				pinConnections: e.pinConnections
			};
		},
		worldProps: (e, t) => {
			let n = R(e, t);
			return {
				id: e.id,
				hasSignal: !!n.hasSignal,
				frequency: typeof n.frequency == "number" ? n.frequency : 0,
				duty: typeof n.duty == "number" ? n.duty : 0,
				label: e.props?.label ?? "Buzzer",
				variant: e.props?.variant ?? "passive_pwm"
			};
		}
	}
});
//#endregion
export { B as buzzerDefinition, B as default };
