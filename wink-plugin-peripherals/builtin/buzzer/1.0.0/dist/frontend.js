import { definePeripheral as e, pinsFromBinderVariant as t, resolvePluginInstanceId as n } from "@wink-ai/unisim-ui";
import { resolvePluginIdentity as r } from "@wink-ai/unisim";
import { createCommentVNode as i, createElementBlock as a, createElementVNode as o, defineComponent as s, normalizeClass as c, onBeforeUnmount as l, onMounted as u, openBlock as d, ref as f, toDisplayString as p, watch as m } from "vue";
import "@wokwi/elements";
//#region builtin/buzzer/1.0.0/src/CanvasGlyph.vue?vue&type=script&setup=true&lang.ts
var h = ["hasSignal"], g = { class: "buzzer-badge-area" }, _ = { class: "buzzer-info-row" }, v = { class: "buzzer-label" }, y = ["title"], b = {
	key: 0,
	class: "freq-tag"
}, x = /*@__PURE__*/ s({
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
		let t = e, n = f(!1), r = null, s = null, x = null, S = null, C = null, w = null, T = null, E = 0;
		function D() {
			if (typeof window > "u") return null;
			let e = window.AudioContext || window.webkitAudioContext;
			return e ? (r || (r = new e(), console.log("[Buzzer Glyph] AudioContext created, initial state:", r.state)), r.state === "suspended" && r.resume().then(() => {
				console.log("[Buzzer Glyph] AudioContext resumed, state:", r?.state);
			}).catch((e) => {
				console.warn("[Buzzer Glyph] AudioContext resume waiting for user interaction:", e);
			}), r) : (console.warn("[Buzzer Glyph] Web Audio API not supported in this browser environment"), null);
		}
		function O(e) {
			if (n.value) return;
			let t = D();
			if (!t) return;
			T &&= (clearTimeout(T), null);
			let r = Math.max(20, Math.min(2e4, e > 0 ? e : 2e3)), i = t.currentTime;
			if (!s || !x || !w) try {
				s = t.createOscillator(), x = t.createGain(), w = t.createGain(), S = t.createOscillator(), C = t.createGain(), r >= 3e3 ? (s.type = "sine", s.frequency.setValueAtTime(r, i), x.gain.setValueAtTime(.24, i), S.type = "sine", S.frequency.setValueAtTime(2400, i), C.gain.setValueAtTime(.035, i)) : (s.type = "square", s.frequency.setValueAtTime(r, i), x.gain.setValueAtTime(.12, i), C.gain.setValueAtTime(0, i)), w.gain.setValueAtTime(1, i), s.connect(x).connect(w), S.connect(C).connect(w), w.connect(t.destination), s.start(), S.start(), E = r, console.log("[Buzzer Glyph] High-fidelity acoustic engine started at", r, "Hz");
			} catch (e) {
				console.error("[Buzzer Glyph] Failed to start audio graph:", e);
			}
			else Math.abs(E - r) > 5 && (r >= 3e3 ? (s.type = "sine", s.frequency.setValueAtTime(r, i), x.gain.setValueAtTime(.24, i), S.frequency.setValueAtTime(2400, i), C.gain.setValueAtTime(.035, i)) : (s.type = "square", s.frequency.setValueAtTime(r, i), x.gain.setValueAtTime(.12, i), C.gain.setValueAtTime(0, i)), E = r), w.gain.setValueAtTime(1, i);
		}
		function k() {
			if (w && r) {
				let e = r.currentTime;
				w.gain.setValueAtTime(0, e), T && clearTimeout(T), T = setTimeout(() => {
					if (s) {
						try {
							s.stop(), s.disconnect();
						} catch {}
						s = null;
					}
					if (S) {
						try {
							S.stop(), S.disconnect();
						} catch {}
						S = null;
					}
					if (x) {
						try {
							x.disconnect();
						} catch {}
						x = null;
					}
					if (C) {
						try {
							C.disconnect();
						} catch {}
						C = null;
					}
					if (w) {
						try {
							w.disconnect();
						} catch {}
						w = null;
					}
					E = 0, T = null;
				}, 50);
			}
		}
		function A(e) {
			e && e.stopPropagation(), n.value = !n.value, console.log("[Buzzer Glyph] Audio mute toggled to:", n.value), D(), n.value ? k() : t.hasSignal && O(t.frequency ?? 0);
		}
		function j() {
			r && r.state === "suspended" && r.resume().then(() => {
				console.log("[Buzzer Glyph] AudioContext unlocked by interaction, state:", r?.state);
			}).catch(() => {});
		}
		return u(() => {
			console.log("[Buzzer Glyph] mounted. props:", t), typeof window < "u" && (window.addEventListener("click", j, {
				capture: !0,
				passive: !0
			}), window.addEventListener("keydown", j, {
				capture: !0,
				passive: !0
			}), window.addEventListener("pointerdown", j, {
				capture: !0,
				passive: !0
			}));
		}), m(() => [
			t.hasSignal,
			t.frequency,
			n.value
		], ([e, n, r]) => {
			console.log("[Buzzer Glyph] watch triggered:", {
				id: t.id,
				hasSignal: e,
				freq: n,
				muted: r
			}), e && !r ? O(n ?? 0) : k();
		}, { immediate: !0 }), l(() => {
			typeof window < "u" && (window.removeEventListener("click", j, { capture: !0 }), window.removeEventListener("keydown", j, { capture: !0 })), k(), r &&= (r.close().catch(() => {}), null);
		}), (t, r) => (d(), a("div", {
			class: c(["buzzer-glyph-wrapper", { "is-active": e.hasSignal }]),
			onClick: D
		}, [o("wokwi-buzzer", { hasSignal: !!e.hasSignal }, null, 8, h), o("div", g, [o("div", _, [o("span", v, p(e.label || "Buzzer"), 1), o("button", {
			type: "button",
			class: "audio-toggle-btn",
			title: n.value ? "点击开启声音" : "点击静音",
			onClick: A
		}, p(n.value ? "🔇" : "🔊"), 9, y)]), e.hasSignal && e.frequency ? (d(), a("span", b, p(Math.round(e.frequency)) + "Hz ", 1)) : i("", !0)])], 2));
	}
}), S = (e, t) => {
	let n = e.__vccOpts || e;
	for (let [e, r] of t) n[e] = r;
	return n;
}, C = /*#__PURE__*/ S(x, [["__scopeId", "data-v-e63c540c"]]), w = { class: "widget-header" }, T = { class: "widget-title" }, E = { class: "metrics-grid" }, D = { class: "metric-item" }, O = { class: "metric-val" }, k = { class: "metric-item" }, A = { class: "metric-val" }, j = { class: "metric-item full-width" }, M = { class: "metric-mode" }, N = { class: "audio-control" }, P = /*#__PURE__*/ S(/* @__PURE__ */ s({
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
		let t = e, n = f(!0), r = null, i = null, s = null, h = null, g = null, _ = null, v = null, y = 0;
		function b() {
			if (typeof window > "u") return null;
			let e = window.AudioContext || window.webkitAudioContext;
			return e ? (r ||= new e(), r.state === "suspended" && r.resume().catch(() => {}), r) : null;
		}
		function x(e) {
			if (n.value) return;
			let t = b();
			if (!t) return;
			v &&= (clearTimeout(v), null);
			let r = Math.max(20, Math.min(2e4, e > 0 ? e : 2e3)), a = t.currentTime;
			if (!i || !s || !_) try {
				i = t.createOscillator(), s = t.createGain(), _ = t.createGain(), h = t.createOscillator(), g = t.createGain(), r >= 3e3 ? (i.type = "sine", i.frequency.setValueAtTime(r, a), s.gain.setValueAtTime(.24, a), h.type = "sine", h.frequency.setValueAtTime(2400, a), g.gain.setValueAtTime(.035, a)) : (i.type = "square", i.frequency.setValueAtTime(r, a), s.gain.setValueAtTime(.12, a), g.gain.setValueAtTime(0, a)), _.gain.setValueAtTime(1, a), i.connect(s).connect(_), h.connect(g).connect(_), _.connect(t.destination), i.start(), h.start(), y = r;
			} catch {}
			else Math.abs(y - r) > 5 && (r >= 3e3 ? (i.type = "sine", i.frequency.setValueAtTime(r, a), s.gain.setValueAtTime(.24, a), h.frequency.setValueAtTime(2400, a), g.gain.setValueAtTime(.035, a)) : (i.type = "square", i.frequency.setValueAtTime(r, a), s.gain.setValueAtTime(.12, a), g.gain.setValueAtTime(0, a)), y = r), _.gain.setValueAtTime(1, a);
		}
		function S() {
			if (_ && r) {
				let e = r.currentTime;
				_.gain.setValueAtTime(0, e), v && clearTimeout(v), v = setTimeout(() => {
					if (i) {
						try {
							i.stop(), i.disconnect();
						} catch {}
						i = null;
					}
					if (h) {
						try {
							h.stop(), h.disconnect();
						} catch {}
						h = null;
					}
					if (s) {
						try {
							s.disconnect();
						} catch {}
						s = null;
					}
					if (g) {
						try {
							g.disconnect();
						} catch {}
						g = null;
					}
					if (_) {
						try {
							_.disconnect();
						} catch {}
						_ = null;
					}
					y = 0, v = null;
				}, 50);
			}
		}
		function C() {
			n.value = !n.value, b(), n.value ? S() : t.hasSignal && x(t.frequency);
		}
		function P() {
			r && r.state === "suspended" && r.resume().catch(() => {});
		}
		return u(() => {
			typeof window < "u" && (window.addEventListener("click", P, {
				capture: !0,
				passive: !0
			}), window.addEventListener("keydown", P, {
				capture: !0,
				passive: !0
			}));
		}), m(() => [
			t.hasSignal,
			t.frequency,
			n.value
		], ([e, t, n]) => {
			e && !n ? x(t) : S();
		}, { immediate: !0 }), l(() => {
			typeof window < "u" && (window.removeEventListener("click", P, { capture: !0 }), window.removeEventListener("keydown", P, { capture: !0 })), S(), r &&= (r.close().catch(() => {}), null);
		}), (t, r) => (d(), a("div", { class: c(["buzzer-world-widget", { "is-active": e.hasSignal }]) }, [
			o("div", w, [o("div", T, [r[0] ||= o("span", { class: "icon" }, "🔔", -1), o("span", null, p(e.label || "Buzzer"), 1)]), o("span", { class: c(["state-pill", e.hasSignal ? "state-on" : "state-off"]) }, p(e.hasSignal ? "SOUNDING" : "QUIET"), 3)]),
			o("div", E, [
				o("div", D, [r[1] ||= o("span", { class: "metric-label" }, "Frequency", -1), o("span", O, p(e.hasSignal && e.frequency ? `${Math.round(e.frequency)} Hz` : "0 Hz"), 1)]),
				o("div", k, [r[2] ||= o("span", { class: "metric-label" }, "Duty", -1), o("span", A, p(e.hasSignal ? `${Math.round(e.duty)}%` : "0%"), 1)]),
				o("div", j, [r[3] ||= o("span", { class: "metric-label" }, "Mode", -1), o("span", M, p(e.variant), 1)])
			]),
			o("div", N, [o("button", {
				class: c(["sound-toggle-btn", { "is-unmuted": !n.value }]),
				type: "button",
				onClick: C
			}, [o("span", null, p(n.value ? "🔇 Audio Muted" : "🔊 Audio Active"), 1)], 2)])
		], 2));
	}
}), [["__scopeId", "data-v-13f5b533"]]);
Object.freeze({});
var F = Object.freeze({
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
}), I = Object.freeze([{
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
}]), L = Object.freeze([{
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
function R(e) {
	try {
		let n = t("buzzer", e);
		if (n && n.length > 0) return n;
	} catch {}
	return e === "active_gpio" ? L : I;
}
var z = Object.freeze({
	passive_pwm: Object.freeze({
		variant: "passive_pwm",
		getPins: () => R("passive_pwm"),
		pinsOverlay: F,
		defaultAppearanceId: "buzzer_passive"
	}),
	active_gpio: Object.freeze({
		variant: "active_gpio",
		getPins: () => R("active_gpio"),
		pinsOverlay: F,
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
var B = r(import.meta.url, "buzzer", "1.0.0", "output");
function V(e, t) {
	let r = n(e, B.type);
	return t.pluginChannels?.[e.id] ?? t.pluginChannels?.[r] ?? t.pluginChannels?.[`${B.type}:0`] ?? t.pluginChannels?.[B.type] ?? {};
}
var H = z.passive_pwm, U = e({
	type: B.type,
	size: {
		width: 75,
		height: 85
	},
	wireColor: "#f59e0b",
	pinsOverlay: H.pinsOverlay,
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
	canvas: C,
	world: P,
	ui: {
		canvasProps: (e, t) => {
			let n = V(e, t);
			return {
				id: e.id,
				hasSignal: t.isRunning !== !1 && !!n.hasSignal,
				frequency: typeof n.frequency == "number" ? n.frequency : 0,
				duty: typeof n.duty == "number" ? n.duty : 0,
				label: e.props?.label ?? "Buzzer",
				pinConnections: e.pinConnections
			};
		},
		worldProps: (e, t) => {
			let n = V(e, t);
			return {
				id: e.id,
				hasSignal: t.isRunning !== !1 && !!n.hasSignal,
				frequency: typeof n.frequency == "number" ? n.frequency : 0,
				duty: typeof n.duty == "number" ? n.duty : 0,
				label: e.props?.label ?? "Buzzer",
				variant: e.props?.variant ?? "passive_pwm"
			};
		}
	}
});
//#endregion
export { U as buzzerDefinition, U as default };
