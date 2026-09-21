import { definePeripheral as e, pinsFromBinderVariant as t, resolvePluginInstanceId as n } from "@wink-ai/unisim-ui";
import { resolvePluginIdentity as r } from "@wink-ai/unisim-sdk";
import { computed as i, createElementBlock as a, createElementVNode as o, defineComponent as s, normalizeStyle as c, openBlock as l } from "vue";
import "@wokwi/elements";
//#region builtin/led/1.0.0/src/CanvasGlyph.vue?vue&type=script&setup=true&lang.ts
var u = [
	"pin",
	"color",
	"value",
	"brightness",
	"flip"
], d = /*#__PURE__*/ ((e, t) => {
	let n = e.__vccOpts || e;
	for (let [e, r] of t) n[e] = r;
	return n;
})(/* @__PURE__ */ s({
	__name: "CanvasGlyph",
	props: {
		pinConnections: {},
		color: {},
		brightness: {},
		label: {},
		flip: { type: Boolean },
		level: { type: Boolean }
	},
	setup(e) {
		let t = e, n = i(() => t.level ? { filter: `drop-shadow(0 0 9px ${t.color}) drop-shadow(0 0 3px ${t.color})` } : void 0);
		return (t, r) => (l(), a("div", {
			class: "led-glyph",
			style: c(n.value)
		}, [o("wokwi-led", {
			pin: typeof e.pinConnections?.A == "number" ? e.pinConnections.A : 1,
			color: e.color,
			value: e.level,
			brightness: e.brightness,
			label: "",
			flip: e.flip
		}, null, 8, u)], 4));
	}
}), [["__scopeId", "data-v-fb3834d1"]]), f = [
	"pin",
	"color",
	"value",
	"brightness",
	"label",
	"flip"
], p = /* @__PURE__ */ s({
	__name: "WorldWidget",
	props: {
		pinConnections: {},
		color: {},
		brightness: {},
		label: {},
		flip: { type: Boolean },
		level: { type: Boolean }
	},
	setup(e) {
		return (t, n) => (l(), a("wokwi-led", {
			pin: typeof e.pinConnections?.A == "number" ? e.pinConnections.A : 1,
			color: e.color,
			value: e.level,
			brightness: e.brightness,
			label: e.label,
			flip: e.flip
		}, null, 8, f));
	}
}), m = Object.freeze({
	width: 40,
	height: 50
}), h = Object.freeze({
	A: Object.freeze({
		relX: 24,
		relY: 42,
		wireNet: "primary",
		defaultConnection: null,
		required: !0
	}),
	C: Object.freeze({
		relX: 16,
		relY: 42,
		wireNet: "gnd",
		defaultConnection: "GND",
		required: !1
	})
});
Object.freeze({});
var g = h, _ = m, v = Object.freeze({ default: Object.freeze({
	variant: "default",
	getPins: () => t("led", "default"),
	pinsOverlay: g,
	defaultAppearanceId: "led_default"
}) });
Object.freeze({ led_default: Object.freeze({
	appearanceId: "led_default",
	variant: "default",
	displayName: "GPIO LED",
	searchAliases: Object.freeze([
		"led",
		"gpio",
		"indicator",
		"default"
	])
}) });
//#endregion
//#region builtin/led/1.0.0/src/definition.ts
var y = r(import.meta.url, "led", "1.0.0", "output");
function b(e, t) {
	let r = n(e, y.type);
	return t.pluginChannels?.[r]?.on === !0;
}
var x = v.default, S = e({
	type: y.type,
	size: _,
	wireColor: "#00ff88",
	pinsOverlay: x.pinsOverlay,
	props: {
		variant: {
			type: "string",
			default: "default",
			description: "LED topology variant"
		},
		appearanceId: {
			type: "string",
			default: "led_default",
			description: "Display appearance id"
		},
		color: {
			type: "string",
			default: "red",
			description: "LED color",
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
		brightness: {
			type: "number",
			default: 1,
			description: "Brightness (0-1)"
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
		contactWelded: {
			type: "boolean",
			default: !1,
			description: "Contact welded fault state"
		},
		pullDownResistor: {
			type: "number",
			default: 0,
			description: "External pull-down resistor in ohms (e.g. 4700 for MCS-51 reset clamping)"
		}
	},
	canvas: d,
	world: p,
	ui: {
		canvasProps: (e, t) => ({
			pinConnections: e.pinConnections,
			color: e.props.color,
			brightness: e.props.brightness,
			label: e.props.label,
			flip: e.props.flip,
			level: b(e, t)
		}),
		worldProps: (e, t) => ({
			pinConnections: e.pinConnections,
			color: e.props.color,
			level: b(e, t),
			brightness: e.props.brightness,
			label: e.props.label,
			flip: e.props.flip
		})
	}
});
//#endregion
export { S as default, S as ledDefinition };
