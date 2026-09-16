import { definePeripheral as e, resolvePluginInstanceId as t } from "@wink-ai/unisim-ui";
import { resolvePluginIdentity as n } from "@wink-ai/unisim";
import { computed as r, createElementBlock as i, createElementVNode as a, defineComponent as o, normalizeStyle as s, openBlock as c, ref as l, toDisplayString as u, watch as d } from "vue";
import "@wokwi/elements";
//#region builtin/ntc/1.0.0/src/CanvasGlyph.vue?vue&type=script&setup=true&lang.ts
var f = { class: "ntc-glyph" }, p = ["title"], m = /*@__PURE__*/ o({
	__name: "CanvasGlyph",
	props: {
		temperature: { default: 25 },
		label: { default: "" }
	},
	setup(e) {
		let t = e, n = r(() => Number.isFinite(Number(t.temperature)) ? Number(t.temperature) : 25), o = r(() => `${n.value.toFixed(1)}°C`), l = r(() => {
			let e = (Math.min(120, Math.max(-20, n.value)) + 20) / 140;
			return `hsl(${Math.round(210 - e * 210)}, 85%, 55%)`;
		});
		return (t, n) => (c(), i("div", f, [n[0] ||= a("wokwi-ntc-temperature-sensor", null, null, -1), a("span", {
			class: "ntc-badge",
			style: s({
				borderColor: l.value,
				color: l.value
			}),
			title: e.label || "NTC"
		}, u(o.value), 13, p)]));
	}
}), h = (e, t) => {
	let n = e.__vccOpts || e;
	for (let [e, r] of t) n[e] = r;
	return n;
}, g = /*#__PURE__*/ h(m, [["__scopeId", "data-v-7b54d211"]]), _ = { class: "ntc-widget" }, v = { class: "temp-control" }, y = { class: "temp-label" }, b = [
	"min",
	"max",
	"value"
], x = /*#__PURE__*/ h(/* @__PURE__ */ o({
	__name: "WorldWidget",
	props: {
		pluginInstanceId: {},
		pinConnections: {},
		temperature: { default: 25 },
		minTemp: { default: -20 },
		maxTemp: { default: 120 }
	},
	emits: ["update:temperature", "prop-change"],
	setup(e, { emit: t }) {
		let n = e, r = t, o = l(n.temperature);
		d(() => n.temperature, (e) => {
			e !== void 0 && e !== o.value && (o.value = e);
		});
		function s(e) {
			let t = e.target, n = Number(t.value);
			o.value = n, r("update:temperature", n), r("prop-change", "temperature", n);
		}
		return (t, n) => (c(), i("div", _, [n[0] ||= a("wokwi-ntc-temperature-sensor", null, null, -1), a("div", v, [a("div", y, u(o.value) + " °C", 1), a("input", {
			type: "range",
			class: "temp-slider",
			min: e.minTemp,
			max: e.maxTemp,
			step: "1",
			value: o.value,
			onInput: s
		}, null, 40, b)])]));
	}
}), [["__scopeId", "data-v-699e702e"]]), S = Object.freeze({
	width: 135,
	height: 72
}), C = Object.freeze({
	GND: Object.freeze({
		relX: 135,
		relY: 26,
		wireNet: "gnd",
		defaultConnection: "GND",
		required: !1
	}),
	VCC: Object.freeze({
		relX: 135,
		relY: 36,
		wireNet: "vcc",
		defaultConnection: "VCC",
		required: !1
	}),
	OUT: Object.freeze({
		relX: 135,
		relY: 46,
		wireNet: "primary",
		defaultConnection: null,
		required: !0
	})
}), w = S, T = Object.freeze([
	{
		name: "OUT",
		direction: "source",
		signal: "analog",
		catalogType: "analog",
		simRole: "signal",
		aliases: [
			"out",
			"AO",
			"ao",
			"sig",
			"SIG"
		],
		required: !0
	},
	{
		name: "VCC",
		direction: "power",
		signal: "power",
		catalogType: "power",
		simRole: "vcc",
		aliases: [
			"vcc",
			"5v",
			"3v3"
		],
		required: !1
	},
	{
		name: "GND",
		direction: "ground",
		signal: "power",
		catalogType: "power",
		simRole: "gnd",
		aliases: ["gnd", "ground"],
		required: !1
	}
]);
Object.freeze({ default: Object.freeze({
	variant: "default",
	getPins: () => T,
	pinsOverlay: C,
	defaultAppearanceId: "ntc_default"
}) }), Object.freeze({ ntc_default: Object.freeze({
	appearanceId: "ntc_default",
	variant: "default",
	displayName: "NTC Temperature Sensor",
	elementTag: "wokwi-ntc-temperature-sensor",
	searchAliases: Object.freeze([
		"ntc",
		"temp",
		"temperature",
		"thermistor",
		"default"
	])
}) });
//#endregion
//#region builtin/ntc/1.0.0/src/definition.ts
var E = n(import.meta.url, "ntc", "1.0.0", "sensor"), D = e({
	type: E.type,
	catalog: {
		id: E.type,
		worldCoupling: "optional"
	},
	size: w,
	wireColor: "#38bdf8",
	pinsOverlay: C,
	props: {
		variant: {
			type: "string",
			default: "default",
			description: "Sensor variant"
		},
		temperature: {
			type: "number",
			default: 25,
			description: "Measured temperature in °C",
			range: {
				min: -40,
				max: 200,
				step: 1
			}
		},
		r25: {
			type: "number",
			default: 1e4,
			description: "Resistance at 25°C in ohms (e.g. 10000 or 100000)"
		},
		bValue: {
			type: "number",
			default: 3950,
			description: "Beta coefficient in Kelvins (e.g. 3950)"
		},
		pullUpResistor: {
			type: "number",
			default: 1e4,
			description: "Pull-up/divider resistor in ohms (e.g. 10000)"
		},
		minTemp: {
			type: "number",
			default: -20,
			description: "Min slider temperature in °C"
		},
		maxTemp: {
			type: "number",
			default: 120,
			description: "Max slider temperature in °C"
		}
	},
	canvas: g,
	world: x,
	ui: {
		canvasProps: (e, n) => {
			let r = t(e, E.type), i = n.pluginChannels?.[r]?.temperature;
			return { temperature: typeof i == "number" ? i : e.props.temperature };
		},
		worldProps: (e) => ({
			pinConnections: e.pinConnections,
			temperature: Number(e.props.temperature ?? 25),
			minTemp: Number(e.props.minTemp ?? -20),
			maxTemp: Number(e.props.maxTemp ?? 120),
			...e.props.pluginInstanceId ? { pluginInstanceId: e.props.pluginInstanceId } : {}
		})
	}
});
//#endregion
export { D as default, D as ntcDefinition };
