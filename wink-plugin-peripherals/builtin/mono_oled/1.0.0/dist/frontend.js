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
});
Object.freeze({
	ssd1306_i2c: [],
	ssd1306_spi: []
});
var h = Object.freeze({
	DATA: Object.freeze({
		relX: 40,
		relY: 75,
		wireNet: "primary",
		defaultConnection: 21,
		required: !0
	}),
	CLK: Object.freeze({
		relX: 50,
		relY: 75,
		wireNet: "secondary",
		defaultConnection: 22,
		required: !0
	}),
	"3V3": Object.freeze({
		relX: 90,
		relY: 75,
		wireNet: "vcc",
		defaultConnection: "3V3",
		required: !1
	}),
	GND: Object.freeze({
		relX: 110,
		relY: 75,
		wireNet: "gnd",
		defaultConnection: "GND",
		required: !1
	})
}), g = Object.freeze({
	CLK: Object.freeze({
		relX: 30,
		relY: 75,
		wireNet: "secondary",
		defaultConnection: 18,
		required: !0
	}),
	DIN: Object.freeze({
		relX: 45,
		relY: 75,
		wireNet: "primary",
		defaultConnection: 23,
		required: !0
	}),
	CS: Object.freeze({
		relX: 60,
		relY: 75,
		wireNet: "secondary",
		defaultConnection: 5,
		required: !1
	}),
	DC: Object.freeze({
		relX: 75,
		relY: 75,
		wireNet: "secondary",
		defaultConnection: 17,
		required: !0
	}),
	RES: Object.freeze({
		relX: 90,
		relY: 75,
		wireNet: "secondary",
		defaultConnection: 16,
		required: !1
	}),
	"3V3": Object.freeze({
		relX: 105,
		relY: 75,
		wireNet: "vcc",
		defaultConnection: "3V3",
		required: !1
	}),
	GND: Object.freeze({
		relX: 120,
		relY: 75,
		wireNet: "gnd",
		defaultConnection: "GND",
		required: !1
	})
}), _ = Object.freeze({
	ssd1306_i2c: Object.freeze({
		variant: "ssd1306_i2c",
		getPins: () => t("mono_oled", "ssd1306_i2c"),
		pinsOverlay: h,
		defaultAppearanceId: "mono_oled_ssd1306_i2c"
	}),
	ssd1306_spi: Object.freeze({
		variant: "ssd1306_spi",
		getPins: () => t("mono_oled", "ssd1306_spi"),
		pinsOverlay: g,
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
var v = 128, y = 64, b = n(import.meta.url, "mono_oled", "1.0.0", "display"), x = /* @__PURE__ */ new Set(["ssd1306_fb", "framebuffer"]), S = [
	"displayFrameInstanceId",
	"pluginInstanceId",
	"runtimeInstanceId",
	"simulationInstanceId",
	"pluginId"
], C = [
	"displayIndex",
	"pluginIndex",
	"instanceIndex"
];
function w(e) {
	return !e.kind || x.has(e.kind);
}
function T(e) {
	return e || "mono_oled";
}
function E(e, t) {
	for (let n of t) {
		let t = e[n];
		if (typeof t == "string" && t.trim()) return t;
	}
	return null;
}
function D(e, t) {
	for (let n of t) {
		let t = e[n];
		if (typeof t == "number" && Number.isInteger(t) && t >= 0) return t;
		if (typeof t == "string" && /^\d+$/.test(t)) return Number(t);
	}
	return null;
}
function O(e) {
	if (!e) return [];
	if (typeof e == "string") return [e];
	let t = [], n = e.props || {}, r = E(n, S);
	r && t.push(r);
	let i = D(n, C);
	return i !== null && t.push(`${T(e.type)}:${i}`), t.push(e.id), Array.from(new Set(t));
}
function k(e, t) {
	let n = e.displayFrames ?? [], r = O(t);
	for (let t of r) {
		let r = e.getDisplayFrame?.(t) ?? n.find((e) => e.instanceId === t) ?? null;
		if (r) return r;
	}
	let i = n.filter(w);
	return !t || r.length === 0 ? i[0] ?? n[0] ?? null : i.length === 1 ? i[0] : null;
}
function A(e, t) {
	return k(e, t)?.fb ?? null;
}
var j = "ssd1306_i2c", M = _[j], N = {
	variant: {
		type: "string",
		default: j,
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
}, P = M.pinsOverlay, F = e({
	type: b.type,
	size: {
		width: v,
		height: y
	},
	wireColor: "#a855f7",
	pinsOverlay: P,
	props: N,
	canvas: p,
	world: m,
	ui: {
		canvasProps: (e, t) => ({
			displayFrame: k(t, e),
			framebuffer: A(t, e)
		}),
		worldProps: (e, t) => {
			let n = k(t, e);
			return {
				pinConnections: e.pinConnections,
				displayFrame: n,
				framebuffer: A(t, e)
			};
		}
	}
});
//#endregion
export { F as default, F as oledDefinition, k as pickDisplayFrame };
