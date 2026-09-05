import { BaseSimulationPlugin as e, LogicStates as t, createThrottlePublish as n, normalizeManifest as r, normalizeVariantKey as i, resolvePluginIdentity as a } from "@wink-ai/unisim";
import { getBuiltinManifest as o } from "@wink-ai/unisim/sdk";
//#region node_modules/@wink-ai/unisim-ui/dist/index.js
var s = /* @__PURE__ */ new Map();
function c(e) {
	return e.catalogType ? e.catalogType : e.direction === "power" || e.direction === "ground" ? "power" : e.busGroup?.startsWith("i2c") ? "i2c" : e.simRole === "pwm" || e.signal === "analog" && e.direction === "source" ? "pwm" : "gpio";
}
function l(e) {
	let t = {
		name: e.name,
		direction: e.direction,
		signal: e.signal,
		catalogType: c(e)
	};
	return e.simRole && (t.simRole = e.simRole), e.description && (t.description = e.description), e.busGroup && (t.busGroup = e.busGroup), e.aliases?.length && (t.aliases = [...e.aliases]), e.required !== void 0 && (t.required = e.required), e.voltage && (t.voltage = e.voltage), t;
}
function u(e, t) {
	let n = `${e}:${t ?? ""}`, r = s.get(n);
	if (r) return r;
	let i = (o(e, t)?.pins ?? []).map(l), a = Object.freeze(i);
	return s.set(n, a), a;
}
Object.freeze([
	"direct_gpio_8d",
	"direct_gpio_4d",
	"direct_gpio_2d",
	"direct_gpio_1d"
]);
var d = Object.freeze({
	direct_gpio_8d: 8,
	direct_gpio_4d: 4,
	direct_gpio_2d: 2,
	direct_gpio_1d: 1
});
function f(e) {
	let t = i(e);
	return t && t in d ? t : "direct_gpio_8d";
}
Object.freeze({
	"8d": Object.freeze(["direct_gpio_8d"]),
	"4d": Object.freeze(["direct_gpio_4d"]),
	"2d": Object.freeze(["direct_gpio_2d"]),
	"1d": Object.freeze(["direct_gpio_1d"])
});
var p = [
	"A",
	"B",
	"C",
	"D",
	"E",
	"F",
	"G",
	"DP"
], m = [
	23,
	47,
	70,
	93,
	117,
	140,
	163,
	187
];
function h() {
	let e = {};
	for (let t = 0; t < p.length; t++) e[p[t]] = Object.freeze({
		relX: m[t],
		relY: 96,
		wireNet: "primary",
		required: !1
	});
	return e;
}
function g(e) {
	return Object.freeze(Object.entries(e).map(([e]) => ({
		name: e,
		direction: "sink",
		signal: "digital",
		catalogType: "gpio",
		required: !1
	})));
}
function _(e) {
	let t = h(), n = {}, r = d[e];
	if (r === 8) for (let e = 0; e < 8; e++) n[`DIG${e + 1}`] = Object.freeze({
		relX: m[e],
		relY: 0,
		wireNet: "secondary",
		required: !1
	});
	else if (r === 4) {
		let e = [
			42,
			84,
			126,
			168
		];
		for (let t = 0; t < 4; t++) n[`DIG${t + 1}`] = Object.freeze({
			relX: e[t],
			relY: 0,
			wireNet: "secondary",
			required: !1
		});
	} else if (r === 2) {
		let e = [70, 140];
		for (let t = 0; t < 2; t++) n[`DIG${t + 1}`] = Object.freeze({
			relX: e[t],
			relY: 0,
			wireNet: "secondary",
			required: !1
		});
	} else n.DIG1 = Object.freeze({
		relX: 105,
		relY: 0,
		wireNet: "secondary",
		required: !1
	});
	return Object.freeze({
		...t,
		...n
	});
}
Object.freeze({
	direct_gpio_8d: Object.freeze({
		variant: "direct_gpio_8d",
		getPins: () => {
			let e = u("seg_display", "direct_gpio_8d");
			return e.length > 0 ? e : g(_("direct_gpio_8d"));
		},
		pinsOverlay: _("direct_gpio_8d"),
		defaultAppearanceId: "seg_display_8"
	}),
	direct_gpio_4d: Object.freeze({
		variant: "direct_gpio_4d",
		getPins: () => {
			let e = u("seg_display", "direct_gpio_4d");
			return e.length > 0 ? e : g(_("direct_gpio_4d"));
		},
		pinsOverlay: _("direct_gpio_4d"),
		defaultAppearanceId: "seg_display_4"
	}),
	direct_gpio_2d: Object.freeze({
		variant: "direct_gpio_2d",
		getPins: () => {
			let e = u("seg_display", "direct_gpio_2d");
			return e.length > 0 ? e : g(_("direct_gpio_2d"));
		},
		pinsOverlay: _("direct_gpio_2d"),
		defaultAppearanceId: "seg_display_2"
	}),
	direct_gpio_1d: Object.freeze({
		variant: "direct_gpio_1d",
		getPins: () => {
			let e = u("seg_display", "direct_gpio_1d");
			return e.length > 0 ? e : g(_("direct_gpio_1d"));
		},
		pinsOverlay: _("direct_gpio_1d"),
		defaultAppearanceId: "seg_display_1"
	})
}), Object.freeze({
	seg_display_8: Object.freeze({
		appearanceId: "seg_display_8",
		variant: "direct_gpio_8d",
		displayName: "8-Digit 7-Segment Display",
		searchAliases: Object.freeze([
			"seg",
			"7seg",
			"数码管",
			"8d",
			"digital tube"
		])
	}),
	seg_display_4: Object.freeze({
		appearanceId: "seg_display_4",
		variant: "direct_gpio_4d",
		displayName: "4-Digit 7-Segment Display",
		searchAliases: Object.freeze([
			"seg",
			"7seg",
			"数码管",
			"4d",
			"digital tube"
		])
	}),
	seg_display_2: Object.freeze({
		appearanceId: "seg_display_2",
		variant: "direct_gpio_2d",
		displayName: "2-Digit 7-Segment Display",
		searchAliases: Object.freeze([
			"seg",
			"7seg",
			"数码管",
			"2d",
			"digital tube"
		])
	}),
	seg_display_1: Object.freeze({
		appearanceId: "seg_display_1",
		variant: "direct_gpio_1d",
		displayName: "1-Digit 7-Segment Display",
		searchAliases: Object.freeze([
			"seg",
			"7seg",
			"数码管",
			"1d",
			"digital tube"
		])
	})
});
//#endregion
//#region builtin/seg_display/1.0.0/src/seg-font.ts
var v = Object.freeze({
	A: 1,
	B: 2,
	C: 4,
	D: 8,
	E: 16,
	F: 32,
	G: 64,
	DP: 128
});
Object.freeze({
	0: v.A | v.B | v.C | v.D | v.E | v.F,
	1: v.B | v.C,
	2: v.A | v.B | v.D | v.E | v.G,
	3: v.A | v.B | v.C | v.D | v.G,
	4: v.B | v.C | v.F | v.G,
	5: v.A | v.C | v.D | v.F | v.G,
	6: v.A | v.C | v.D | v.E | v.F | v.G,
	7: v.A | v.B | v.C,
	8: v.A | v.B | v.C | v.D | v.E | v.F | v.G,
	9: v.A | v.B | v.C | v.D | v.F | v.G,
	A: v.A | v.B | v.C | v.E | v.F | v.G,
	a: v.A | v.B | v.C | v.E | v.F | v.G,
	B: v.C | v.D | v.E | v.F | v.G,
	b: v.C | v.D | v.E | v.F | v.G,
	C: v.A | v.D | v.E | v.F,
	c: v.D | v.E | v.G,
	D: v.B | v.C | v.D | v.E | v.G,
	d: v.B | v.C | v.D | v.E | v.G,
	E: v.A | v.D | v.E | v.F | v.G,
	e: v.A | v.D | v.E | v.F | v.G,
	F: v.A | v.E | v.F | v.G,
	f: v.A | v.E | v.F | v.G,
	H: v.B | v.C | v.E | v.F | v.G,
	h: v.C | v.E | v.F | v.G,
	L: v.D | v.E | v.F,
	l: v.D | v.E | v.F,
	n: v.C | v.E | v.G,
	N: v.A | v.B | v.C | v.E | v.F,
	O: v.A | v.B | v.C | v.D | v.E | v.F,
	o: v.C | v.D | v.E | v.G,
	P: v.A | v.B | v.E | v.F | v.G,
	p: v.A | v.B | v.E | v.F | v.G,
	r: v.E | v.G,
	R: v.E | v.G,
	t: v.D | v.E | v.F | v.G,
	T: v.D | v.E | v.F | v.G,
	U: v.B | v.C | v.D | v.E | v.F,
	u: v.C | v.D | v.E,
	"-": v.G,
	_: v.D,
	" ": 0
});
var y = /* @__PURE__ */ new Map();
y.set(0, " "), y.set(63, "0"), y.set(6, "1"), y.set(91, "2"), y.set(79, "3"), y.set(102, "4"), y.set(109, "5"), y.set(125, "6"), y.set(7, "7"), y.set(127, "8"), y.set(111, "9"), y.set(119, "A"), y.set(124, "b"), y.set(57, "C"), y.set(88, "c"), y.set(94, "d"), y.set(121, "E"), y.set(113, "F"), y.set(118, "H"), y.set(116, "h"), y.set(56, "L"), y.set(84, "n"), y.set(92, "o"), y.set(115, "P"), y.set(80, "r"), y.set(120, "t"), y.set(62, "U"), y.set(28, "u"), y.set(64, "-"), y.set(8, "_");
function b(e) {
	let t = e & 127;
	return y.get(t) ?? "?";
}
//#endregion
//#region builtin/seg_display/1.0.0/src/simulation.ts
var x = a(import.meta.url, "seg_display", "1.0.0", "display"), S = [
	"A",
	"B",
	"C",
	"D",
	"E",
	"F",
	"G",
	"DP"
];
function C(e) {
	let t = d[e] ?? 8, n = [];
	for (let e of S) n.push({
		name: e,
		pinType: "digital_in",
		role: `seg_${e.toLowerCase()}`,
		aliases: [e.toLowerCase(), `seg_${e.toLowerCase()}`],
		required: !1
	});
	for (let e = 0; e < t; e++) {
		let t = e + 1;
		n.push({
			name: `DIG${t}`,
			pinType: "digital_in",
			role: `dig_${t}`,
			aliases: [
				`dig${t}`,
				`digit${t}`,
				`com${e}`
			],
			required: !1
		});
	}
	return n;
}
function w(e = "direct_gpio_8d") {
	let t = f(e), n = C(t);
	return r({
		type: x.type,
		version: x.version,
		category: x.category,
		displayName: `${d[t]}-Digit 7-Segment Display`,
		description: "Multiplexed 7-segment digital LED display with duty-cycle brightness simulation",
		timingModel: "event-driven",
		pins: n,
		properties: {
			variant: {
				type: "string",
				default: t,
				enum: [
					"direct_gpio_8d",
					"direct_gpio_4d",
					"direct_gpio_2d",
					"direct_gpio_1d"
				]
			},
			appearanceId: {
				type: "string",
				default: `seg_display_${d[t]}`
			},
			segActiveLevel: {
				type: "string",
				default: "high",
				enum: ["high", "low"]
			},
			digitActiveLevel: {
				type: "string",
				default: "low",
				enum: ["high", "low"]
			},
			commonAnode: {
				type: "boolean",
				default: !1
			},
			color: {
				type: "string",
				default: "red",
				enum: [
					"red",
					"green",
					"blue",
					"yellow",
					"white",
					"orange",
					"purple"
				]
			},
			glow: {
				type: "boolean",
				default: !0
			},
			brightness: {
				type: "number",
				default: 1,
				min: 0,
				max: 1
			},
			label: {
				type: "string",
				default: ""
			},
			flip: {
				type: "boolean",
				default: !1
			}
		},
		stateChannels: {
			bright: {
				type: "string",
				default: ""
			},
			segMask: {
				type: "string",
				default: "[]"
			},
			text: {
				type: "string",
				default: ""
			},
			scanHz: {
				type: "number",
				default: 0
			},
			activeDigits: {
				type: "number",
				default: 0
			}
		},
		events: {}
	});
}
var T = w("direct_gpio_8d"), E = (e) => w(f(e)), D = 30000n, O = 255 / 2e3, k = 120, A = 40, j = 33000n, M = 100000n, N = class extends e {
	manifest = T;
	static manifest = T;
	nDigits = 8;
	segPinOf = /* @__PURE__ */ new Map();
	digPinOf = /* @__PURE__ */ new Map();
	segLevel = /* @__PURE__ */ new Uint8Array(8);
	digLevel = /* @__PURE__ */ new Uint8Array(8);
	bright = /* @__PURE__ */ new Uint8Array(64);
	segMask = /* @__PURE__ */ new Uint8Array(8);
	lastEdgeUs = 0n;
	tailGen = 0;
	tailPending = !1;
	staticDrive = !1;
	segActiveHigh = !0;
	digActiveHigh = !1;
	lastDig0ActiveUs = 0n;
	scanHz = 0;
	maxActiveDigitsInWindow = 0;
	lastConflictWarnUs = 0n;
	rawProperties;
	throttle = n({
		ctx: () => this.ctx,
		intervalUs: j,
		publish: (e) => this.publishFrame(e)
	});
	onBind(e, t, n) {
		this.rawProperties = n, super.onBind(e, t, n);
	}
	onBound(e, n, r) {
		let i = f(r.variant);
		this.nDigits = d[i] ?? 8;
		let a = this.rawProperties ?? {};
		this.segActiveHigh = a.segActiveLevel === void 0 ? a.commonAnode === void 0 ? r.segActiveLevel === "high" : !a.commonAnode : a.segActiveLevel === "high", this.digActiveHigh = a.digitActiveLevel === void 0 ? a.commonAnode === void 0 ? r.digitActiveLevel === "high" : !!a.commonAnode : a.digitActiveLevel === "high", this.segLevel.fill(t.HI_Z), this.digLevel = new Uint8Array(this.nDigits), this.digLevel.fill(t.HI_Z), this.bright = new Uint8Array(this.nDigits * 8), this.segMask = new Uint8Array(this.nDigits), this.segPinOf.clear(), this.digPinOf.clear();
		for (let e = 0; e < S.length; e++) {
			let t = S[e], r = n[t] ?? n[t.toLowerCase()] ?? n[`seg_${t.toLowerCase()}`];
			r !== void 0 && this.segPinOf.set(r, e);
		}
		for (let e = 0; e < this.nDigits; e++) {
			let t = `DIG${e + 1}`, r = n[t] ?? n[t.toLowerCase()] ?? n[`digit${e + 1}`] ?? n[`dig_${e + 1}`];
			r !== void 0 && this.digPinOf.set(r, e);
		}
		this.staticDrive = this.nDigits === 1 && this.digPinOf.size === 0;
		let o = this.getNowUs();
		return this.lastEdgeUs = o, this.tailGen = 0, this.tailPending = !1, this.scanHz = 0, this.maxActiveDigitsInWindow = 0, this.lastConflictWarnUs = 0n, {
			bright: this.bright,
			segMask: JSON.stringify(Array.from(this.segMask)),
			text: "".padStart(this.nDigits, " "),
			scanHz: 0,
			activeDigits: 0
		};
	}
	getNowUs() {
		let e = this.ctx;
		return typeof e?.nowUs == "function" ? e.nowUs() : typeof e?.system?.time?.nowUs == "function" ? e.system.time.nowUs() : 0n;
	}
	isDigitActive(e) {
		if (this.staticDrive && e === 0) return !0;
		let n = this.digLevel[e];
		return n === t.HI_Z || n === t.CONFLICT ? !1 : this.digActiveHigh ? n === t.HIGH : n === t.LOW;
	}
	isSegActive(e) {
		let n = this.segLevel[e];
		return n === t.HI_Z || n === t.CONFLICT ? !1 : this.segActiveHigh ? n === t.HIGH : n === t.LOW;
	}
	integrateTo(e) {
		if (e <= this.lastEdgeUs) return;
		let t = e - this.lastEdgeUs;
		t > 100000n && (t = M);
		let n = Number(t);
		if (n <= 0) {
			this.lastEdgeUs = e;
			return;
		}
		let r = Math.exp(-n / Number(D)), i = n * O, a = 0;
		for (let e = 0; e < this.nDigits; e++) this.isDigitActive(e) && a++;
		a > this.maxActiveDigitsInWindow && (this.maxActiveDigitsInWindow = a), a > 1 && e - this.lastConflictWarnUs >= 100000n && (this.lastConflictWarnUs = e, this.ctx?.system?.log?.warn?.(`[seg_display] multiple digits driven simultaneously (${a})`));
		for (let e = 0; e < this.nDigits; e++) {
			let t = this.isDigitActive(e), n = e * 8;
			for (let e = 0; e < 8; e++) {
				let a = this.isSegActive(e), o = t && a, s = n + e, c = this.bright[s];
				o && (c = Math.min(255, c + i)), c = Math.max(0, c * r), this.bright[s] = Math.round(c);
			}
		}
		this.lastEdgeUs = e;
	}
	onPinChange(e, n, r) {
		let i = typeof e == "object" && e ? e.pin : e, a = typeof e == "object" && e ? e.state : n, o = typeof e == "object" && e ? e.atUs ?? e.tUs : r, s = typeof i == "number" ? i : parseInt(String(i), 10), c = o === void 0 ? this.getNowUs() : BigInt(o);
		this.integrateTo(c);
		let l = !1, u = this.segPinOf.get(s);
		u !== void 0 && this.segLevel[u] !== a && (this.segLevel[u] = a ?? t.HI_Z, l = !0);
		let d = this.digPinOf.get(s);
		if (d !== void 0) {
			let e = this.isDigitActive(d);
			this.digLevel[d] !== a && (this.digLevel[d] = a ?? t.HI_Z, l = !0);
			let n = this.isDigitActive(d);
			if (d === 0 && n && !e) {
				if (this.lastDig0ActiveUs > 0n && c > this.lastDig0ActiveUs) {
					let e = c - this.lastDig0ActiveUs;
					e > 0n && (this.scanHz = Math.round(1e6 / Number(e)));
				}
				this.lastDig0ActiveUs = c;
			}
		}
		a === t.CONFLICT && c - this.lastConflictWarnUs >= 100000n && (this.lastConflictWarnUs = c, this.ctx?.system?.log?.warn?.(`[seg_display] bus conflict on MCU pin ${s}`)), l && this.throttle.request();
	}
	publishFrame(e = this.getNowUs()) {
		this.integrateTo(e);
		let t = "";
		for (let e = 0; e < this.nDigits; e++) {
			let n = 0, r = e * 8;
			for (let e = 0; e < 8; e++) this.bright[r + e] >= 120 && (n |= 1 << e);
			this.segMask[e] = n, t += b(n);
		}
		this.ctx && (this.ctx.publish("bright", this.bright), this.ctx.publish("segMask", JSON.stringify(Array.from(this.segMask))), this.ctx.publish("text", t), this.ctx.publish("scanHz", this.scanHz), this.ctx.publish("activeDigits", this.maxActiveDigitsInWindow)), this.maxActiveDigitsInWindow = 0;
		let n = !1;
		for (let e = 0; e < this.bright.length; e++) if (this.bright[e] > 0) {
			n = !0;
			break;
		}
		n && !this.tailPending && this.ctx && this.scheduleTail(e);
	}
	scheduleTail(e) {
		this.tailPending = !0;
		let t = ++this.tailGen, n = this.ctx;
		typeof n?.deferUs == "function" ? n.deferUs(j, () => {
			if (t !== this.tailGen) return;
			this.tailPending = !1;
			let n = this.getNowUs(), r = n > e ? n : e + j;
			this.publishFrame(r);
		}) : this.tailPending = !1;
	}
	onReset() {
		this.segLevel.fill(t.HI_Z), this.digLevel.fill(t.HI_Z), this.bright.fill(0), this.segMask.fill(0), this.throttle.reset(), this.tailGen++, this.tailPending = !1, this.scanHz = 0, this.maxActiveDigitsInWindow = 0, this.lastEdgeUs = this.getNowUs(), this.publishFrame(this.lastEdgeUs);
	}
	serializeState() {
		return {
			bright: Array.from(this.bright),
			segLevel: Array.from(this.segLevel),
			digLevel: Array.from(this.digLevel),
			segMask: Array.from(this.segMask),
			staticDrive: this.staticDrive,
			segActiveHigh: this.segActiveHigh,
			digActiveHigh: this.digActiveHigh
		};
	}
	deserializeState(e) {
		Array.isArray(e.bright) && this.bright.set(e.bright), Array.isArray(e.segLevel) && this.segLevel.set(e.segLevel), Array.isArray(e.digLevel) && this.digLevel.set(e.digLevel), Array.isArray(e.segMask) && this.segMask.set(e.segMask), typeof e.staticDrive == "boolean" && (this.staticDrive = e.staticDrive), typeof e.segActiveHigh == "boolean" && (this.segActiveHigh = e.segActiveHigh), typeof e.digActiveHigh == "boolean" && (this.digActiveHigh = e.digActiveHigh), this.lastEdgeUs = this.getNowUs();
	}
	onPropertyChange(e, t, n) {
		if (e === "variant") {
			this.ctx?.system?.log?.warn?.("[seg_display] runtime variant change is not supported (pin sets are static)");
			return;
		}
		if (e === "segActiveLevel") this.segActiveHigh = n === "high";
		else if (e === "digitActiveLevel") this.digActiveHigh = n === "high";
		else if (e === "commonAnode") {
			let e = !!n;
			this.segActiveHigh = !e, this.digActiveHigh = e;
		}
	}
	async onPowerOn(e) {
		this.onReset();
	}
	onPowerOff() {
		this.bright.fill(0), this.segMask.fill(0), this.publishFrame(this.getNowUs());
	}
	onDestroy() {
		this.throttle.reset(), this.tailGen++, this.tailPending = !1, super.onDestroy();
	}
}, P = {
	manifest: T,
	manifestFactory: E,
	PluginClass: N
};
//#endregion
export { O as CHARGE_RATE, D as DECAY_TAU_US, A as GHOST_MAX_BRIGHT, k as LOGIC_THRESHOLD, M as MAX_DT_US, j as PUBLISH_INTERVAL_US, N as SegDisplayPlugin, w as createSegDisplayManifest, C as createSegDisplayPins, P as default, T as segDisplayManifest, E as segDisplayManifestFactory };
