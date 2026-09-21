import { definePeripheral as e, resolvePluginInstanceId as t } from "@wink-ai/unisim-ui";
import { resolvePluginIdentity as n } from "@wink-ai/unisim-sdk";
import { createElementBlock as r, createElementVNode as i, defineComponent as a, openBlock as o, ref as s, toDisplayString as c, watch as l } from "vue";
import "@wokwi/elements";
//#endregion
//#region builtin/ntc/1.0.0/src/CanvasGlyph.vue
var u = /* @__PURE__ */ a({
	__name: "CanvasGlyph",
	setup(e) {
		return (e, t) => (o(), r("wokwi-ntc-temperature-sensor"));
	}
}), d = { class: "ntc-widget" }, f = { class: "temp-control" }, p = { class: "temp-label" }, m = [
	"min",
	"max",
	"value"
], h = /*#__PURE__*/ ((e, t) => {
	let n = e.__vccOpts || e;
	for (let [e, r] of t) n[e] = r;
	return n;
})(/* @__PURE__ */ a({
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
		let n = e, a = t, u = s(n.temperature);
		l(() => n.temperature, (e) => {
			e !== void 0 && e !== u.value && (u.value = e);
		});
		function h(e) {
			let t = e.target, n = Number(t.value);
			u.value = n, a("update:temperature", n), a("prop-change", "temperature", n);
		}
		return (t, n) => (o(), r("div", d, [n[0] ||= i("wokwi-ntc-temperature-sensor", null, null, -1), i("div", f, [i("div", p, c(u.value) + " °C", 1), i("input", {
			type: "range",
			class: "temp-slider",
			min: e.minTemp,
			max: e.maxTemp,
			step: "1",
			value: u.value,
			onInput: h
		}, null, 40, m)])]));
	}
}), [["__scopeId", "data-v-699e702e"]]), g = Object.freeze({
	width: 135,
	height: 72
}), _ = Object.freeze({
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
}), v = g, y = Object.freeze([
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
	getPins: () => y,
	pinsOverlay: _,
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
var b = n(import.meta.url, "ntc", "1.0.0", "sensor"), x = e({
	type: b.type,
	catalog: {
		id: b.type,
		worldCoupling: "optional"
	},
	size: v,
	wireColor: "#38bdf8",
	pinsOverlay: _,
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
		},
		vref: {
			type: "number",
			default: 3,
			description: "Nominal reference voltage in volts"
		},
		vrefNoisePct: {
			type: "number",
			default: 0,
			description: "Vref random noise percentage (e.g. 0.1 for +/-0.1%)"
		},
		lineRegulationPct: {
			type: "number",
			default: 0,
			description: "LDO line regulation drift percentage"
		},
		tempDriftPpm: {
			type: "number",
			default: 0,
			description: "LDO temperature coefficient in ppm/degC"
		},
		gndBouncePct: {
			type: "number",
			default: 0,
			description: "Ground bounce perturbation percentage"
		},
		heaterActive: {
			type: "boolean",
			default: !1,
			description: "Heater active state causing ground bounce"
		}
	},
	canvas: u,
	world: h,
	ui: {
		canvasBadge: (e, n) => {
			let r = t(e, b.type), i = n.pluginChannels?.[r]?.temperature, a = typeof i == "number" ? i : Number(e.props.temperature ?? 25);
			if (!Number.isFinite(a)) return null;
			let o = Math.round(210 - (Math.min(120, Math.max(-20, a)) + 20) / 140 * 210);
			return {
				text: `${a.toFixed(1)}°C`,
				title: "NTC",
				accent: `hsl(${o}, 85%, 60%)`
			};
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
export { x as default, x as ntcDefinition };
