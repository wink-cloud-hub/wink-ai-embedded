import { definePeripheral as e, pinsFromBinderVariant as t, resolvePluginInstanceId as n } from "@wink-ai/unisim-ui";
import { normalizeVariantKey as r, resolvePluginIdentity as i } from "@wink-ai/unisim";
import { Fragment as a, computed as o, createCommentVNode as s, createElementBlock as c, createElementVNode as l, createStaticVNode as u, defineComponent as d, normalizeStyle as f, openBlock as p, renderList as m, toDisplayString as h } from "vue";
//#region builtin/seg_display/1.0.0/src/CanvasGlyph.vue?vue&type=script&setup=true&lang.ts
var g = ["aria-label"], _ = ["transform"], v = [
	"points",
	"fill",
	"fill-opacity",
	"filter"
], y = [
	"fill",
	"fill-opacity",
	"filter"
], b = {
	key: 0,
	x: "105",
	y: "80",
	"text-anchor": "middle",
	fill: "#888890",
	"font-size": "7",
	"font-family": "monospace"
}, x = /*@__PURE__*/ d({
	__name: "CanvasGlyph",
	props: {
		pinConnections: {},
		variant: { default: "direct_gpio_8d" },
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
		nDigits: { default: 8 }
	},
	setup(e) {
		let t = e, n = {
			red: {
				lit: "#ff2828",
				dim: "#380c0c",
				glow: "#ff2222"
			},
			green: {
				lit: "#00ff55",
				dim: "#053310",
				glow: "#00ff55"
			},
			blue: {
				lit: "#2299ff",
				dim: "#082038",
				glow: "#0088ff"
			},
			yellow: {
				lit: "#ffee00",
				dim: "#383300",
				glow: "#ffee00"
			},
			white: {
				lit: "#f5f5f5",
				dim: "#282828",
				glow: "#ffffff"
			},
			orange: {
				lit: "#ff8800",
				dim: "#381c00",
				glow: "#ff7700"
			},
			purple: {
				lit: "#cc33ff",
				dim: "#2b0838",
				glow: "#bb11ff"
			}
		}, r = o(() => {
			let e = t.color?.toLowerCase() ?? "red";
			return n[e] ?? n.red;
		}), i = o(() => t.nDigits && t.nDigits > 0 ? t.nDigits : t.variant === "direct_gpio_4d" ? 4 : t.variant === "direct_gpio_2d" ? 2 : t.variant === "direct_gpio_1d" ? 1 : 8), d = o(() => {
			let e = i.value, t = [], n = 20, r = 4, a = 1.35;
			e === 8 ? (n = 19, r = 4.5, a = 1.35) : e === 4 ? (n = 34, r = 14, a = 1.45) : e === 2 ? (n = 56, r = 26, a = 1.5) : e === 1 && (n = 75, r = 0, a = 1.55);
			let o = 10 + (190 - (e * n + (e - 1) * r)) / 2, s = n / 26;
			for (let i = 0; i < e; i++) t.push({
				x: o + i * (n + r),
				y: 22,
				scaleX: s,
				scaleY: a
			});
			return t;
		}), x = [
			{
				id: "a",
				points: "3,2  17,2  15,5  5,5"
			},
			{
				id: "b",
				points: "17.5,3.5  19.5,5.5  19.5,17  17.5,19  15.5,17  15.5,5.5"
			},
			{
				id: "c",
				points: "17.5,21  19.5,23  19.5,34.5  17.5,36.5  15.5,34.5  15.5,23"
			},
			{
				id: "d",
				points: "5,35  15,35  17,38  3,38"
			},
			{
				id: "e",
				points: "2.5,21  4.5,23  4.5,34.5  2.5,36.5  0.5,34.5  0.5,23"
			},
			{
				id: "f",
				points: "2.5,3.5  4.5,5.5  4.5,17  2.5,19  0.5,17  0.5,5.5"
			},
			{
				id: "g",
				points: "3.5,19.5  5,17.5  15,17.5  16.5,19.5  15,21.5  5,21.5"
			}
		];
		function S(e, n) {
			let r = t.bright, i = r && r.length > e * 8 + n ? r[e * 8 + n] : 0, a = Math.max(0, Math.min(1, t.brightness)), o = i / 255;
			return Math.min(1, .1 + o * .9 * a);
		}
		function C(e, n) {
			let r = t.bright;
			return (r && r.length > e * 8 + n ? r[e * 8 + n] : 0) >= 40;
		}
		return (t, n) => (p(), c("svg", {
			class: "seg-display-canvas",
			viewBox: "0 0 210 96",
			width: "210",
			height: "96",
			role: "img",
			"aria-label": e.text ? `Display: ${e.text}` : e.label || "7-Segment Display",
			style: f(e.flip ? "transform: rotate(180deg);" : void 0)
		}, [
			n[0] ||= u("<defs data-v-ef5e031c><filter id=\"seg-glow-filter\" x=\"-20%\" y=\"-20%\" width=\"140%\" height=\"140%\" data-v-ef5e031c><feGaussianBlur stdDeviation=\"1.8\" result=\"blur\" data-v-ef5e031c></feGaussianBlur><feMerge data-v-ef5e031c><feMergeNode in=\"blur\" data-v-ef5e031c></feMergeNode><feMergeNode in=\"SourceGraphic\" data-v-ef5e031c></feMergeNode></feMerge></filter></defs><rect x=\"1\" y=\"1\" width=\"208\" height=\"94\" rx=\"6\" ry=\"6\" fill=\"#141416\" stroke=\"#2c2d33\" stroke-width=\"1.5\" data-v-ef5e031c></rect><rect x=\"8\" y=\"12\" width=\"194\" height=\"72\" rx=\"3\" ry=\"3\" fill=\"#090a0c\" stroke=\"#1c1d22\" stroke-width=\"1\" data-v-ef5e031c></rect>", 3),
			(p(!0), c(a, null, m(d.value, (t, n) => (p(), c("g", {
				key: n,
				class: "seg-digit",
				transform: `translate(${t.x}, ${t.y}) scale(${t.scaleX}, ${t.scaleY}) skewX(-7)`
			}, [(p(), c(a, null, m(x, (t, i) => l("polygon", {
				key: t.id,
				points: t.points,
				fill: C(n, i) ? r.value.lit : r.value.dim,
				"fill-opacity": S(n, i),
				filter: e.glow && C(n, i) ? "url(#seg-glow-filter)" : void 0
			}, null, 8, v)), 64)), l("circle", {
				cx: "23",
				cy: "37",
				r: "1.8",
				fill: C(n, 7) ? r.value.lit : r.value.dim,
				"fill-opacity": S(n, 7),
				filter: e.glow && C(n, 7) ? "url(#seg-glow-filter)" : void 0
			}, null, 8, y)], 8, _))), 128)),
			e.label ? (p(), c("text", b, h(e.label), 1)) : s("", !0)
		], 12, g));
	}
}), S = (e, t) => {
	let n = e.__vccOpts || e;
	for (let [e, r] of t) n[e] = r;
	return n;
}, C = /*#__PURE__*/ S(x, [["__scopeId", "data-v-ef5e031c"]]), w = { class: "seg-display-world-widget" }, T = { class: "display-bezel" }, E = { class: "display-screen" }, D = {
	key: 0,
	class: "display-label"
}, O = /*#__PURE__*/ S(/* @__PURE__ */ d({
	__name: "WorldWidget",
	props: {
		pinConnections: {},
		color: {},
		brightness: {},
		label: {},
		text: {}
	},
	setup(e) {
		return (t, n) => (p(), c("div", w, [l("div", T, [l("div", E, [l("span", {
			class: "display-text",
			style: f({ color: e.color })
		}, h(e.text || "--------"), 5)]), e.label ? (p(), c("div", D, h(e.label), 1)) : s("", !0)])]));
	}
}), [["__scopeId", "data-v-ffe7c08c"]]);
Object.freeze([
	"direct_gpio_8d",
	"direct_gpio_4d",
	"direct_gpio_2d",
	"direct_gpio_1d"
]);
var k = Object.freeze({
	direct_gpio_8d: 8,
	direct_gpio_4d: 4,
	direct_gpio_2d: 2,
	direct_gpio_1d: 1
});
function A(e) {
	let t = r(e);
	return t && t in k ? t : "direct_gpio_8d";
}
Object.freeze({
	"8d": Object.freeze(["direct_gpio_8d"]),
	"4d": Object.freeze(["direct_gpio_4d"]),
	"2d": Object.freeze(["direct_gpio_2d"]),
	"1d": Object.freeze(["direct_gpio_1d"])
});
var j = [
	"A",
	"B",
	"C",
	"D",
	"E",
	"F",
	"G",
	"DP"
], M = [
	23,
	47,
	70,
	93,
	117,
	140,
	163,
	187
];
function N() {
	let e = {};
	for (let t = 0; t < j.length; t++) e[j[t]] = Object.freeze({
		relX: M[t],
		relY: 96,
		wireNet: "primary",
		required: !1
	});
	return e;
}
function P(e) {
	let t = N(), n = {}, r = k[e];
	if (r === 8) for (let e = 0; e < 8; e++) n[`DIG${e + 1}`] = Object.freeze({
		relX: M[e],
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
		getPins: () => t("seg_display", "direct_gpio_8d"),
		pinsOverlay: P("direct_gpio_8d"),
		defaultAppearanceId: "seg_display_8"
	}),
	direct_gpio_4d: Object.freeze({
		variant: "direct_gpio_4d",
		getPins: () => t("seg_display", "direct_gpio_4d"),
		pinsOverlay: P("direct_gpio_4d"),
		defaultAppearanceId: "seg_display_4"
	}),
	direct_gpio_2d: Object.freeze({
		variant: "direct_gpio_2d",
		getPins: () => t("seg_display", "direct_gpio_2d"),
		pinsOverlay: P("direct_gpio_2d"),
		defaultAppearanceId: "seg_display_2"
	}),
	direct_gpio_1d: Object.freeze({
		variant: "direct_gpio_1d",
		getPins: () => t("seg_display", "direct_gpio_1d"),
		pinsOverlay: P("direct_gpio_1d"),
		defaultAppearanceId: "seg_display_1"
	})
});
Object.freeze({
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
});
//#endregion
//#region builtin/seg_display/1.0.0/src/definition.ts
var I = i(import.meta.url, "seg_display", "1.0.0", "display");
function L(e, t, r) {
	let i = n(e, I.type);
	return t.pluginChannels?.[i]?.[r];
}
function R(e, t) {
	let n = L(e, t, "bright");
	return n instanceof Uint8Array ? n : null;
}
function z(e, t) {
	let n = L(e, t, "segMask");
	if (typeof n == "string") try {
		let e = JSON.parse(n);
		if (Array.isArray(e)) return e;
	} catch {}
	return [];
}
function B(e, t) {
	let n = L(e, t, "text");
	return typeof n == "string" ? n : "";
}
function V(e) {
	return k[A(e.props?.variant)] ?? 8;
}
var H = F.direct_gpio_8d, U = {
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
}, W = e({
	type: I.type,
	size: {
		width: 210,
		height: 96
	},
	wireColor: "#ff0055",
	pinsOverlay: H.pinsOverlay,
	props: U,
	canvas: C,
	world: O,
	ui: {
		canvasProps: (e, t) => ({
			pinConnections: e.pinConnections,
			variant: e.props.variant,
			color: e.props.color,
			brightness: e.props.brightness,
			glow: e.props.glow,
			label: e.props.label,
			flip: e.props.flip,
			bright: R(e, t),
			segMask: z(e, t),
			text: B(e, t),
			nDigits: V(e)
		}),
		worldProps: (e, t) => ({
			pinConnections: e.pinConnections,
			color: e.props.color,
			brightness: e.props.brightness,
			label: e.props.label,
			text: B(e, t)
		})
	}
});
//#endregion
export { W as default, W as segDisplayDefinition, U as segDisplayProps };
