import { definePeripheral as e, pinsFromBinderVariant as t } from "@wink-ai/unisim-ui";
import { resolvePluginIdentity as n } from "@wink-ai/unisim";
import { computed as r, createElementBlock as i, createElementVNode as a, defineComponent as o, openBlock as s, toDisplayString as c } from "vue";
import "@wokwi/elements";
//#region builtin/button/1.0.0/src/CanvasGlyph.vue?vue&type=script&setup=true&lang.ts
var l = [
	"color",
	"label",
	"xray"
], u = /* @__PURE__ */ o({
	__name: "CanvasGlyph",
	props: {
		color: {},
		label: {},
		xray: { type: Boolean }
	},
	emits: ["buttonPress", "buttonRelease"],
	setup(e) {
		return (t, n) => (s(), i("wokwi-pushbutton", {
			color: e.color,
			label: e.label,
			xray: e.xray,
			onButtonPress: n[0] ||= (e) => t.$emit("buttonPress"),
			onButtonRelease: n[1] ||= (e) => t.$emit("buttonRelease")
		}, null, 40, l));
	}
}), d = { class: "virtual-button" }, f = { class: "component-label" }, p = { class: "btn-wrapper" }, m = [
	"color",
	"label",
	"xray"
], h = /*#__PURE__*/ ((e, t) => {
	let n = e.__vccOpts || e;
	for (let [e, r] of t) n[e] = r;
	return n;
})(/* @__PURE__ */ o({
	__name: "WorldWidget",
	props: {
		pluginInstanceId: {},
		pinConnections: {},
		color: {},
		label: {},
		xray: { type: Boolean },
		activeLow: { type: Boolean }
	},
	emits: ["buttonPress", "buttonRelease"],
	setup(e, { emit: t }) {
		let n = e, o = t, l = r(() => `1.l:${n.pinConnections ? n.pinConnections["1.l"] : void 0}, 2.l:${n.pinConnections ? n.pinConnections["2.l"] : void 0}, 1.r:${n.pinConnections ? n.pinConnections["1.r"] : void 0}, 2.r:${n.pinConnections ? n.pinConnections["2.r"] : void 0}`);
		function u() {
			o("buttonPress");
		}
		function h() {
			o("buttonRelease");
		}
		return (t, n) => (s(), i("div", d, [a("div", f, "Button (" + c(l.value) + ")", 1), a("div", p, [a("wokwi-pushbutton", {
			color: e.color,
			label: e.label,
			xray: e.xray,
			onButtonPress: u,
			onButtonRelease: h
		}, null, 40, m)])]));
	}
}), [["__scopeId", "data-v-bb07f06d"]]);
Object.freeze({});
var g = Object.freeze({
	"1.l": Object.freeze({
		relX: -5,
		relY: 20,
		wireNet: "primary"
	}),
	"2.l": Object.freeze({
		relX: -5,
		relY: 40,
		wireNet: "gnd",
		defaultConnection: "GND"
	}),
	"1.r": Object.freeze({
		relX: 75,
		relY: 13,
		wireNet: "primary"
	}),
	"2.r": Object.freeze({
		relX: 75,
		relY: 33,
		wireNet: "gnd"
	})
}), _ = Object.freeze({ default: Object.freeze({
	variant: "default",
	getPins: () => t("button", "default"),
	pinsOverlay: g,
	defaultAppearanceId: "button_default"
}) });
Object.freeze({ button_default: Object.freeze({
	appearanceId: "button_default",
	variant: "default",
	displayName: "GPIO Push Button",
	searchAliases: Object.freeze([
		"button",
		"gpio",
		"push",
		"input",
		"default"
	])
}) });
//#endregion
//#region builtin/button/1.0.0/src/definition.ts
var v = n(import.meta.url, "button", "1.0.0", "input"), y = _.default, b = e({
	type: v.type,
	size: {
		width: 80,
		height: 60
	},
	wireColor: "#38bdf8",
	pinsOverlay: y.pinsOverlay,
	props: {
		variant: {
			type: "string",
			default: "default",
			description: "Button topology variant"
		},
		appearanceId: {
			type: "string",
			default: "button_default",
			description: "Display appearance id"
		},
		color: {
			type: "string",
			default: "red",
			description: "Button color",
			options: [
				"red",
				"green",
				"blue",
				"yellow",
				"white",
				"black"
			]
		},
		label: {
			type: "string",
			default: "",
			description: "Label text"
		},
		xray: {
			type: "boolean",
			default: !1,
			description: "Show internal structure"
		},
		activeLow: {
			type: "boolean",
			default: !0,
			description: "Active low mode (pull-up)"
		}
	},
	canvas: u,
	world: h,
	ui: {
		canvasProps: (e) => ({
			color: e.props.color,
			label: e.props.label,
			xray: e.props.xray
		}),
		worldProps: (e) => ({
			pinConnections: e.pinConnections,
			color: e.props.color,
			label: e.props.label,
			xray: e.props.xray,
			activeLow: e.props.activeLow,
			...e.props.pluginInstanceId ? { pluginInstanceId: e.props.pluginInstanceId } : {}
		})
	}
});
//#endregion
export { b as buttonDefinition, b as default };
