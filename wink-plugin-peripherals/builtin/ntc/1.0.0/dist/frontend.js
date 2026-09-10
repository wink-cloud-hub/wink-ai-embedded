import { definePeripheral as e } from "@wink-ai/unisim-ui";
import { resolvePluginIdentity as t } from "@wink-ai/unisim";
import { createElementBlock as n, createElementVNode as r, defineComponent as i, openBlock as a, ref as o, toDisplayString as s, watch as c } from "vue";
import "@wokwi/elements";
//#endregion
//#region builtin/ntc/1.0.0/src/CanvasGlyph.vue
var l = /* @__PURE__ */ i({
	__name: "CanvasGlyph",
	setup(e) {
		return (e, t) => (a(), n("wokwi-ntc-temperature-sensor"));
	}
}), u = { class: "ntc-widget" }, d = { class: "temp-control" }, f = { class: "temp-label" }, p = [
	"min",
	"max",
	"value"
], m = /*#__PURE__*/ ((e, t) => {
	let n = e.__vccOpts || e;
	for (let [e, r] of t) n[e] = r;
	return n;
})(/* @__PURE__ */ i({
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
		let i = e, l = t, m = o(i.temperature);
		c(() => i.temperature, (e) => {
			e !== void 0 && e !== m.value && (m.value = e);
		});
		function h(e) {
			let t = e.target, n = Number(t.value);
			m.value = n, l("update:temperature", n), l("prop-change", "temperature", n);
		}
		return (t, i) => (a(), n("div", u, [i[0] ||= r("wokwi-ntc-temperature-sensor", null, null, -1), r("div", d, [r("div", f, s(m.value) + " °C", 1), r("input", {
			type: "range",
			class: "temp-slider",
			min: e.minTemp,
			max: e.maxTemp,
			step: "1",
			value: m.value,
			onInput: h
		}, null, 40, p)])]));
	}
}), [["__scopeId", "data-v-699e702e"]]), h = Object.freeze({
	width: 135,
	height: 72
}), g = Object.freeze({
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
}), _ = h, v = Object.freeze([
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
	getPins: () => v,
	pinsOverlay: g,
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
var y = t(import.meta.url, "ntc", "1.0.0", "sensor"), b = e({
	type: y.type,
	catalog: {
		id: y.type,
		worldCoupling: "optional"
	},
	size: _,
	wireColor: "#38bdf8",
	pinsOverlay: g,
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
	canvas: l,
	world: m,
	ui: { worldProps: (e) => ({
		pinConnections: e.pinConnections,
		temperature: Number(e.props.temperature ?? 25),
		minTemp: Number(e.props.minTemp ?? -20),
		maxTemp: Number(e.props.maxTemp ?? 120),
		...e.props.pluginInstanceId ? { pluginInstanceId: e.props.pluginInstanceId } : {}
	}) }
});
//#endregion
export { b as default, b as ntcDefinition };
