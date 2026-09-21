import { BaseSimulationPlugin as e, LogicStates as t, createThrottlePublish as n, normalizeManifest as r, normalizeVariantKey as i, resolvePluginIdentity as a } from "@wink-ai/unisim-sdk";
//#region ../../wink-ai/packages/unisim-ui/dist/index.js
var o = 0;
function s() {
	return o;
}
var c = /* @__PURE__ */ new Map();
function l(e) {
	return e.catalogType ? e.catalogType : e.direction === "power" || e.direction === "ground" ? "power" : e.busGroup?.startsWith("i2c") ? "i2c" : e.simRole === "pwm" || e.signal === "analog" && e.direction === "source" ? "pwm" : "gpio";
}
function u(e) {
	let t = {
		name: e.name,
		direction: e.direction,
		signal: e.signal,
		catalogType: l(e)
	};
	return e.simRole && (t.simRole = e.simRole), e.description && (t.description = e.description), e.busGroup && (t.busGroup = e.busGroup), e.aliases?.length && (t.aliases = [...e.aliases]), e.required !== void 0 && (t.required = e.required), e.voltage && (t.voltage = e.voltage), t;
}
function d(e, t, n) {
	if (n) return Object.freeze((n.pins ?? []).map(u));
	let r = `${e}:${t ?? ""}:${s()}`, i = c.get(r);
	if (i) return i;
	let a = [].map(u), o = Object.freeze(a);
	return c.set(r, o), o;
}
Object.freeze({
	width: 210,
	height: 96
});
var f = Object.freeze({
	A: Object.freeze({
		relX: 23,
		relY: 96,
		wireNet: "primary",
		defaultConnection: null,
		required: !1
	}),
	B: Object.freeze({
		relX: 47,
		relY: 96,
		wireNet: "primary",
		defaultConnection: null,
		required: !1
	}),
	C: Object.freeze({
		relX: 70,
		relY: 96,
		wireNet: "primary",
		defaultConnection: null,
		required: !1
	}),
	D: Object.freeze({
		relX: 93,
		relY: 96,
		wireNet: "primary",
		defaultConnection: null,
		required: !1
	}),
	E: Object.freeze({
		relX: 117,
		relY: 96,
		wireNet: "primary",
		defaultConnection: null,
		required: !1
	}),
	F: Object.freeze({
		relX: 140,
		relY: 96,
		wireNet: "primary",
		defaultConnection: null,
		required: !1
	}),
	G: Object.freeze({
		relX: 163,
		relY: 96,
		wireNet: "primary",
		defaultConnection: null,
		required: !1
	}),
	DP: Object.freeze({
		relX: 187,
		relY: 96,
		wireNet: "primary",
		defaultConnection: null,
		required: !1
	})
});
Object.freeze([
	"direct_gpio_8d",
	"direct_gpio_4d",
	"direct_gpio_2d",
	"direct_gpio_1d"
]);
var p = Object.freeze({
	direct_gpio_8d: 8,
	direct_gpio_4d: 4,
	direct_gpio_2d: 2,
	direct_gpio_1d: 1
});
function m(e) {
	let t = i(e);
	return t && t in p ? t : "direct_gpio_8d";
}
Object.freeze({
	"8d": Object.freeze(["direct_gpio_8d"]),
	"4d": Object.freeze(["direct_gpio_4d"]),
	"2d": Object.freeze(["direct_gpio_2d"]),
	"1d": Object.freeze(["direct_gpio_1d"])
});
var h = Object.freeze([
	"A",
	"B",
	"C",
	"D",
	"E",
	"F",
	"G",
	"DP"
].map((e) => f[e]?.relX ?? 0));
function g() {
	return f;
}
function _(e) {
	return Object.freeze(Object.entries(e).map(([e]) => ({
		name: e,
		direction: "sink",
		signal: "digital",
		catalogType: "gpio",
		required: !1
	})));
}
function v(e) {
	let t = g(), n = {}, r = p[e];
	if (r === 8) for (let e = 0; e < 8; e++) n[`DIG${e + 1}`] = Object.freeze({
		relX: h[e],
		relY: 0,
		wireNet: "secondary",
		required: !0
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
			required: !0
		});
	} else if (r === 2) {
		let e = [70, 140];
		for (let t = 0; t < 2; t++) n[`DIG${t + 1}`] = Object.freeze({
			relX: e[t],
			relY: 0,
			wireNet: "secondary",
			required: !0
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
			let e = d("seg_display", "direct_gpio_8d");
			return e.length > 0 ? e : _(v("direct_gpio_8d"));
		},
		pinsOverlay: v("direct_gpio_8d"),
		defaultAppearanceId: "seg_display_8"
	}),
	direct_gpio_4d: Object.freeze({
		variant: "direct_gpio_4d",
		getPins: () => {
			let e = d("seg_display", "direct_gpio_4d");
			return e.length > 0 ? e : _(v("direct_gpio_4d"));
		},
		pinsOverlay: v("direct_gpio_4d"),
		defaultAppearanceId: "seg_display_4"
	}),
	direct_gpio_2d: Object.freeze({
		variant: "direct_gpio_2d",
		getPins: () => {
			let e = d("seg_display", "direct_gpio_2d");
			return e.length > 0 ? e : _(v("direct_gpio_2d"));
		},
		pinsOverlay: v("direct_gpio_2d"),
		defaultAppearanceId: "seg_display_2"
	}),
	direct_gpio_1d: Object.freeze({
		variant: "direct_gpio_1d",
		getPins: () => {
			let e = d("seg_display", "direct_gpio_1d");
			return e.length > 0 ? e : _(v("direct_gpio_1d"));
		},
		pinsOverlay: v("direct_gpio_1d"),
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
var y = Object.freeze({
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
	0: y.A | y.B | y.C | y.D | y.E | y.F,
	1: y.B | y.C,
	2: y.A | y.B | y.D | y.E | y.G,
	3: y.A | y.B | y.C | y.D | y.G,
	4: y.B | y.C | y.F | y.G,
	5: y.A | y.C | y.D | y.F | y.G,
	6: y.A | y.C | y.D | y.E | y.F | y.G,
	7: y.A | y.B | y.C,
	8: y.A | y.B | y.C | y.D | y.E | y.F | y.G,
	9: y.A | y.B | y.C | y.D | y.F | y.G,
	A: y.A | y.B | y.C | y.E | y.F | y.G,
	a: y.A | y.B | y.C | y.E | y.F | y.G,
	B: y.C | y.D | y.E | y.F | y.G,
	b: y.C | y.D | y.E | y.F | y.G,
	C: y.A | y.D | y.E | y.F,
	c: y.D | y.E | y.G,
	D: y.B | y.C | y.D | y.E | y.G,
	d: y.B | y.C | y.D | y.E | y.G,
	E: y.A | y.D | y.E | y.F | y.G,
	e: y.A | y.D | y.E | y.F | y.G,
	F: y.A | y.E | y.F | y.G,
	f: y.A | y.E | y.F | y.G,
	H: y.B | y.C | y.E | y.F | y.G,
	h: y.C | y.E | y.F | y.G,
	L: y.D | y.E | y.F,
	l: y.D | y.E | y.F,
	n: y.C | y.E | y.G,
	N: y.A | y.B | y.C | y.E | y.F,
	O: y.A | y.B | y.C | y.D | y.E | y.F,
	o: y.C | y.D | y.E | y.G,
	P: y.A | y.B | y.E | y.F | y.G,
	p: y.A | y.B | y.E | y.F | y.G,
	r: y.E | y.G,
	R: y.E | y.G,
	t: y.D | y.E | y.F | y.G,
	T: y.D | y.E | y.F | y.G,
	U: y.B | y.C | y.D | y.E | y.F,
	u: y.C | y.D | y.E,
	"-": y.G,
	_: y.D,
	" ": 0
});
var b = /* @__PURE__ */ new Map();
b.set(0, " "), b.set(63, "0"), b.set(6, "1"), b.set(91, "2"), b.set(79, "3"), b.set(102, "4"), b.set(109, "5"), b.set(125, "6"), b.set(7, "7"), b.set(127, "8"), b.set(111, "9"), b.set(119, "A"), b.set(124, "b"), b.set(57, "C"), b.set(88, "c"), b.set(94, "d"), b.set(121, "E"), b.set(113, "F"), b.set(118, "H"), b.set(116, "h"), b.set(56, "L"), b.set(84, "n"), b.set(92, "o"), b.set(115, "P"), b.set(80, "r"), b.set(120, "t"), b.set(62, "U"), b.set(28, "u"), b.set(64, "-"), b.set(8, "_");
function x(e) {
	let t = e & 127;
	return b.get(t) ?? "?";
}
//#endregion
//#region builtin/seg_display/1.0.0/src/simulation.ts
var S = a(import.meta.url, "seg_display", "1.0.0", "display"), C = [
	"A",
	"B",
	"C",
	"D",
	"E",
	"F",
	"G",
	"DP"
];
function w(e) {
	let t = p[e] ?? 8, n = [];
	for (let e of C) n.push({
		name: e,
		pinType: "digital_in",
		role: `seg_${e.toLowerCase()}`,
		aliases: [e.toLowerCase(), `seg_${e.toLowerCase()}`],
		required: !1
	});
	let r = t > 1;
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
			required: r
		});
	}
	return n;
}
function T(e = "direct_gpio_8d") {
	let t = m(e), n = w(t);
	return r({
		type: S.type,
		version: S.version,
		category: S.category,
		displayName: `${p[t]}-Digit 7-Segment Display`,
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
				default: `seg_display_${p[t]}`
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
			},
			deadbandCheck: {
				type: "boolean",
				default: !0
			},
			decayTauUs: {
				type: "number",
				default: 8e4,
				min: 1e3,
				max: 5e5
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
				default: 0,
				description: "Display frame refresh rate in Hz (full cycle of all active digits)"
			},
			activeDigits: {
				type: "number",
				default: 0
			},
			ghostingDetected: {
				type: "boolean",
				default: !1,
				description: "Flag indicating whether visual ghosting was detected due to unblanked segment change"
			},
			deadbandViolations: {
				type: "number",
				default: 0,
				description: "Cumulative count of deadband and commutation timing violations"
			}
		},
		events: {}
	});
}
var E = T("direct_gpio_8d"), D = (e) => T(m(e)), O = 80000n, k = 255 / 2e3, A = 50, j = 40, M = 16000n, N = 100000n, P = class extends e {
	manifest = E;
	static manifest = E;
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
	dig0HistoryUs = [];
	scanHz = 0;
	maxActiveDigitsInWindow = 0;
	lastConflictWarnUs = 0n;
	rawProperties;
	deadbandCheck = !0;
	decayTauUs = O;
	ghostingDetected = !1;
	deadbandViolations = 0;
	lastActiveDigitIndex = -1;
	blankedAtUs = -1n;
	blankedAtNs;
	throttle = n({
		ctx: () => this.ctx,
		intervalUs: M,
		publish: (e) => this.publishFrame(e)
	});
	onBind(e, t, n) {
		this.rawProperties = n, super.onBind(e, t, n);
	}
	onBound(e, n, r) {
		let i = m(r.variant);
		this.nDigits = p[i] ?? 8;
		let a = this.rawProperties ?? {};
		this.segActiveHigh = a.segActiveLevel === void 0 ? a.commonAnode === void 0 ? r.segActiveLevel === "high" : !a.commonAnode : a.segActiveLevel === "high", this.digActiveHigh = a.digitActiveLevel === void 0 ? a.commonAnode === void 0 ? r.digitActiveLevel === "high" : !!a.commonAnode : a.digitActiveLevel === "high", this.segLevel.fill(t.HI_Z), this.digLevel = new Uint8Array(this.nDigits), this.digLevel.fill(t.HI_Z), this.bright = new Uint8Array(this.nDigits * 8), this.segMask = new Uint8Array(this.nDigits), this.segPinOf.clear(), this.digPinOf.clear();
		for (let e = 0; e < C.length; e++) {
			let t = C[e], r = n[t] ?? n[t.toLowerCase()] ?? n[`seg_${t.toLowerCase()}`];
			r !== void 0 && this.segPinOf.set(r, e);
		}
		for (let e = 0; e < this.nDigits; e++) {
			let t = `DIG${e + 1}`, r = n[t] ?? n[t.toLowerCase()] ?? n[`digit${e + 1}`] ?? n[`dig_${e + 1}`];
			r !== void 0 && this.digPinOf.set(r, e);
		}
		this.staticDrive = this.nDigits === 1 && this.digPinOf.size === 0, this.deadbandCheck = a.deadbandCheck === void 0 ? r.deadbandCheck === void 0 || !!r.deadbandCheck : !!a.deadbandCheck;
		let o = a.decayTauUs ?? r.decayTauUs;
		this.decayTauUs = typeof o == "number" && o > 0 ? BigInt(Math.round(o)) : O, this.ghostingDetected = !1, this.deadbandViolations = 0, this.lastActiveDigitIndex = -1, this.blankedAtUs = -1n, this.blankedAtNs = void 0;
		let s = this.getNowUs();
		return this.lastEdgeUs = s, this.tailGen = 0, this.tailPending = !1, this.scanHz = 0, this.lastDig0ActiveUs = 0n, this.dig0HistoryUs = [], this.maxActiveDigitsInWindow = 0, this.lastConflictWarnUs = -100000n, {
			bright: this.bright,
			segMask: JSON.stringify(Array.from(this.segMask)),
			text: "".padStart(this.nDigits, " "),
			scanHz: 0,
			activeDigits: 0,
			ghostingDetected: !1,
			deadbandViolations: 0
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
	getActiveDigitsCount() {
		let e = 0;
		for (let t = 0; t < this.nDigits; t++) this.isDigitActive(t) && e++;
		return e;
	}
	integrateTo(e) {
		let t = this.getActiveDigitsCount();
		if (t > this.maxActiveDigitsInWindow && (this.maxActiveDigitsInWindow = t), t > 1 && e - this.lastConflictWarnUs >= 100000n && (this.lastConflictWarnUs = e, this.ctx?.system?.log?.warn?.(`[seg_display] multiple digits driven simultaneously (${t})`)), e <= this.lastEdgeUs) return;
		let n = e - this.lastEdgeUs;
		n > 100000n && (n = N);
		let r = Number(n);
		if (r <= 0) {
			this.lastEdgeUs = e;
			return;
		}
		let i = Math.exp(-r / Number(this.decayTauUs)), a = r * k;
		for (let e = 0; e < this.nDigits; e++) {
			let t = this.isDigitActive(e), n = e * 8;
			for (let e = 0; e < 8; e++) {
				let r = this.isSegActive(e), o = t && r, s = n + e, c = this.bright[s];
				if (o) c = Math.min(255, c + a), this.bright[s] = Math.round(c);
				else {
					c = Math.max(0, c * i);
					let e = Math.round(c);
					e >= this.bright[s] && this.bright[s] > 0 && (e = this.bright[s] - 1), this.bright[s] = e;
				}
			}
		}
		this.lastEdgeUs = e;
	}
	onPinChange(e, n, r) {
		let i = typeof e == "object" && e ? e.pin : e, a = typeof e == "object" && e ? e.state : n, o = typeof e == "object" && e ? e.atUs ?? e.tUs : r, s = typeof e == "object" && e ? e.atNs ?? e.tNs : void 0, c = typeof i == "number" ? i : parseInt(String(i), 10), l = o === void 0 ? this.getNowUs() : BigInt(o), u = s === void 0 ? void 0 : BigInt(s);
		this.integrateTo(l);
		let d = !1, f = this.segPinOf.get(c);
		if (f !== void 0 && this.segLevel[f] !== a) {
			if (this.deadbandCheck && !this.staticDrive) {
				let e = this.getActiveDigitsCount();
				e > 0 && (this.ghostingDetected = !0, this.deadbandViolations++, this.ctx?.publish?.("ghostingDetected", !0), this.ctx?.publish?.("deadbandViolations", this.deadbandViolations), this.ctx?.system?.log?.warn?.(`[seg_display] ghosting detected: segment ${C[f]} changed while digit COM active (${e} active)`));
			}
			this.segLevel[f] = a ?? t.HI_Z, d = !0;
		}
		let p = this.digPinOf.get(c);
		if (p !== void 0) {
			let e = this.isDigitActive(p);
			this.digLevel[p] !== a && (this.digLevel[p] = a ?? t.HI_Z, d = !0);
			let n = this.isDigitActive(p);
			if (e && !n) this.getActiveDigitsCount() === 0 && (this.lastActiveDigitIndex = p, this.blankedAtUs = l, this.blankedAtNs = u);
			else if (!e && n && this.deadbandCheck && !this.staticDrive) {
				let e = 0;
				for (let t = 0; t < this.nDigits; t++) t !== p && this.isDigitActive(t) && e++;
				if (e > 0 && (this.deadbandViolations++, this.ctx?.publish?.("deadbandViolations", this.deadbandViolations), this.ctx?.system?.log?.warn?.(`[seg_display] deadband violation: digit ${p + 1} activated while ${e} other digit(s) active`)), u !== void 0 && this.blankedAtNs !== void 0 && this.lastActiveDigitIndex !== -1 && this.lastActiveDigitIndex !== p) {
					let e = u - this.blankedAtNs;
					e >= 0n && e < 100n && (this.deadbandViolations++, this.ctx?.publish?.("deadbandViolations", this.deadbandViolations), this.ctx?.system?.log?.warn?.(`[seg_display] deadband violation: switching deadband between DIG${this.lastActiveDigitIndex + 1} and DIG${p + 1} too short (${e}ns < 100ns)`));
				}
			}
			if (p === 0 && n && !e) {
				if (this.dig0HistoryUs.length > 0 && l - this.dig0HistoryUs[this.dig0HistoryUs.length - 1] > 500000n && (this.dig0HistoryUs = []), this.dig0HistoryUs.push(l), this.dig0HistoryUs.length > 3 && this.dig0HistoryUs.shift(), this.dig0HistoryUs.length >= 2) {
					let e = this.dig0HistoryUs.length - 1, t = l - this.dig0HistoryUs[0];
					if (t > 0n) {
						let n = t / BigInt(e);
						n > 0n && (this.scanHz = Math.round(1e6 / Number(n)), this.ctx?.publish?.("scanHz", this.scanHz));
					}
				}
				this.lastDig0ActiveUs = l;
			}
		}
		a === t.CONFLICT && l - this.lastConflictWarnUs >= 100000n && (this.lastConflictWarnUs = l, this.ctx?.system?.log?.warn?.(`[seg_display] bus conflict on MCU pin ${c}`)), d && this.throttle.request();
	}
	publishFrame(e = this.getNowUs()) {
		this.integrateTo(e);
		let t = this.getActiveDigitsCount(), n = Math.max(this.maxActiveDigitsInWindow, t), r = "";
		for (let e = 0; e < this.nDigits; e++) {
			let t = 0, n = e * 8;
			for (let e = 0; e < 8; e++) this.bright[n + e] >= 50 && (t |= 1 << e);
			this.segMask[e] = t, r += x(t);
		}
		this.ctx && (this.ctx.publish("bright", this.bright), this.ctx.publish("segMask", JSON.stringify(Array.from(this.segMask))), this.ctx.publish("text", r), this.ctx.publish("scanHz", this.scanHz), this.ctx.publish("activeDigits", n), this.ctx.publish("ghostingDetected", this.ghostingDetected), this.ctx.publish("deadbandViolations", this.deadbandViolations)), this.maxActiveDigitsInWindow = t;
		let i = !1;
		for (let e = 0; e < this.bright.length; e++) if (this.bright[e] > 0) {
			i = !0;
			break;
		}
		i && !this.tailPending && !this.throttle.isPending() && this.ctx && this.scheduleTail(e);
	}
	scheduleTail(e) {
		if (this.tailPending || this.throttle.isPending()) return;
		this.tailPending = !0;
		let t = ++this.tailGen, n = this.ctx;
		typeof n?.deferUs == "function" ? n.deferUs(M, () => {
			if (t !== this.tailGen) return;
			this.tailPending = !1;
			let n = this.getNowUs(), r = n > e ? n : e + M;
			this.publishFrame(r);
		}) : this.tailPending = !1;
	}
	onReset() {
		this.segLevel.fill(t.HI_Z), this.digLevel.fill(t.HI_Z), this.bright.fill(0), this.segMask.fill(0), this.throttle.reset(), this.tailGen++, this.tailPending = !1, this.scanHz = 0, this.lastDig0ActiveUs = 0n, this.dig0HistoryUs = [], this.maxActiveDigitsInWindow = 0, this.ghostingDetected = !1, this.deadbandViolations = 0, this.lastActiveDigitIndex = -1, this.blankedAtUs = -1n, this.blankedAtNs = void 0, this.lastEdgeUs = this.getNowUs(), this.publishFrame(this.lastEdgeUs);
	}
	serializeState() {
		return {
			bright: Array.from(this.bright),
			segLevel: Array.from(this.segLevel),
			digLevel: Array.from(this.digLevel),
			segMask: Array.from(this.segMask),
			staticDrive: this.staticDrive,
			segActiveHigh: this.segActiveHigh,
			digActiveHigh: this.digActiveHigh,
			ghostingDetected: this.ghostingDetected,
			deadbandViolations: this.deadbandViolations,
			decayTauUs: Number(this.decayTauUs),
			deadbandCheck: this.deadbandCheck
		};
	}
	deserializeState(e) {
		Array.isArray(e.bright) && this.bright.set(e.bright), Array.isArray(e.segLevel) && this.segLevel.set(e.segLevel), Array.isArray(e.digLevel) && this.digLevel.set(e.digLevel), Array.isArray(e.segMask) && this.segMask.set(e.segMask), typeof e.staticDrive == "boolean" && (this.staticDrive = e.staticDrive), typeof e.segActiveHigh == "boolean" && (this.segActiveHigh = e.segActiveHigh), typeof e.digActiveHigh == "boolean" && (this.digActiveHigh = e.digActiveHigh), typeof e.ghostingDetected == "boolean" && (this.ghostingDetected = e.ghostingDetected), typeof e.deadbandViolations == "number" && (this.deadbandViolations = e.deadbandViolations), typeof e.decayTauUs == "number" && e.decayTauUs > 0 && (this.decayTauUs = BigInt(Math.round(e.decayTauUs))), typeof e.deadbandCheck == "boolean" && (this.deadbandCheck = e.deadbandCheck), this.lastEdgeUs = this.getNowUs();
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
		} else if (e === "deadbandCheck") this.deadbandCheck = !!n;
		else if (e === "decayTauUs") {
			let e = Number(n);
			Number.isFinite(e) && e > 0 && (this.decayTauUs = BigInt(Math.round(e)));
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
}, F = {
	manifest: E,
	manifestFactory: D,
	PluginClass: P
};
//#endregion
export { k as CHARGE_RATE, O as DECAY_TAU_US, j as GHOST_MAX_BRIGHT, A as LOGIC_THRESHOLD, N as MAX_DT_US, M as PUBLISH_INTERVAL_US, P as SegDisplayPlugin, T as createSegDisplayManifest, w as createSegDisplayPins, F as default, E as segDisplayManifest, D as segDisplayManifestFactory };
