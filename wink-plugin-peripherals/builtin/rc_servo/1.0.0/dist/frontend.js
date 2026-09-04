import { definePeripheral as e, pinsFromBinderVariant as t, resolvePluginInstanceId as n } from "@wink-ai/unisim-ui";
import { resolvePluginIdentity as r } from "@wink-ai/unisim";
import { computed as i, createElementBlock as a, createElementVNode as o, defineComponent as s, normalizeClass as c, normalizeStyle as l, openBlock as u, toDisplayString as d } from "vue";
import "@wokwi/elements";
//#region builtin/rc_servo/1.0.0/src/CanvasGlyph.vue?vue&type=script&setup=true&lang.ts
var f = {
	class: "servo-container",
	style: {
		position: "relative",
		display: "flex",
		"align-items": "center",
		"justify-content": "center",
		width: "100%",
		height: "100%"
	}
}, p = ["angle"], m = /*#__PURE__*/ ((e, t) => {
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
		let t = e, n = i(() => Math.abs((t.rotation ?? 0) % 180) === 90), r = i(() => {
			let e = n.value ? "48px" : "38px";
			return {
				position: "absolute",
				left: "50%",
				top: "50%",
				transformOrigin: "0 0",
				transform: `rotate(calc(-1 * var(--rot, ${t.rotation ?? 0}deg))) translateY(${e}) translateX(-50%)`,
				fontSize: "9px",
				color: "#94a3b8",
				whiteSpace: "nowrap",
				pointerEvents: "none",
				userSelect: "none",
				textAlign: "center",
				transition: "transform 0.15s ease"
			};
		});
		return (t, i) => (u(), a("div", f, [o("wokwi-servo", { angle: e.angle }, null, 8, p), o("span", {
			class: c(["label", { "is-transposed": n.value }]),
			style: l(r.value)
		}, d(e.label || e.id) + " (" + d(Math.round(e.angle)) + "°) ", 7)]));
	}
}), [["__scopeId", "data-v-8d19946d"]]);
Object.freeze({});
var h = Object.freeze({
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
}), g = Object.freeze({ sg90: Object.freeze({
	variant: "sg90",
	getPins: () => t("rc_servo", "sg90"),
	pinsOverlay: h,
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
var _ = r(import.meta.url, "rc_servo", "1.0.0", "actuator"), v = g.sg90, y = e({
	type: _.type,
	size: {
		width: 80,
		height: 60
	},
	wireColor: "#3b82f6",
	pinsOverlay: v.pinsOverlay,
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
	canvas: m,
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
export { y as default, y as servoDefinition };
