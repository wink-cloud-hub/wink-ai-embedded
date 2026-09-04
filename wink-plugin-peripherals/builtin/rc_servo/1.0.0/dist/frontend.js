import { definePeripheral as e, pinsFromBinderVariant as t, resolvePluginInstanceId as n } from "@wink-ai/unisim-ui";
import { resolvePluginIdentity as r } from "@wink-ai/unisim";
import { computed as i, createElementBlock as a, createElementVNode as o, defineComponent as s, normalizeClass as c, openBlock as l, toDisplayString as u } from "vue";
import "@wokwi/elements";
//#region builtin/rc_servo/1.0.0/src/CanvasGlyph.vue?vue&type=script&setup=true&lang.ts
var d = { class: "servo-container" }, f = ["angle"], p = /*#__PURE__*/ ((e, t) => {
	let n = e.__vccOpts || e;
	for (let [e, r] of t) n[e] = r;
	return n;
})(/* @__PURE__ */ s({
	__name: "CanvasGlyph",
	props: {
		id: {},
		label: {},
		pwmChannel: {},
		angle: {},
		rotation: {}
	},
	setup(e) {
		let t = e, n = i(() => Math.abs((t.rotation ?? 0) % 180) === 90);
		return (t, r) => (l(), a("div", d, [o("wokwi-servo", { angle: e.angle }, null, 8, f), o("span", { class: c(["label", { "is-transposed": n.value }]) }, u(e.label || e.id) + " (" + u(Math.round(e.angle)) + "°) ", 3)]));
	}
}), [["__scopeId", "data-v-414a5e05"]]);
Object.freeze({});
var m = Object.freeze({
	PWM: Object.freeze({
		relX: -5,
		relY: 50,
		wireNet: "primary",
		defaultConnection: null,
		required: !0
	}),
	VCC: Object.freeze({
		relX: -5,
		relY: 60,
		wireNet: "vcc",
		defaultConnection: "VCC",
		required: !1
	}),
	GND: Object.freeze({
		relX: -5,
		relY: 70,
		wireNet: "gnd",
		defaultConnection: "GND",
		required: !1
	})
}), h = Object.freeze({ sg90: Object.freeze({
	variant: "sg90",
	getPins: () => t("rc_servo", "sg90"),
	pinsOverlay: m,
	defaultAppearanceId: "rc_servo_sg90"
}) });
Object.freeze({ rc_servo_sg90: Object.freeze({
	appearanceId: "rc_servo_sg90",
	variant: "sg90",
	displayName: "SG90 9g Micro Servo",
	searchAliases: Object.freeze([
		"sg90",
		"servo",
		"pwm"
	])
}) });
//#endregion
//#region builtin/rc_servo/1.0.0/src/definition.ts
var g = r(import.meta.url, "rc_servo", "1.0.0", "actuator"), _ = h.sg90, v = e({
	type: g.type,
	size: {
		width: 80,
		height: 60
	},
	wireColor: "#3b82f6",
	pinsOverlay: _.pinsOverlay,
	props: {
		variant: {
			type: "string",
			default: "sg90",
			description: "RC servo topology variant"
		},
		appearanceId: {
			type: "string",
			default: "rc_servo_sg90",
			description: "Display appearance id"
		},
		minAngle: {
			type: "number",
			default: 0,
			description: "Min Angle (degrees)",
			range: {
				min: 0,
				max: 180,
				step: 1
			}
		},
		maxAngle: {
			type: "number",
			default: 180,
			description: "Max Angle (degrees)",
			range: {
				min: 0,
				max: 180,
				step: 1
			}
		},
		minPulseMs: {
			type: "number",
			default: .5,
			description: "Min Pulse Width (ms)"
		},
		maxPulseMs: {
			type: "number",
			default: 2.5,
			description: "Max Pulse Width (ms)"
		},
		framePeriodMs: {
			type: "number",
			default: 20,
			description: "Frame Period (ms)"
		},
		pwmChannel: {
			type: "number",
			default: 0,
			description: "PWM Channel",
			range: {
				min: 0,
				max: 15,
				step: 1
			}
		}
	},
	canvas: p,
	ui: { canvasProps: (e, t) => {
		let r = n(e, "rc_servo"), i = t.pluginChannels?.[r]?.angle, a = typeof i == "number" ? i : 90;
		return {
			id: e.id,
			label: e.props.label ?? e.id,
			pwmChannel: e.props.pwmChannel,
			angle: a,
			rotation: e.rotation ?? 0
		};
	} }
});
//#endregion
export { v as default, v as servoDefinition };
