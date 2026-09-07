import { definePeripheral as e, pinsFromBinderVariant as t } from "@wink-ai/unisim-ui";
import { resolvePluginIdentity as n } from "@wink-ai/unisim";
import { computed as r, createElementBlock as i, createElementVNode as a, defineComponent as o, openBlock as s, ref as c, toDisplayString as l, withModifiers as u, onBeforeUnmount as unmountFn } from "vue";
import "@wokwi/elements";
//#region builtin/button/1.0.0/src/CanvasGlyph.vue?vue&type=script&setup=true&lang.ts
var d = [
	"color",
	"label",
	"xray",
	"pressed"
], f = /*@__PURE__*/ o({
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
		let n = e, r = t, o = c(!1), l = c(!1), k = null;
		function f(e) {
			if (e.button === 0 && n.readonly !== !1) {
				if (e.preventDefault(), e.stopPropagation(), l.value) {
					l.value = !1, o.value = !1, r("buttonRelease");
					return;
				}
				o.value = !0, r("buttonPress");
				let handleGlobalUp = e => {
					k && (k(), k = null);
					if (!o.value) return;
					if (e.ctrlKey || e.metaKey) {
						l.value = !0;
						return;
					}
					o.value = !1, r("buttonRelease");
				};
				window.addEventListener("pointerup", handleGlobalUp);
				window.addEventListener("pointercancel", handleGlobalUp);
				k = () => {
					window.removeEventListener("pointerup", handleGlobalUp);
					window.removeEventListener("pointercancel", handleGlobalUp);
				};
			}
		}
		unmountFn(() => {
			k && (k(), k = null);
		});
		return (t, n) => (s(), i("div", {
			class: "button-glyph-wrapper",
			draggable: "false",
			onPointerdown: f,
			onDragstart: n[0] ||= u(() => {}, ["prevent"]),
			onSelectstart: n[1] ||= u(() => {}, ["prevent"]),
			onContextmenu: n[2] ||= u(() => {}, ["prevent"])
		}, [a("wokwi-pushbutton", {
			color: e.color,
			label: e.label,
			xray: e.xray,
			pressed: o.value,
			style: { "pointer-events": "none" }
		}, null, 8, d)], 32));
	}
}), p = (e, t) => {
	let n = e.__vccOpts || e;
	for (let [e, r] of t) n[e] = r;
	return n;
}, m = /*@__PURE__*/ p(f, [["__scopeId", "data-v-c563efee"]]);
//#endregion
//#region builtin/button/1.0.0/src/WorldWidget.vue?vue&type=script&setup=true&lang.ts
var h = { class: "virtual-button" }, g = { class: "component-label" }, _ = [
	"color",
	"label",
	"xray",
	"pressed"
], v = /*#__PURE__*/ p(/* @__PURE__ */ o({
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
		let n = e, o = t, d = c(!1), f = c(!1), k = null, p = r(() => `1.l:${n.pinConnections ? n.pinConnections["1.l"] : void 0}, 2.l:${n.pinConnections ? n.pinConnections["2.l"] : void 0}, 1.r:${n.pinConnections ? n.pinConnections["1.r"] : void 0}, 2.r:${n.pinConnections ? n.pinConnections["2.r"] : void 0}`);
		function m(e) {
			if (e.button === 0) {
				if (e.preventDefault(), e.stopPropagation(), f.value) {
					f.value = !1, d.value = !1, o("buttonRelease");
					return;
				}
				d.value = !0, o("buttonPress");
				let handleGlobalUp = e => {
					k && (k(), k = null);
					if (!d.value) return;
					if (e.ctrlKey || e.metaKey) {
						f.value = !0;
						return;
					}
					d.value = !1, o("buttonRelease");
				};
				window.addEventListener("pointerup", handleGlobalUp);
				window.addEventListener("pointercancel", handleGlobalUp);
				k = () => {
					window.removeEventListener("pointerup", handleGlobalUp);
					window.removeEventListener("pointercancel", handleGlobalUp);
				};
			}
		}
		unmountFn(() => {
			k && (k(), k = null);
		});
		return (t, n) => (s(), i("div", h, [a("div", g, "Button (" + l(p.value) + ")", 1), a("div", {
			class: "btn-wrapper",
			draggable: "false",
			onPointerdown: m,
			onDragstart: n[0] ||= u(() => {}, ["prevent"]),
			onSelectstart: n[1] ||= u(() => {}, ["prevent"]),
			onContextmenu: n[2] ||= u(() => {}, ["prevent"])
		}, [a("wokwi-pushbutton", {
			color: e.color,
			label: e.label,
			xray: e.xray,
			pressed: d.value,
			style: { "pointer-events": "none" }
		}, null, 8, _)], 32)]));
	}
}), [["__scopeId", "data-v-1436a574"]]);
Object.freeze({});
var y = Object.freeze({
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
}), b = Object.freeze({ default: Object.freeze({
	variant: "default",
	getPins: () => t("button", "default"),
	pinsOverlay: y,
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
var x = n(import.meta.url, "button", "1.0.0", "input"), S = b.default, C = e({
	type: x.type,
	size: {
		width: 80,
		height: 60
	},
	wireColor: "#38bdf8",
	pinsOverlay: S.pinsOverlay,
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
	canvas: m,
	world: v,
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
export { C as buttonDefinition, C as default };
