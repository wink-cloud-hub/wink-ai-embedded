import { definePeripheral as e, pinsFromBinderVariant as t } from "@wink-ai/unisim-ui";
import { resolvePluginIdentity as n } from "@wink-ai/unisim";
import { createElementBlock as r, defineComponent as i, openBlock as a, ref as o, watch as s } from "vue";
import "@wokwi/elements";
//#region builtin/mono_oled/1.0.0/src/paintFramebuffer.ts
var c = 128, l = 64, u = 1024, d = 8;
function f(e, t) {
	let n = new ImageData(c, l), r = n.data;
	if (t && t.length === u) for (let e = 0; e < d; e++) for (let n = 0; n < c; n++) {
		let i = t[e * c + n];
		for (let t = 0; t < 8; t++) {
			let a = e * 8 + t, o = i >> t & 1, s = (a * c + n) * 4;
			r[s] = o ? 0 : 8, r[s + 1] = o ? 210 : 12, r[s + 2] = o ? 255 : 24, r[s + 3] = 255;
		}
	}
	else {
		r.fill(0);
		for (let e = 3; e < r.length; e += 4) r[e] = 255;
	}
	e.imageData = n, typeof e.redraw == "function" && e.redraw();
	let i = e.shadowRoot?.querySelector("canvas");
	i && i.getContext("2d")?.putImageData(n, 0, 0);
}
//#endregion
//#region builtin/mono_oled/1.0.0/src/CanvasGlyph.vue
var p = /* @__PURE__ */ i({
	__name: "CanvasGlyph",
	props: {
		displayFrame: {},
		framebuffer: {}
	},
	setup(e) {
		let t = e, n = o(null);
		return s(() => [
			t.displayFrame?.seq,
			t.displayFrame ? null : t.framebuffer,
			n.value
		], ([, , e]) => {
			e && f(e, t.displayFrame?.fb ?? t.framebuffer ?? null ?? null);
		}, { immediate: !0 }), (e, t) => (a(), r("wokwi-ssd1306", {
			ref_key: "oledEl",
			ref: n
		}, null, 512));
	}
}), m = /* @__PURE__ */ i({
	__name: "WorldWidget",
	props: {
		pinConnections: {},
		displayFrame: {},
		framebuffer: {}
	},
	setup(e) {
		let t = e, n = o(null);
		return s(() => [
			t.displayFrame?.seq,
			t.displayFrame ? null : t.framebuffer,
			n.value
		], ([, , e]) => {
			e && f(e, t.displayFrame?.fb ?? t.framebuffer ?? null ?? null);
		}, { immediate: !0 }), (e, t) => (a(), r("wokwi-ssd1306", {
			ref_key: "oledEl",
			ref: n
		}, null, 512));
	}
}), h = Object.freeze({
	width: 150,
	height: 116
}), g = Object.freeze({
	DATA: Object.freeze({
		relX: 37,
		relY: 13,
		wireNet: "primary",
		defaultConnection: null,
		required: !0
	}),
	CLK: Object.freeze({
		relX: 46,
		relY: 13,
		wireNet: "secondary",
		defaultConnection: null,
		required: !1
	}),
	DC: Object.freeze({
		relX: 55,
		relY: 13,
		wireNet: "secondary",
		defaultConnection: null,
		required: !1
	}),
	RST: Object.freeze({
		relX: 65,
		relY: 13,
		wireNet: "secondary",
		defaultConnection: null,
		required: !1
	}),
	CS: Object.freeze({
		relX: 75,
		relY: 13,
		wireNet: "secondary",
		defaultConnection: null,
		required: !1
	}),
	"3V3": Object.freeze({
		relX: 84,
		relY: 13,
		wireNet: "vcc",
		defaultConnection: "3V3",
		required: !1
	}),
	VIN: Object.freeze({
		relX: 94,
		relY: 13,
		wireNet: "vcc",
		defaultConnection: "VCC",
		required: !1
	}),
	GND: Object.freeze({
		relX: 104,
		relY: 12,
		wireNet: "gnd",
		defaultConnection: "GND",
		required: !1
	})
});
Object.freeze({
	ssd1306_i2c: [],
	ssd1306_spi: []
});
var _ = h, v = Object.freeze({
	DATA: Object.freeze({
		...g.DATA,
		defaultConnection: 21
	}),
	CLK: Object.freeze({
		...g.CLK,
		defaultConnection: 22,
		required: !0
	}),
	"3V3": g["3V3"],
	GND: g.GND
}), y = Object.freeze({
	CLK: Object.freeze({
		...g.CLK,
		defaultConnection: 18
	}),
	DIN: Object.freeze({
		...g.DATA,
		defaultConnection: 23
	}),
	CS: Object.freeze({
		...g.CS,
		defaultConnection: 5
	}),
	DC: Object.freeze({
		...g.DC,
		defaultConnection: 17
	}),
	RES: Object.freeze({
		...g.RST,
		defaultConnection: 16
	}),
	"3V3": g["3V3"],
	GND: g.GND
}), b = Object.freeze({
	ssd1306_i2c: Object.freeze({
		variant: "ssd1306_i2c",
		getPins: () => t("mono_oled", "ssd1306_i2c"),
		pinsOverlay: v,
		defaultAppearanceId: "mono_oled_ssd1306_i2c"
	}),
	ssd1306_spi: Object.freeze({
		variant: "ssd1306_spi",
		getPins: () => t("mono_oled", "ssd1306_spi"),
		pinsOverlay: y,
		defaultAppearanceId: "mono_oled_ssd1306_spi"
	})
});
Object.freeze({
	mono_oled_ssd1306_i2c: Object.freeze({
		appearanceId: "mono_oled_ssd1306_i2c",
		variant: "ssd1306_i2c",
		displayName: "SSD1306 0.96\" I2C OLED",
		searchAliases: Object.freeze([
			"0.96",
			"i2c",
			"ssd1306"
		])
	}),
	mono_oled_ssd1306_spi: Object.freeze({
		appearanceId: "mono_oled_ssd1306_spi",
		variant: "ssd1306_spi",
		displayName: "SSD1306 0.96\" SPI OLED",
		searchAliases: Object.freeze([
			"0.96",
			"spi",
			"ssd1306"
		])
	})
});
//#endregion
//#region builtin/mono_oled/1.0.0/src/definition.ts
var x = n(import.meta.url, "mono_oled", "1.0.0", "display"), S = /* @__PURE__ */ new Set(["ssd1306_fb", "framebuffer"]), C = [
	"displayFrameInstanceId",
	"pluginInstanceId",
	"runtimeInstanceId",
	"simulationInstanceId",
	"pluginId"
], w = [
	"displayIndex",
	"pluginIndex",
	"instanceIndex"
];
function T(e) {
	return !e.kind || S.has(e.kind);
}
function E(e) {
	return e || "mono_oled";
}
function D(e, t) {
	for (let n of t) {
		let t = e[n];
		if (typeof t == "string" && t.trim()) return t;
	}
	return null;
}
function O(e, t) {
	for (let n of t) {
		let t = e[n];
		if (typeof t == "number" && Number.isInteger(t) && t >= 0) return t;
		if (typeof t == "string" && /^\d+$/.test(t)) return Number(t);
	}
	return null;
}
function k(e) {
	if (!e) return [];
	if (typeof e == "string") return [e];
	let t = [], n = e.props || {}, r = D(n, C);
	r && t.push(r);
	let i = O(n, w);
	return i !== null && t.push(`${E(e.type)}:${i}`), t.push(e.id), Array.from(new Set(t));
}
function A(e, t) {
	let n = e.displayFrames ?? [], r = k(t);
	for (let t of r) {
		let r = e.getDisplayFrame?.(t) ?? n.find((e) => e.instanceId === t) ?? null;
		if (r) return r;
	}
	let i = n.filter(T);
	return !t || r.length === 0 ? i[0] ?? n[0] ?? null : i.length === 1 ? i[0] : null;
}
function j(e, t) {
	let n = A(e, t);
	return n?.fb ?? n?.framebuffer ?? e.displayFb ?? null;
}
var M = "ssd1306_i2c", N = b[M], P = {
	variant: {
		type: "string",
		default: M,
		description: "OLED topology variant (ssd1306_i2c | ssd1306_spi)"
	},
	panel_ic: {
		type: "string",
		default: "ssd1306",
		description: "OLED controller IC (ssd1306 | sh1106)"
	},
	pluginInstanceId: {
		type: "string",
		default: "",
		description: "Simulation plugin instance id, e.g. ssd1306_i2c:0",
		advanced: !0
	},
	pluginIndex: {
		type: "number",
		default: -1,
		description: "Simulation plugin instance index; -1 means unspecified",
		advanced: !0
	}
}, F = N.pinsOverlay, I = e({
	type: x.type,
	size: _,
	wireColor: "#a855f7",
	rotationPolicy: "fixed",
	pinsOverlay: F,
	props: P,
	canvas: p,
	world: m,
	ui: {
		canvasProps: (e, t) => ({
			displayFrame: A(t, e),
			framebuffer: j(t, e)
		}),
		worldProps: (e, t) => {
			let n = A(t, e);
			return {
				pinConnections: e.pinConnections,
				displayFrame: n,
				framebuffer: j(t, e)
			};
		}
	}
});
//#endregion
export { I as default, I as oledDefinition, A as pickDisplayFrame };
