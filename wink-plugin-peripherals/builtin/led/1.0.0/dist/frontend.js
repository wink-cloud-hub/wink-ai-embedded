import { definePeripheral as e, pinsFromBinderVariant as t, resolvePluginInstanceId as n } from "@wink-ai/unisim-ui";
import { resolvePluginIdentity as r } from "@wink-ai/unisim";
import { createElementBlock as i, defineComponent as a, openBlock as o } from "vue";
import "@wokwi/elements";
//#region builtin/led/1.0.0/src/CanvasGlyph.vue?vue&type=script&setup=true&lang.ts
var s = [
	"pin",
	"color",
	"value",
	"brightness",
	"label",
	"flip"
], c = /* @__PURE__ */ a({
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
		return (t, n) => (o(), i("wokwi-led", {
			pin: typeof e.pinConnections?.A == "number" ? e.pinConnections.A : 1,
			color: e.color,
			value: e.level,
			brightness: e.brightness,
			label: e.label,
			flip: e.flip
		}, null, 8, s));
	}
}), l = [
	"pin",
	"color",
	"value",
	"brightness",
	"label",
	"flip"
], u = /* @__PURE__ */ a({
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
		return (t, n) => (o(), i("wokwi-led", {
			pin: typeof e.pinConnections?.A == "number" ? e.pinConnections.A : 1,
			color: e.color,
			value: e.level,
			brightness: e.brightness,
			label: e.label,
			flip: e.flip
		}, null, 8, l));
	}
});
Object.freeze({});
var d = Object.freeze({
	A: Object.freeze({
		relX: 30,
		relY: 50,
		wireNet: "primary",
		defaultConnection: 13,
		required: !0
	}),
	C: Object.freeze({
		relX: 10,
		relY: 50,
		wireNet: "gnd",
		defaultConnection: "GND",
		required: !1
	})
}), f = Object.freeze({ default: Object.freeze({
	variant: "default",
	getPins: () => t("led", "default"),
	pinsOverlay: d,
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
var p = r(import.meta.url, "led", "1.0.0", "output");
function m(e, t) {
	let r = n(e, p.type);
	return t.pluginChannels?.[r]?.on === !0;
}
var h = f.default, g = e({
	type: p.type,
	size: {
		width: 50,
		height: 60
	},
	wireColor: "#00ff88",
	pinsOverlay: h.pinsOverlay,
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
		}
	},
	canvas: c,
	world: u,
	ui: {
		canvasProps: (e, t) => ({
			pinConnections: e.pinConnections,
			color: e.props.color,
			brightness: e.props.brightness,
			label: e.props.label,
			flip: e.props.flip,
			level: m(e, t)
		}),
		worldProps: (e, t) => ({
			pinConnections: e.pinConnections,
			color: e.props.color,
			level: m(e, t),
			brightness: e.props.brightness,
			label: e.props.label,
			flip: e.props.flip
		})
	}
});
//#endregion
export { g as default, g as ledDefinition };
