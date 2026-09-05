import { definePeripheral as e, pinsFromBinderVariant as t, resolvePluginInstanceId as n } from "@wink-ai/unisim-ui";
import { normalizeVariantKey as r, resolvePluginIdentity as i } from "@wink-ai/unisim";
import { computed as a, createCommentVNode as o, createElementBlock as s, createElementVNode as c, defineComponent as l, normalizeStyle as u, openBlock as d, ref as f, toDisplayString as p, watchEffect as m } from "vue";
import "@wokwi/elements";
//#region builtin/seg_display/1.0.0/src/CanvasGlyph.vue?vue&type=script&setup=true&lang.ts
var h = [
	"digits",
	"color",
	"values",
	".values"
], g = {
	key: 0,
	class: "display-label"
}, _ = /*@__PURE__*/ l({
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
			".values": i.value,
			pins: "top"
		}, null, 40, h), e.label ? (d(), s("span", g, p(e.label), 1)) : o("", !0)], 4));
	}
}), v = (e, t) => {
	let n = e.__vccOpts || e;
	for (let [e, r] of t) n[e] = r;
	return n;
}, y = /*#__PURE__*/ v(_, [["__scopeId", "data-v-57525e19"]]), b = { class: "seg-display-world-widget" }, x = [
	"digits",
	"color",
	"values",
	".values"
], S = {
	key: 0,
	class: "display-label"
}, C = /*#__PURE__*/ v(/* @__PURE__ */ l({
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
		}), (t, a) => (d(), s("div", b, [c("wokwi-7segment", {
			ref_key: "segEl",
			ref: n,
			digits: r.value,
			color: e.color || "red",
			values: i.value,
			".values": i.value,
			pins: "none"
		}, null, 40, x), e.label ? (d(), s("div", S, p(e.label), 1)) : o("", !0)]));
	}
}), [["__scopeId", "data-v-3e35649d"]]), w = Object.freeze([
	"direct_gpio_8d",
	"direct_gpio_4d",
	"direct_gpio_2d",
	"direct_gpio_1d"
]), T = Object.freeze({
	direct_gpio_8d: 8,
	direct_gpio_4d: 4,
	direct_gpio_2d: 2,
	direct_gpio_1d: 1
});
function E(e) {
	let t = r(e);
	return t && t in T ? t : "direct_gpio_8d";
}
var D = Object.freeze({
	"8d": Object.freeze(["direct_gpio_8d"]),
	"4d": Object.freeze(["direct_gpio_4d"]),
	"2d": Object.freeze(["direct_gpio_2d"]),
	"1d": Object.freeze(["direct_gpio_1d"])
}), O = [
	"A",
	"B",
	"C",
	"D",
	"E",
	"F",
	"G",
	"DP"
], k = [
	23,
	47,
	70,
	93,
	117,
	140,
	163,
	187
];
function A() {
	let e = {};
	for (let t = 0; t < O.length; t++) e[O[t]] = Object.freeze({
		relX: k[t],
		relY: 96,
		wireNet: "primary",
		required: !1
	});
	return e;
}
function j(e) {
	return Object.freeze(Object.entries(e).map(([e]) => ({
		name: e,
		direction: "sink",
		signal: "digital",
		catalogType: "gpio",
		required: !1
	})));
}
function M(e) {
	let t = A(), n = {}, r = T[e];
	if (r === 8) for (let e = 0; e < 8; e++) n[`DIG${e + 1}`] = Object.freeze({
		relX: k[e],
		relY: 0,
		wireNet: "secondary",
		required: !1
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
			required: !1
		});
	} else if (r === 2) {
		let e = [70, 140];
		for (let t = 0; t < 2; t++) n[`DIG${t + 1}`] = Object.freeze({
			relX: e[t],
			relY: 0,
			wireNet: "secondary",
			required: !1
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
var N = Object.freeze({
	direct_gpio_8d: Object.freeze({
		variant: "direct_gpio_8d",
		getPins: () => {
			let e = t("seg_display", "direct_gpio_8d");
			return e.length > 0 ? e : j(M("direct_gpio_8d"));
		},
		pinsOverlay: M("direct_gpio_8d"),
		defaultAppearanceId: "seg_display_8"
	}),
	direct_gpio_4d: Object.freeze({
		variant: "direct_gpio_4d",
		getPins: () => {
			let e = t("seg_display", "direct_gpio_4d");
			return e.length > 0 ? e : j(M("direct_gpio_4d"));
		},
		pinsOverlay: M("direct_gpio_4d"),
		defaultAppearanceId: "seg_display_4"
	}),
	direct_gpio_2d: Object.freeze({
		variant: "direct_gpio_2d",
		getPins: () => {
			let e = t("seg_display", "direct_gpio_2d");
			return e.length > 0 ? e : j(M("direct_gpio_2d"));
		},
		pinsOverlay: M("direct_gpio_2d"),
		defaultAppearanceId: "seg_display_2"
	}),
	direct_gpio_1d: Object.freeze({
		variant: "direct_gpio_1d",
		getPins: () => {
			let e = t("seg_display", "direct_gpio_1d");
			return e.length > 0 ? e : j(M("direct_gpio_1d"));
		},
		pinsOverlay: M("direct_gpio_1d"),
		defaultAppearanceId: "seg_display_1"
	})
}), P = Object.freeze({
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
}), F = N, I = P, L = D, R = "direct_gpio_8d", z = i(import.meta.url, "seg_display", "1.0.0", "display");
function B(e, t, r) {
	let i = n(e, z.type);
	return t.pluginChannels?.[e.id]?.[r] ?? t.pluginChannels?.[i]?.[r] ?? t.pluginChannels?.[`${z.type}:0`]?.[r] ?? t.pluginChannels?.[z.type]?.[r];
}
function V(e, t) {
	let n = B(e, t, "bright");
	return n instanceof Uint8Array ? n : Array.isArray(n) ? new Uint8Array(n) : n && typeof n == "object" ? new Uint8Array(Object.values(n)) : null;
}
function H(e, t) {
	let n = B(e, t, "segMask");
	if (Array.isArray(n)) return n;
	if (typeof n == "string") try {
		let e = JSON.parse(n);
		if (Array.isArray(e)) return e;
	} catch {}
	return [];
}
function U(e, t) {
	let n = B(e, t, "text");
	return typeof n == "string" ? n : "";
}
function W(e) {
	return T[E(e.props?.variant)] ?? 8;
}
function G(e, t) {
	let n = W(e), r = n * 8, i = V(e, t);
	if (i && i.length > 0) {
		let e = [];
		for (let t = 0; t < r; t++) {
			let n = t < i.length ? i[t] : 0;
			e.push(+(n >= 50));
		}
		return e;
	}
	let a = H(e, t);
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
var K = N.direct_gpio_8d, q = {
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
	}
}, J = e({
	type: z.type,
	size: {
		width: 210,
		height: 96
	},
	wireColor: "#ff0055",
	pinsOverlay: K.pinsOverlay,
	props: q,
	canvas: y,
	world: C,
	ui: {
		canvasProps: (e, t) => ({
			pinConnections: e.pinConnections,
			variant: e.props.variant,
			color: e.props.color,
			brightness: e.props.brightness,
			glow: e.props.glow,
			label: e.props.label,
			flip: e.props.flip,
			bright: V(e, t),
			segMask: H(e, t),
			text: U(e, t),
			nDigits: W(e),
			values: G(e, t)
		}),
		worldProps: (e, t) => ({
			pinConnections: e.pinConnections,
			variant: e.props.variant,
			color: e.props.color,
			brightness: e.props.brightness,
			label: e.props.label,
			text: U(e, t),
			bright: V(e, t),
			segMask: H(e, t),
			nDigits: W(e),
			values: G(e, t)
		})
	}
});
//#endregion
export { P as SEG_APPEARANCES, N as SEG_TOPOLOGIES, D as SEG_TOPOLOGY_EQUIVALENCE, w as SEG_VARIANTS, T as SEG_VARIANT_DIGITS, I as appearances, J as default, J as segDisplayDefinition, R as defaultVariant, L as equivalence, E as resolveSegVariant, q as segDisplayProps, F as topologies };
