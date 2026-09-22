import { definePeripheral as e, pinsFromBinderVariant as t } from "@wink-ai/unisim-ui";
import { resolvePluginIdentity as n } from "@wink-ai/unisim-sdk";
import { computed as r, createElementBlock as i, createElementVNode as a, defineComponent as o, onBeforeUnmount as s, openBlock as c, ref as l, toDisplayString as u, withModifiers as d } from "vue";
import "@wokwi/elements";
//#region builtin/button/1.0.0/src/CanvasGlyph.vue?vue&type=script&setup=true&lang.ts
var f = [
	"color",
	"xray",
	"pressed"
], p = /*@__PURE__*/ o({
	__name: "CanvasGlyph",
	props: {
		color: {},
		label: {},
		xray: { type: Boolean },
		readonly: {
			type: Boolean,
			default: !0
		}
	},
	emits: ["buttonPress", "buttonRelease"],
	setup(e, { emit: t }) {
		let n = e, r = t, o = l(!1), u = l(!1), p = null;
		function m(e) {
			if (e.button !== 0 || n.readonly === !1) return;
			if (e.preventDefault(), e.stopPropagation(), u.value) {
				u.value = !1, o.value = !1, r("buttonRelease");
				return;
			}
			o.value = !0, r("buttonPress");
			let t = (e) => {
				if (p &&= (p(), null), o.value) {
					if (e.ctrlKey || e.metaKey) {
						u.value = !0;
						return;
					}
					o.value = !1, r("buttonRelease");
				}
			};
			window.addEventListener("pointerup", t), window.addEventListener("pointercancel", t), p = () => {
				window.removeEventListener("pointerup", t), window.removeEventListener("pointercancel", t);
			};
		}
		return s(() => {
			p &&= (p(), null);
		}), (t, n) => (c(), i("div", {
			class: "button-glyph-wrapper",
			draggable: "false",
			onPointerdown: m,
			onDragstart: n[0] ||= d(() => {}, ["prevent"]),
			onSelectstart: n[1] ||= d(() => {}, ["prevent"]),
			onContextmenu: n[2] ||= d(() => {}, ["prevent"])
		}, [a("wokwi-pushbutton", {
			color: e.color,
			label: "",
			xray: e.xray,
			pressed: o.value,
			style: { "pointer-events": "none" }
		}, null, 8, f)], 32));
	}
}), m = (e, t) => {
	let n = e.__vccOpts || e;
	for (let [e, r] of t) n[e] = r;
	return n;
}, h = /*#__PURE__*/ m(p, [["__scopeId", "data-v-8c68b518"]]), g = { class: "virtual-button" }, _ = { class: "component-label" }, v = [
	"color",
	"label",
	"xray",
	"pressed"
], y = /*#__PURE__*/ m(/* @__PURE__ */ o({
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
		let n = e, o = t, f = l(!1), p = l(!1), m = null, h = r(() => `1.l:${n.pinConnections ? n.pinConnections["1.l"] : void 0}, 2.l:${n.pinConnections ? n.pinConnections["2.l"] : void 0}, 1.r:${n.pinConnections ? n.pinConnections["1.r"] : void 0}, 2.r:${n.pinConnections ? n.pinConnections["2.r"] : void 0}`);
		function y(e) {
			if (e.button !== 0) return;
			if (e.preventDefault(), e.stopPropagation(), p.value) {
				p.value = !1, f.value = !1, o("buttonRelease");
				return;
			}
			f.value = !0, o("buttonPress");
			let t = (e) => {
				if (m &&= (m(), null), f.value) {
					if (e.ctrlKey || e.metaKey) {
						p.value = !0;
						return;
					}
					f.value = !1, o("buttonRelease");
				}
			};
			window.addEventListener("pointerup", t), window.addEventListener("pointercancel", t), m = () => {
				window.removeEventListener("pointerup", t), window.removeEventListener("pointercancel", t);
			};
		}
		return s(() => {
			m &&= (m(), null);
		}), (t, n) => (c(), i("div", g, [a("div", _, "Button (" + u(h.value) + ")", 1), a("div", {
			class: "btn-wrapper",
			draggable: "false",
			onPointerdown: y,
			onDragstart: n[0] ||= d(() => {}, ["prevent"]),
			onSelectstart: n[1] ||= d(() => {}, ["prevent"]),
			onContextmenu: n[2] ||= d(() => {}, ["prevent"])
		}, [a("wokwi-pushbutton", {
			color: e.color,
			label: e.label,
			xray: e.xray,
			pressed: f.value,
			style: { "pointer-events": "none" }
		}, null, 8, v)], 32)]));
	}
}), [["__scopeId", "data-v-1a915d69"]]), b = Object.freeze({
	width: 69,
	height: 46
}), x = Object.freeze({
	"1.l": Object.freeze({
		relX: 2,
		relY: 9,
		wireNet: "primary",
		defaultConnection: null,
		required: !0
	}),
	"2.l": Object.freeze({
		relX: 2,
		relY: 36,
		wireNet: "gnd",
		defaultConnection: "GND",
		required: !1
	}),
	"1.r": Object.freeze({
		relX: 65,
		relY: 9,
		wireNet: "primary",
		defaultConnection: null,
		required: !1
	}),
	"2.r": Object.freeze({
		relX: 65,
		relY: 36,
		wireNet: "gnd",
		defaultConnection: null,
		required: !1
	})
});
Object.freeze({});
var S = x, C = b, w = Object.freeze({ default: Object.freeze({
	variant: "default",
	getPins: () => t("button", "default"),
	pinsOverlay: S,
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
var T = n(import.meta.url, "button", "1.0.0", "input"), E = w.default, D = e({
	type: T.type,
	size: C,
	wireColor: "#38bdf8",
	pinsOverlay: E.pinsOverlay,
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
	canvas: h,
	world: y,
	ui: {
		canvasProps: (e, t) => ({
			color: e.props.color,
			label: e.props.label,
			xray: e.props.xray,
			...t?.readonly === void 0 ? {} : { readonly: t.readonly }
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
export { D as buttonDefinition, D as default };
