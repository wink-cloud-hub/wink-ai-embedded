import { definePeripheral as e, pinsFromBinderVariant as t, resolvePluginInstanceId as n } from "@wink-ai/unisim-ui";
import { resolvePluginIdentity as r } from "@wink-ai/unisim-sdk";
import { createCommentVNode as i, createElementBlock as a, createElementVNode as o, defineComponent as s, normalizeClass as c, onBeforeUnmount as l, onMounted as u, openBlock as d, ref as f, toDisplayString as p, watch as m } from "vue";
import "@wokwi/elements";
//#region builtin/buzzer/1.0.0/src/CanvasGlyph.vue?vue&type=script&setup=true&lang.ts
var h = ["hasSignal"], g = { class: "buzzer-badge-area" }, _ = { class: "buzzer-info-row" }, v = ["title"], y = {
	key: 0,
	class: "freq-tag"
}, b = /*@__PURE__*/ s({
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
		let t = e, n = f(!1), r = null, s = null, b = null, x = null, S = null, C = null, w = null, T = 0;
		function E() {
			if (typeof window > "u") return null;
			let e = window.AudioContext || window.webkitAudioContext;
			return e ? (r || (r = new e(), console.log("[Buzzer Glyph] AudioContext created, initial state:", r.state)), r.state === "suspended" && r.resume().then(() => {
				console.log("[Buzzer Glyph] AudioContext resumed, state:", r?.state);
			}).catch((e) => {
				console.warn("[Buzzer Glyph] AudioContext resume waiting for user interaction:", e);
			}), r) : (console.warn("[Buzzer Glyph] Web Audio API not supported in this browser environment"), null);
		}
		function D(e) {
			if (n.value) return;
			let t = E();
			if (!t) return;
			w &&= (clearTimeout(w), null);
			let r = Math.max(20, Math.min(2e4, e > 0 ? e : 2e3)), i = t.currentTime;
			if (!s || !b || !C) try {
				s = t.createOscillator(), b = t.createGain(), C = t.createGain(), x = t.createOscillator(), S = t.createGain(), r >= 3e3 ? (s.type = "sine", s.frequency.setValueAtTime(r, i), b.gain.setValueAtTime(.24, i), x.type = "sine", x.frequency.setValueAtTime(2400, i), S.gain.setValueAtTime(.035, i)) : (s.type = "square", s.frequency.setValueAtTime(r, i), b.gain.setValueAtTime(.12, i), S.gain.setValueAtTime(0, i)), C.gain.setValueAtTime(1, i), s.connect(b).connect(C), x.connect(S).connect(C), C.connect(t.destination), s.start(), x.start(), T = r, console.log("[Buzzer Glyph] High-fidelity acoustic engine started at", r, "Hz");
			} catch (e) {
				console.error("[Buzzer Glyph] Failed to start audio graph:", e);
			}
			else Math.abs(T - r) > 5 && (r >= 3e3 ? (s.type = "sine", s.frequency.setValueAtTime(r, i), b.gain.setValueAtTime(.24, i), x.frequency.setValueAtTime(2400, i), S.gain.setValueAtTime(.035, i)) : (s.type = "square", s.frequency.setValueAtTime(r, i), b.gain.setValueAtTime(.12, i), S.gain.setValueAtTime(0, i)), T = r), C.gain.setValueAtTime(1, i);
		}
		function O() {
			if (C && r) {
				let e = r.currentTime;
				C.gain.setValueAtTime(0, e), w && clearTimeout(w), w = setTimeout(() => {
					if (s) {
						try {
							s.stop(), s.disconnect();
						} catch {}
						s = null;
					}
					if (x) {
						try {
							x.stop(), x.disconnect();
						} catch {}
						x = null;
					}
					if (b) {
						try {
							b.disconnect();
						} catch {}
						b = null;
					}
					if (S) {
						try {
							S.disconnect();
						} catch {}
						S = null;
					}
					if (C) {
						try {
							C.disconnect();
						} catch {}
						C = null;
					}
					T = 0, w = null;
				}, 50);
			}
		}
		function k(e) {
			e && e.stopPropagation(), n.value = !n.value, console.log("[Buzzer Glyph] Audio mute toggled to:", n.value), E(), n.value ? O() : t.hasSignal && D(t.frequency ?? 0);
		}
		function A() {
			r && r.state === "suspended" && r.resume().then(() => {
				console.log("[Buzzer Glyph] AudioContext unlocked by interaction, state:", r?.state);
			}).catch(() => {});
		}
		u(() => {
			console.log("[Buzzer Glyph] mounted. props:", t), typeof window < "u" && (window.addEventListener("click", A, {
				capture: !0,
				passive: !0
			}), window.addEventListener("keydown", A, {
				capture: !0,
				passive: !0
			}), window.addEventListener("pointerdown", A, {
				capture: !0,
				passive: !0
			}), document.addEventListener("visibilitychange", j));
		});
		function j() {
			typeof document < "u" && (document.hidden ? O() : t.hasSignal && !n.value && D(t.frequency ?? 0));
		}
		return m(() => [
			t.hasSignal,
			t.frequency,
			n.value
		], ([e, n, r]) => {
			console.log("[Buzzer Glyph] watch triggered:", {
				id: t.id,
				hasSignal: e,
				freq: n,
				muted: r
			}), e && !r ? D(n ?? 0) : O();
		}, { immediate: !0 }), l(() => {
			typeof window < "u" && (window.removeEventListener("click", A, { capture: !0 }), window.removeEventListener("keydown", A, { capture: !0 }), window.removeEventListener("pointerdown", A, { capture: !0 }), document.removeEventListener("visibilitychange", j)), O(), r &&= (r.close().catch(() => {}), null);
		}), (t, r) => (d(), a("div", {
			class: c(["buzzer-glyph-wrapper", { "is-active": e.hasSignal }]),
			onClick: E
		}, [o("wokwi-buzzer", { hasSignal: !!e.hasSignal }, null, 8, h), o("div", g, [o("div", _, [o("button", {
			type: "button",
			class: "audio-toggle-btn",
			title: n.value ? "点击开启声音" : "点击静音",
			onClick: k
		}, p(n.value ? "🔇" : "🔊"), 9, v)]), e.hasSignal && e.frequency ? (d(), a("span", y, p(Math.round(e.frequency)) + "Hz ", 1)) : i("", !0)])], 2));
	}
}), x = (e, t) => {
	let n = e.__vccOpts || e;
	for (let [e, r] of t) n[e] = r;
	return n;
}, S = /*#__PURE__*/ x(b, [["__scopeId", "data-v-818734d4"]]), C = { class: "widget-header" }, w = { class: "widget-title" }, T = { class: "metrics-grid" }, E = { class: "metric-item" }, D = { class: "metric-val" }, O = { class: "metric-item" }, k = { class: "metric-val" }, A = { class: "metric-item full-width" }, j = { class: "metric-mode" }, M = { class: "audio-control" }, N = /*#__PURE__*/ x(/* @__PURE__ */ s({
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
		function N() {
			n.value = !n.value, b(), n.value ? S() : t.hasSignal && x(t.frequency);
		}
		function P() {
			r && r.state === "suspended" && r.resume().catch(() => {});
		}
		u(() => {
			typeof window < "u" && (window.addEventListener("click", P, {
				capture: !0,
				passive: !0
			}), window.addEventListener("keydown", P, {
				capture: !0,
				passive: !0
			}), document.addEventListener("visibilitychange", F));
		});
		function F() {
			typeof document < "u" && (document.hidden ? S() : t.hasSignal && !n.value && x(t.frequency));
		}
		return m(() => [
			t.hasSignal,
			t.frequency,
			n.value
		], ([e, t, n]) => {
			e && !n ? x(t) : S();
		}, { immediate: !0 }), l(() => {
			typeof window < "u" && (window.removeEventListener("click", P, { capture: !0 }), window.removeEventListener("keydown", P, { capture: !0 }), document.removeEventListener("visibilitychange", F)), S(), r &&= (r.close().catch(() => {}), null);
		}), (t, r) => (d(), a("div", { class: c(["buzzer-world-widget", { "is-active": e.hasSignal }]) }, [
			o("div", C, [o("div", w, [r[0] ||= o("span", { class: "icon" }, "🔔", -1), o("span", null, p(e.label || "Buzzer"), 1)]), o("span", { class: c(["state-pill", e.hasSignal ? "state-on" : "state-off"]) }, p(e.hasSignal ? "SOUNDING" : "QUIET"), 3)]),
			o("div", T, [
				o("div", E, [r[1] ||= o("span", { class: "metric-label" }, "Frequency", -1), o("span", D, p(e.hasSignal && e.frequency ? `${Math.round(e.frequency)} Hz` : "0 Hz"), 1)]),
				o("div", O, [r[2] ||= o("span", { class: "metric-label" }, "Duty", -1), o("span", k, p(e.hasSignal ? `${Math.round(e.duty)}%` : "0%"), 1)]),
				o("div", A, [r[3] ||= o("span", { class: "metric-label" }, "Mode", -1), o("span", j, p(e.variant), 1)])
			]),
			o("div", M, [o("button", {
				class: c(["sound-toggle-btn", { "is-unmuted": !n.value }]),
				type: "button",
				onClick: N
			}, [o("span", null, p(n.value ? "🔇 Audio Muted" : "🔊 Audio Active"), 1)], 2)])
		], 2));
	}
}), [["__scopeId", "data-v-54c22c23"]]), P = Object.freeze({
	width: 75,
	height: 90
}), F = Object.freeze({
	1: Object.freeze({
		relX: 27,
		relY: 84,
		wireNet: "primary",
		defaultConnection: null,
		required: !0
	}),
	2: Object.freeze({
		relX: 37,
		relY: 84,
		wireNet: "gnd",
		defaultConnection: "GND",
		required: !1
	})
});
Object.freeze({});
var I = F, L = P, R = Object.freeze([{
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
}]), z = Object.freeze([{
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
function B(e) {
	try {
		let n = t("buzzer", e);
		if (n && n.length > 0) return n;
	} catch {}
	return e === "active_gpio" ? z : R;
}
var V = Object.freeze({
	passive_pwm: Object.freeze({
		variant: "passive_pwm",
		getPins: () => B("passive_pwm"),
		pinsOverlay: I,
		defaultAppearanceId: "buzzer_passive"
	}),
	active_gpio: Object.freeze({
		variant: "active_gpio",
		getPins: () => B("active_gpio"),
		pinsOverlay: I,
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
var H = r(import.meta.url, "buzzer", "1.0.0", "output");
function U(e, t) {
	let r = n(e, H.type);
	return t.pluginChannels?.[e.id] ?? t.pluginChannels?.[r] ?? t.pluginChannels?.[`${H.type}:0`] ?? t.pluginChannels?.[H.type] ?? {};
}
var W = V.passive_pwm, G = e({
	type: H.type,
	size: L,
	wireColor: "#f59e0b",
	pinsOverlay: W.pinsOverlay,
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
	canvas: S,
	world: N,
	ui: {
		canvasProps: (e, t) => {
			let n = U(e, t), r = t.isRunning !== !1;
			return {
				id: e.id,
				hasSignal: r && !!n.hasSignal,
				frequency: typeof n.frequency == "number" ? n.frequency : 0,
				duty: typeof n.duty == "number" ? n.duty : 0,
				label: e.props?.label ?? "Buzzer",
				pinConnections: e.pinConnections
			};
		},
		worldProps: (e, t) => {
			let n = U(e, t), r = t.isRunning !== !1;
			return {
				id: e.id,
				hasSignal: r && !!n.hasSignal,
				frequency: typeof n.frequency == "number" ? n.frequency : 0,
				duty: typeof n.duty == "number" ? n.duty : 0,
				label: e.props?.label ?? "Buzzer",
				variant: e.props?.variant ?? "passive_pwm"
			};
		}
	}
});
//#endregion
export { G as buzzerDefinition, G as default };
