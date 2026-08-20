import { definePeripheral as e, pinsFromBinderVariant as t, resolvePluginInstanceId as n } from "@wink-ai/unisim-ui";
import { resolvePluginIdentity as r } from "@wink-ai/unisim";
import { createElementBlock as i, createElementVNode as a, defineComponent as o, openBlock as s, toDisplayString as c } from "vue";
import "@wokwi/elements";
//#region builtin/rc_servo/1.0.0/src/CanvasGlyph.vue?vue&type=script&setup=true&lang.ts
var l = { class: "servo-container" }, u = ["angle"], d = { class: "label" }, f = /*#__PURE__*/ ((e, t) => {
	let n = e.__vccOpts || e;
	for (let [e, r] of t) n[e] = r;
	return n;
})(/* @__PURE__ */ o({
	__name: "CanvasGlyph",
	props: {
		id: {},
		label: {},
		pwmChannel: {},
		angle: {}
	},
	setup(e) {
		return (t, n) => (s(), i("div", l, [a("wokwi-servo", { angle: e.angle }, null, 8, u), a("span", d, c(e.label || e.id) + " (" + c(Math.round(e.angle)) + "°)", 1)]));
	}
}), [["__scopeId", "data-v-24cf67f0"]]);
Object.freeze({});
var p = Object.freeze({
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
}), m = Object.freeze({ sg90: Object.freeze({
	variant: "sg90",
	getPins: () => t("rc_servo", "sg90"),
	pinsOverlay: p,
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
var h = r(import.meta.url, "rc_servo", "1.0.0", "actuator"), g = m.sg90, _ = e({
	type: h.type,
	size: {
		width: 80,
		height: 60
	},
	wireColor: "#3b82f6",
	pinsOverlay: g.pinsOverlay,
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
	canvas: f,
	ui: { canvasProps: (e, t) => {
		let r = n(e, "rc_servo"), i = t.pluginChannels?.[r]?.angle, a = typeof i == "number" ? i : 90;
		return {
			id: e.id,
			label: e.props.label ?? e.id,
			pwmChannel: e.props.pwmChannel,
			angle: a
		};
	} }
});
//#endregion
export { _ as default, _ as servoDefinition };
