import { BaseSimulationPlugin as e, defaultRolePinName as t, normalizeManifest as n, normalizeVariantKey as r, resolveMappedRolePinName as i, resolvePluginIdentity as a } from "@wink-ai/unisim";
//#region builtin/rc_servo/1.0.0/src/physics/pwm-angle-duty.ts
function o(e, t) {
	let n = t.maxAngle - t.minAngle || 1, r = Math.min(1, Math.max(0, (e - t.minAngle) / n)), i = t.minPulseMs + r * (t.maxPulseMs - t.minPulseMs), a = t.framePeriodMs || 20;
	return Math.min(1, Math.max(0, i / a));
}
function s(e, t) {
	let n = t.framePeriodMs || 20, r = t.minPulseMs / n, i = t.maxPulseMs / n - r || 1, a = Math.min(1, Math.max(0, (e - r) / i));
	return t.minAngle + a * (t.maxAngle - t.minAngle);
}
//#endregion
//#region builtin/rc_servo/1.0.0/src/simulation.ts
var c = a(import.meta.url, "rc_servo", "1.0.0", "output"), l = { sg90: {
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
function u(e) {
	let t = r(e);
	return t && t in l ? t : "sg90";
}
function d(e = "sg90") {
	let t = l[e] ?? l.sg90;
	return n({
		type: c.type,
		version: c.version,
		category: c.category,
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
var f = d("sg90"), p = (e) => d(u(e)), m = class extends e {
	manifest = f;
	static manifest = f;
	get type() {
		return this.manifest.type;
	}
	angle = 90;
	pwmPinName = "PWM";
	onBound(e, n, r) {
		this.pwmPinName = i(this.manifest, "pwm", n) ?? t(this.manifest, "pwm") ?? "PWM", this.writeDuty();
	}
	getResolvedProps() {
		let e = this.properties || {};
		return {
			minAngle: this.properties?.minAngle ?? e.min_angle ?? 0,
			maxAngle: this.properties?.maxAngle ?? e.max_angle ?? 180,
			minPulseMs: this.properties?.minPulseMs ?? e.min_pulse_ms ?? .5,
			maxPulseMs: this.properties?.maxPulseMs ?? e.max_pulse_ms ?? 2.5,
			framePeriodMs: this.properties?.framePeriodMs ?? e.frame_period_ms ?? 20,
			pwmChannel: this.properties?.pwmChannel ?? e.pwm_channel ?? 0
		};
	}
	writeDuty() {
		let e = o(this.angle, this.getResolvedProps());
		this.ctx?.adc ? this.ctx.adc.writeNorm(this.pwmPinName, e) : typeof this.ctx?.analogWrite == "function" && this.ctx.analogWrite(this.pwmPinName, e);
	}
	clampAngle(e) {
		let { minAngle: t, maxAngle: n } = this.getResolvedProps();
		return Math.max(t, Math.min(n, e));
	}
	publishAngle(e) {
		this.angle = e, this.ctx?.publish("targetAngle", this.angle), this.ctx?.publish("angle", this.angle);
	}
	_angle(e) {
		this.publishAngle(this.clampAngle(e)), this.writeDuty();
	}
	onDutyChange(e, t) {
		let n = this.getResolvedProps();
		if (e !== n.pwmChannel) return;
		let r = this.clampAngle(s(t / 100, n));
		Math.abs(r - this.angle) < .01 || this.publishAngle(r);
	}
	onDestroy() {
		this.ctx?.gpio ? this.ctx.gpio.releasePin(this.pwmPinName) : this.ctx?.releasePin(this.pwmPinName);
	}
}, h = {
	manifest: f,
	manifestFactory: p,
	PluginClass: m
};
//#endregion
export { l as RC_SERVO_PIN_VARIANTS, m as RcServoPlugin, d as createRcServoManifest, h as default, f as rcServoManifest, p as rcServoManifestFactory };
