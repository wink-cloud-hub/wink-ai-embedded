import { definePeripheral as e, pinsFromBinderVariant as t } from "@wink-ai/unisim-ui";
import { resolvePluginIdentity as n } from "@wink-ai/unisim";
import { computed as r, createElementBlock as i, createElementVNode as a, defineComponent as o, openBlock as s, ref as c, toDisplayString as l } from "vue";
import "@wokwi/elements";
//#region builtin/button/1.0.0/src/CanvasGlyph.vue?vue&type=script&setup=true&lang.ts
var u = [
	"color",
	"label",
	"xray",
	"pressed"
], d = /*@__PURE__*/ o({
	__name: "CanvasGlyph",
	props: {
		color: {},
		label: {},
		xray: { type: Boolean }
	},
	emits: ["buttonPress", "buttonRelease"],
	setup(e, { emit: t }) {
		let n = t, r = c(!1), o = c(!1);
		function l(e) {
			if (e.button === 0) {
				if (o.value) {
					o.value = !1, r.value = !1, n("buttonRelease");
					return;
				}
				try {
					e.currentTarget.setPointerCapture(e.pointerId);
				} catch {}
				r.value = !0, n("buttonPress");
			}
		}
		function d(e) {
			try {
				e.currentTarget.releasePointerCapture(e.pointerId);
			} catch {}
			if (r.value) {
				if (e.ctrlKey || e.metaKey) {
					o.value = !0;
					return;
				}
				r.value = !1, n("buttonRelease");
			}
		}
		function f(e) {
			try {
				e.currentTarget.releasePointerCapture(e.pointerId);
			} catch {}
			r.value && !o.value && (r.value = !1, n("buttonRelease"));
		}
		return (t, n) => (s(), i("div", {
			class: "button-glyph-wrapper",
			onPointerdown: l,
			onPointerup: d,
			onPointercancel: f
		}, [a("wokwi-pushbutton", {
			color: e.color,
			label: e.label,
			xray: e.xray,
			pressed: r.value,
			style: { "pointer-events": "none" }
		}, null, 8, u)], 32));
	}
}), f = (e, t) => {
	let n = e.__vccOpts || e;
	for (let [e, r] of t) n[e] = r;
	return n;
}, p = /*#__PURE__*/ f(d, [["__scopeId", "data-v-87ea0df7"]]), m = { class: "virtual-button" }, h = { class: "component-label" }, g = [
	"color",
	"label",
	"xray",
	"pressed"
], _ = /*#__PURE__*/ f(/* @__PURE__ */ o({
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
		let n = e, o = t, u = c(!1), d = c(!1), f = r(() => `1.l:${n.pinConnections ? n.pinConnections["1.l"] : void 0}, 2.l:${n.pinConnections ? n.pinConnections["2.l"] : void 0}, 1.r:${n.pinConnections ? n.pinConnections["1.r"] : void 0}, 2.r:${n.pinConnections ? n.pinConnections["2.r"] : void 0}`);
		function p(e) {
			if (e.button === 0) {
				if (d.value) {
					d.value = !1, u.value = !1, o("buttonRelease");
					return;
				}
				try {
					e.currentTarget.setPointerCapture(e.pointerId);
				} catch {}
				u.value = !0, o("buttonPress");
			}
		}
		function _(e) {
			try {
				e.currentTarget.releasePointerCapture(e.pointerId);
			} catch {}
			if (u.value) {
				if (e.ctrlKey || e.metaKey) {
					d.value = !0;
					return;
				}
				u.value = !1, o("buttonRelease");
			}
		}
		function v(e) {
			try {
				e.currentTarget.releasePointerCapture(e.pointerId);
			} catch {}
			u.value && !d.value && (u.value = !1, o("buttonRelease"));
		}
		return (t, n) => (s(), i("div", m, [a("div", h, "Button (" + l(f.value) + ")", 1), a("div", {
			class: "btn-wrapper",
			onPointerdown: p,
			onPointerup: _,
			onPointercancel: v
		}, [a("wokwi-pushbutton", {
			color: e.color,
			label: e.label,
			xray: e.xray,
			pressed: u.value,
			style: { "pointer-events": "none" }
		}, null, 8, g)], 32)]));
	}
}), [["__scopeId", "data-v-4f593bfc"]]);
Object.freeze({});
var v = Object.freeze({
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
}), y = Object.freeze({ default: Object.freeze({
	variant: "default",
	getPins: () => t("button", "default"),
	pinsOverlay: v,
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
var b = n(import.meta.url, "button", "1.0.0", "input"), x = y.default, S = e({
	type: b.type,
	size: {
		width: 80,
		height: 60
	},
	wireColor: "#38bdf8",
	pinsOverlay: x.pinsOverlay,
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
	canvas: p,
	world: _,
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
export { S as buttonDefinition, S as default };
