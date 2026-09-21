import { definePeripheral as e, pinsFromBinderVariant as t, resolvePluginInstanceId as n } from "@wink-ai/unisim-ui";
import { normalizeVariantKey as r, resolvePluginIdentity as i } from "@wink-ai/unisim-sdk";
import { computed as a, createCommentVNode as o, createElementBlock as s, createElementVNode as c, defineComponent as l, normalizeStyle as u, openBlock as d, ref as f, toDisplayString as p, watchEffect as m } from "vue";
import "@wokwi/elements";
//#region builtin/seg_display/1.0.0/src/CanvasGlyph.vue?vue&type=script&setup=true&lang.ts
var h = [
	"digits",
	"color",
	"values"
], g = /*@__PURE__*/ l({
	__name: "CanvasGlyph",
	props: {
		pinConnections: {},
		variant: { default: "direct_gpio_4d" },
		color: { default: "red" },
		brightness: { default: 1 },
		glow: {
			type: Boolean,
			default: !0
		},
		label: { default: "" },
		flip: {
			type: Boolean,
			default: !1
		},
		bright: { default: null },
		segMask: { default: () => [] },
		text: { default: "" },
		nDigits: { default: 4 },
		values: { default: () => [] }
	},
	setup(e) {
		let t = e, n = f(null), r = a(() => t.nDigits && t.nDigits > 0 ? t.nDigits : t.variant === "direct_gpio_8d" ? 8 : t.variant === "direct_gpio_4d" ? 4 : t.variant === "direct_gpio_2d" ? 2 : t.variant === "direct_gpio_1d" ? 1 : 4), i = a(() => {
			let e = r.value, n = e * 8;
			if (t.values && t.values.length >= n) return t.values.slice(0, n);
			let i = t.bright;
			if (i && i.length > 0) {
				let e = [], t = i.length;
				for (let r = 0; r < n; r++) {
					let n = r < t ? i[r] : 0;
					e.push(+(n >= 50));
				}
				return e;
			}
			let a = [];
			if (Array.isArray(t.segMask)) a = t.segMask;
			else if (typeof t.segMask == "string") try {
				a = JSON.parse(t.segMask);
			} catch {}
			if (a.length > 0) {
				let t = [];
				for (let n = 0; n < e; n++) {
					let e = a[n] ?? 0;
					for (let n = 0; n < 8; n++) t.push(e >> n & 1);
				}
				return t;
			}
			return Array(n).fill(0);
		});
		return m(() => {
			n.value && (n.value.values = i.value);
		}), (t, a) => (d(), s("div", {
			class: "seg-display-canvas",
			style: u(e.flip ? "transform: rotate(180deg);" : void 0)
		}, [c("wokwi-7segment", {
			ref_key: "segEl",
			ref: n,
			digits: r.value,
			color: e.color || "red",
			values: i.value,
			pins: "top"
		}, null, 8, h)], 4));
	}
}), _ = (e, t) => {
	let n = e.__vccOpts || e;
	for (let [e, r] of t) n[e] = r;
	return n;
}, v = /*#__PURE__*/ _(g, [["__scopeId", "data-v-c86f8a56"]]), y = { class: "seg-display-world-widget" }, b = [
	"digits",
	"color",
	"values"
], x = {
	key: 0,
	class: "display-label"
}, S = /*#__PURE__*/ _(/* @__PURE__ */ l({
	__name: "WorldWidget",
	props: {
		pinConnections: {},
		color: { default: "red" },
		brightness: { default: 1 },
		label: { default: "" },
		text: { default: "" },
		variant: { default: "direct_gpio_4d" },
		nDigits: { default: 4 },
		bright: { default: null },
		segMask: { default: () => [] },
		values: { default: () => [] }
	},
	setup(e) {
		let t = e, n = f(null), r = a(() => t.nDigits && t.nDigits > 0 ? t.nDigits : t.variant === "direct_gpio_8d" ? 8 : t.variant === "direct_gpio_4d" ? 4 : t.variant === "direct_gpio_2d" ? 2 : t.variant === "direct_gpio_1d" ? 1 : 4), i = a(() => {
			let e = r.value, n = e * 8;
			if (t.values && t.values.length >= n) return t.values.slice(0, n);
			let i = t.bright;
			if (i && i.length > 0) {
				let e = [], t = i.length;
				for (let r = 0; r < n; r++) {
					let n = r < t ? i[r] : 0;
					e.push(+(n >= 30));
				}
				return e;
			}
			let a = [];
			if (Array.isArray(t.segMask)) a = t.segMask;
			else if (typeof t.segMask == "string") try {
				a = JSON.parse(t.segMask);
			} catch {}
			if (a.length > 0) {
				let t = [];
				for (let n = 0; n < e; n++) {
					let e = a[n] ?? 0;
					for (let n = 0; n < 8; n++) t.push(e >> n & 1);
				}
				return t;
			}
			return Array(n).fill(0);
		});
		return m(() => {
			n.value && (n.value.values = i.value);
		}), (t, a) => (d(), s("div", y, [c("wokwi-7segment", {
			ref_key: "segEl",
			ref: n,
			digits: r.value,
			color: e.color || "red",
			values: i.value,
			pins: "none"
		}, null, 8, b), e.label ? (d(), s("div", x, p(e.label), 1)) : o("", !0)]));
	}
}), [["__scopeId", "data-v-de5254b3"]]), C = Object.freeze({
	width: 210,
	height: 96
}), w = Object.freeze({
	A: Object.freeze({
		relX: 23,
		relY: 96,
		wireNet: "primary",
		defaultConnection: null,
		required: !1
	}),
	B: Object.freeze({
		relX: 47,
		relY: 96,
		wireNet: "primary",
		defaultConnection: null,
		required: !1
	}),
	C: Object.freeze({
		relX: 70,
		relY: 96,
		wireNet: "primary",
		defaultConnection: null,
		required: !1
	}),
	D: Object.freeze({
		relX: 93,
		relY: 96,
		wireNet: "primary",
		defaultConnection: null,
		required: !1
	}),
	E: Object.freeze({
		relX: 117,
		relY: 96,
		wireNet: "primary",
		defaultConnection: null,
		required: !1
	}),
	F: Object.freeze({
		relX: 140,
		relY: 96,
		wireNet: "primary",
		defaultConnection: null,
		required: !1
	}),
	G: Object.freeze({
		relX: 163,
		relY: 96,
		wireNet: "primary",
		defaultConnection: null,
		required: !1
	}),
	DP: Object.freeze({
		relX: 187,
		relY: 96,
		wireNet: "primary",
		defaultConnection: null,
		required: !1
	})
}), T = Object.freeze([
	"direct_gpio_8d",
	"direct_gpio_4d",
	"direct_gpio_2d",
	"direct_gpio_1d"
]), E = Object.freeze({
	direct_gpio_8d: 8,
	direct_gpio_4d: 4,
	direct_gpio_2d: 2,
	direct_gpio_1d: 1
});
function D(e) {
	let t = r(e);
	return t && t in E ? t : "direct_gpio_8d";
}
var O = Object.freeze({
	"8d": Object.freeze(["direct_gpio_8d"]),
	"4d": Object.freeze(["direct_gpio_4d"]),
	"2d": Object.freeze(["direct_gpio_2d"]),
	"1d": Object.freeze(["direct_gpio_1d"])
}), k = C, A = w, j = Object.freeze([
	"A",
	"B",
	"C",
	"D",
	"E",
	"F",
	"G",
	"DP"
].map((e) => w[e]?.relX ?? 0));
function M() {
	return w;
}
function N(e) {
	return Object.freeze(Object.entries(e).map(([e]) => ({
		name: e,
		direction: "sink",
		signal: "digital",
		catalogType: "gpio",
		required: !1
	})));
}
function P(e) {
	let t = M(), n = {}, r = E[e];
	if (r === 8) for (let e = 0; e < 8; e++) n[`DIG${e + 1}`] = Object.freeze({
		relX: j[e],
		relY: 0,
		wireNet: "secondary",
		required: !0
	});
	else if (r === 4) {
		let e = [
			42,
			84,
			126,
			168
		];
		for (let t = 0; t < 4; t++) n[`DIG${t + 1}`] = Object.freeze({
			relX: e[t],
			relY: 0,
			wireNet: "secondary",
			required: !0
		});
	} else if (r === 2) {
		let e = [70, 140];
		for (let t = 0; t < 2; t++) n[`DIG${t + 1}`] = Object.freeze({
			relX: e[t],
			relY: 0,
			wireNet: "secondary",
			required: !0
		});
	} else n.DIG1 = Object.freeze({
		relX: 105,
		relY: 0,
		wireNet: "secondary",
		required: !1
	});
	return Object.freeze({
		...t,
		...n
	});
}
var F = Object.freeze({
	direct_gpio_8d: Object.freeze({
		variant: "direct_gpio_8d",
		getPins: () => {
			let e = t("seg_display", "direct_gpio_8d");
			return e.length > 0 ? e : N(P("direct_gpio_8d"));
		},
		pinsOverlay: P("direct_gpio_8d"),
		defaultAppearanceId: "seg_display_8"
	}),
	direct_gpio_4d: Object.freeze({
		variant: "direct_gpio_4d",
		getPins: () => {
			let e = t("seg_display", "direct_gpio_4d");
			return e.length > 0 ? e : N(P("direct_gpio_4d"));
		},
		pinsOverlay: P("direct_gpio_4d"),
		defaultAppearanceId: "seg_display_4"
	}),
	direct_gpio_2d: Object.freeze({
		variant: "direct_gpio_2d",
		getPins: () => {
			let e = t("seg_display", "direct_gpio_2d");
			return e.length > 0 ? e : N(P("direct_gpio_2d"));
		},
		pinsOverlay: P("direct_gpio_2d"),
		defaultAppearanceId: "seg_display_2"
	}),
	direct_gpio_1d: Object.freeze({
		variant: "direct_gpio_1d",
		getPins: () => {
			let e = t("seg_display", "direct_gpio_1d");
			return e.length > 0 ? e : N(P("direct_gpio_1d"));
		},
		pinsOverlay: P("direct_gpio_1d"),
		defaultAppearanceId: "seg_display_1"
	})
}), I = Object.freeze({
	seg_display_8: Object.freeze({
		appearanceId: "seg_display_8",
		variant: "direct_gpio_8d",
		displayName: "8-Digit 7-Segment Display",
		searchAliases: Object.freeze([
			"seg",
			"7seg",
			"数码管",
			"8d",
			"digital tube"
		])
	}),
	seg_display_4: Object.freeze({
		appearanceId: "seg_display_4",
		variant: "direct_gpio_4d",
		displayName: "4-Digit 7-Segment Display",
		searchAliases: Object.freeze([
			"seg",
			"7seg",
			"数码管",
			"4d",
			"digital tube"
		])
	}),
	seg_display_2: Object.freeze({
		appearanceId: "seg_display_2",
		variant: "direct_gpio_2d",
		displayName: "2-Digit 7-Segment Display",
		searchAliases: Object.freeze([
			"seg",
			"7seg",
			"数码管",
			"2d",
			"digital tube"
		])
	}),
	seg_display_1: Object.freeze({
		appearanceId: "seg_display_1",
		variant: "direct_gpio_1d",
		displayName: "1-Digit 7-Segment Display",
		searchAliases: Object.freeze([
			"seg",
			"7seg",
			"数码管",
			"1d",
			"digital tube"
		])
	})
}), L = F, R = I, z = O, B = "direct_gpio_8d", V = i(import.meta.url, "seg_display", "1.0.0", "display");
function H(e, t, r) {
	let i = n(e, V.type);
	return t.pluginChannels?.[e.id]?.[r] ?? t.pluginChannels?.[i]?.[r] ?? t.pluginChannels?.[`${V.type}:0`]?.[r] ?? t.pluginChannels?.[V.type]?.[r];
}
function U(e, t) {
	let n = H(e, t, "bright");
	return n instanceof Uint8Array ? n : Array.isArray(n) ? new Uint8Array(n) : n && typeof n == "object" ? new Uint8Array(Object.values(n)) : null;
}
function W(e, t) {
	let n = H(e, t, "segMask");
	if (Array.isArray(n)) return n;
	if (typeof n == "string") try {
		let e = JSON.parse(n);
		if (Array.isArray(e)) return e;
	} catch {}
	return [];
}
function G(e, t) {
	let n = H(e, t, "text");
	return typeof n == "string" ? n : "";
}
function K(e) {
	return E[D(e.props?.variant)] ?? 8;
}
function q(e, t) {
	let n = K(e), r = n * 8, i = U(e, t);
	if (i && i.length > 0) {
		let e = [];
		for (let t = 0; t < r; t++) {
			let n = t < i.length ? i[t] : 0;
			e.push(+(n >= 50));
		}
		return e;
	}
	let a = W(e, t);
	if (a && a.length > 0) {
		let e = [];
		for (let t = 0; t < n; t++) {
			let n = a[t] ?? 0;
			for (let t = 0; t < 8; t++) e.push(n >> t & 1);
		}
		return e;
	}
	return Array(r).fill(0);
}
var J = F.direct_gpio_8d, Y = {
	variant: {
		type: "string",
		default: "direct_gpio_8d",
		description: "Segment display topology variant",
		options: [
			"direct_gpio_8d",
			"direct_gpio_4d",
			"direct_gpio_2d",
			"direct_gpio_1d"
		]
	},
	appearanceId: {
		type: "string",
		default: "seg_display_8",
		description: "Display appearance id"
	},
	segActiveLevel: {
		type: "string",
		default: "high",
		description: "Active level for segment pins (high/low)",
		options: ["high", "low"]
	},
	digitActiveLevel: {
		type: "string",
		default: "low",
		description: "Active level for digit select pins (high/low)",
		options: ["high", "low"]
	},
	commonAnode: {
		type: "boolean",
		default: !1,
		description: "Common anode preset (sets seg=low, dig=high if levels not explicitly overridden)"
	},
	color: {
		type: "string",
		default: "red",
		description: "LED segment color",
		options: [
			"red",
			"green",
			"blue",
			"yellow",
			"white",
			"orange",
			"purple"
		]
	},
	glow: {
		type: "boolean",
		default: !0,
		description: "Enable phosphor glow effect"
	},
	brightness: {
		type: "number",
		default: 1,
		description: "Overall brightness multiplier (0.0 - 1.0)"
	},
	label: {
		type: "string",
		default: "",
		description: "Label text"
	},
	flip: {
		type: "boolean",
		default: !1,
		description: "Flip orientation"
	},
	deadbandCheck: {
		type: "boolean",
		default: !0,
		description: "Enable commutation deadband and ghosting linting"
	},
	decayTauUs: {
		type: "number",
		default: 8e4,
		description: "Persistence of vision (POV) exponential decay time constant in microseconds"
	}
}, X = e({
	type: V.type,
	size: k,
	wireColor: "#ff0055",
	rotationPolicy: "fixed",
	pinsOverlay: J.pinsOverlay,
	props: Y,
	canvas: v,
	world: S,
	ui: {
		canvasProps: (e, t) => ({
			pinConnections: e.pinConnections,
			variant: e.props.variant,
			color: e.props.color,
			brightness: e.props.brightness,
			glow: e.props.glow,
			label: e.props.label,
			flip: e.props.flip,
			bright: U(e, t),
			segMask: W(e, t),
			text: G(e, t),
			nDigits: K(e),
			values: q(e, t)
		}),
		worldProps: (e, t) => ({
			pinConnections: e.pinConnections,
			variant: e.props.variant,
			color: e.props.color,
			brightness: e.props.brightness,
			label: e.props.label,
			text: G(e, t),
			bright: U(e, t),
			segMask: W(e, t),
			nDigits: K(e),
			values: q(e, t)
		})
	}
});
//#endregion
export { I as SEG_APPEARANCES, A as SEG_DISPLAY_OVERLAY, k as SEG_DISPLAY_SIZE, F as SEG_TOPOLOGIES, O as SEG_TOPOLOGY_EQUIVALENCE, T as SEG_VARIANTS, E as SEG_VARIANT_DIGITS, R as appearances, X as default, X as segDisplayDefinition, B as defaultVariant, z as equivalence, D as resolveSegVariant, Y as segDisplayProps, L as topologies };
