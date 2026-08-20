import { SimpleGpioPlugin as e, normalizeManifest as t, normalizeVariantKey as n, resolvePluginIdentity as r } from "@wink-ai/unisim";
//#region builtin/led/1.0.0/src/simulation.ts
var i = { default: {
	displayName: "GPIO LED",
	pins: [{
		name: "A",
		pinType: "digital_in",
		role: "anode",
		aliases: ["anode", "gpio"],
		required: !1
	}, {
		name: "C",
		pinType: "gnd",
		role: "cathode",
		aliases: ["cathode"],
		required: !1
	}]
} };
function a(e) {
	let t = n(e);
	return t && t in i ? t : "default";
}
var o = r(import.meta.url, "led", "1.0.0", "output");
function s(e = "default") {
	let n = i[e] ?? i.default;
	return t({
		type: o.type,
		version: o.version,
		category: o.category,
		displayName: n.displayName,
		description: "Discrete LED with anode/cathode package pins",
		timingModel: "event-driven",
		pins: n.pins,
		properties: {
			variant: {
				type: "string",
				default: "default"
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
			brightness: {
				type: "number",
				default: 1,
				min: 0,
				max: 1
			},
			activeHigh: {
				type: "boolean",
				default: !0
			}
		},
		stateChannels: { on: {
			type: "boolean",
			default: !1,
			description: "Lit state"
		} },
		events: {}
	});
}
var c = s("default"), l = (e) => s(a(e)), u = class extends e {
	manifest = c;
	static manifest = c;
}, d = {
	manifest: c,
	manifestFactory: l,
	PluginClass: u
};
//#endregion
export { i as LED_PIN_VARIANTS, u as LedPlugin, s as createLedManifest, d as default, c as ledManifest, l as ledManifestFactory };
