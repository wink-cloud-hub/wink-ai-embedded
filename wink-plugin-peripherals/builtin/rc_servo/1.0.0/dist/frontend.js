import { definePeripheral as e, pinsFromBinderVariant as t, resolvePluginInstanceId as n } from "@wink-ai/unisim-ui";
import { normalizeManifest as r, resolvePluginIdentity as i } from "@wink-ai/unisim-sdk";
import { computed as a, createElementBlock as o, createElementVNode as s, defineComponent as c, normalizeClass as l, normalizeStyle as u, openBlock as d, toDisplayString as f } from "vue";
import "@wokwi/elements";
//#region builtin/rc_servo/1.0.0/src/CanvasGlyph.vue?vue&type=script&setup=true&lang.ts
var p = {
	class: "servo-container",
	style: {
		position: "relative",
		display: "flex",
		"align-items": "center",
		"justify-content": "center",
		width: "100%",
		height: "100%"
	}
}, m = ["angle"], h = /*#__PURE__*/ ((e, t) => {
	let n = e.__vccOpts || e;
	for (let [e, r] of t) n[e] = r;
	return n;
})(/* @__PURE__ */ c({
	__name: "CanvasGlyph",
	props: {
		id: {},
		label: {},
		pwmChannel: {},
		angle: {},
		rotation: {}
	},
	setup(e) {
		let t = e, n = a(() => Math.abs((t.rotation ?? 0) % 180) === 90), r = a(() => {
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
		return (t, i) => (d(), o("div", p, [s("wokwi-servo", { angle: e.angle }, null, 8, m), s("span", {
			class: l(["label", { "is-transposed": n.value }]),
			style: u(r.value)
		}, f(e.label || e.id) + " (" + f(Math.round(e.angle)) + "°) ", 7)]));
	}
}), [["__scopeId", "data-v-8d19946d"]]), g = Object.freeze({
	width: 171,
	height: 120
}), _ = Object.freeze({
	GND: Object.freeze({
		relX: 0,
		relY: 50,
		wireNet: "gnd",
		defaultConnection: "GND",
		required: !1
	}),
	VCC: Object.freeze({
		relX: 0,
		relY: 60,
		wireNet: "vcc",
		defaultConnection: "VCC",
		required: !1
	}),
	PWM: Object.freeze({
		relX: 0,
		relY: 69,
		wireNet: "primary",
		defaultConnection: null,
		required: !0
	})
});
Object.freeze({});
var v = _, y = g, b = Object.freeze({ sg90: Object.freeze({
	variant: "sg90",
	getPins: () => t("rc_servo", "sg90"),
	pinsOverlay: v,
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
//#region builtin/rc_servo/1.0.0/src/simulation.ts
var x = i(import.meta.url, "rc_servo", "1.0.0", "output"), S = { sg90: {
	displayName: "RC Servo Motor",
	pins: [
		{
			name: "PWM",
			pinType: "pwm",
			required: !0
		},
		{
			name: "VCC",
			pinType: "vcc",
			voltage: "5V",
			required: !1
		},
		{
			name: "GND",
			pinType: "gnd",
			required: !1
		}
	]
} };
function C(e = "sg90") {
	let t = S[e] ?? S.sg90;
	return r({
		type: x.type,
		version: x.version,
		category: x.category,
		displayName: t.displayName,
		description: "RC Servo Motor (0-180 degrees)",
		timingModel: "event-driven",
		pins: t.pins,
		properties: {
			variant: {
				type: "string",
				default: e
			},
			minAngle: {
				type: "number",
				default: 0,
				min: 0,
				max: 180
			},
			maxAngle: {
				type: "number",
				default: 180,
				min: 0,
				max: 180
			},
			minPulseMs: {
				type: "number",
				default: .5,
				min: .3,
				max: 1
			},
			maxPulseMs: {
				type: "number",
				default: 2.5,
				min: 2,
				max: 3
			},
			framePeriodMs: {
				type: "number",
				default: 20,
				min: 1,
				max: 100,
				unit: "ms"
			},
			pwmChannel: {
				type: "number",
				default: 0,
				min: 0,
				max: 15
			}
		},
		stateChannels: {
			angle: {
				type: "number",
				default: 90,
				unit: "degrees",
				description: "Current servo angle"
			},
			targetAngle: {
				type: "number",
				default: 90,
				unit: "degrees",
				show: !1,
				description: "Target angle (smoothing)"
			}
		},
		events: { SET_ANGLE: {
			description: "Set servo target angle",
			params: { angle: {
				type: "number",
				required: !0,
				unit: "degrees"
			} }
		} }
	});
}
C("sg90");
//#endregion
//#region builtin/rc_servo/1.0.0/src/definition.ts
var w = i(import.meta.url, "rc_servo", "1.0.0", "actuator"), T = b.sg90, E = e({
	type: w.type,
	displayName: "RC Servo Motor",
	category: "actuator",
	manifest: C(),
	size: y,
	wireColor: "#3b82f6",
	pinsOverlay: T.pinsOverlay,
	props: {
		angle: {
			type: "number",
			default: 90,
			description: "Current Angle (degrees)",
			range: {
				min: 0,
				max: 180,
				step: 1
			}
		},
		invert: {
			type: "boolean",
			default: !1,
			description: "Invert direction"
		},
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
	canvas: h,
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
export { E as default, E as servoDefinition };
