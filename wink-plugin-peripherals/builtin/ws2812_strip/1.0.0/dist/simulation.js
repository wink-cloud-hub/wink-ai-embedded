import { BaseSimulationPlugin as e, normalizeManifest as t, resolvePluginIdentity as n } from "@wink-ai/unisim-sdk";
//#region src/simulation.ts
var r = n(import.meta.url, "ws2812_strip", "1.0.0", "generic");
function i() {
	return t({
		type: r.type,
		version: r.version,
		category: r.category,
		displayName: "WS2812 RGB LED Strip",
		description: "Addressable RGB LED strip powered by WS2812 protocol",
		timingModel: "event-driven",
		pins: [
			{
				name: "DIN",
				pinType: "digital_in",
				role: "data",
				aliases: [
					"data",
					"din",
					"gpio"
				],
				required: !0
			},
			{
				name: "VCC",
				pinType: "vcc",
				role: "power",
				aliases: ["5v", "vcc"],
				required: !1
			},
			{
				name: "GND",
				pinType: "gnd",
				role: "ground",
				aliases: ["gnd"],
				required: !1
			}
		],
		properties: {
			numLeds: {
				type: "number",
				default: 8,
				min: 1,
				max: 256
			},
			maxFps: {
				type: "number",
				default: 30,
				min: 1,
				max: 120
			}
		},
		stateChannels: { pixels: {
			type: "string",
			default: "[]",
			description: "JSON array of RGB hex colors"
		} },
		events: {}
	});
}
var a = i(), o = () => i(), s = class extends e {
	get type() {
		return r.type;
	}
	manifest = a;
	static manifest = a;
	_latestFrame = null;
	_frameScheduled = !1;
	onBound(e, t, n) {
		let r = t.DIN ?? t.din ?? t.data;
		r !== void 0 && e && typeof e.registerWs2812Sink == "function" && e.registerWs2812Sink(r, (e) => {
			this.onWs2812Frame(e);
		});
	}
	onWs2812Frame(e) {
		this._latestFrame = e, this._frameScheduled || (this._frameScheduled = !0, queueMicrotask(() => {
			this._frameScheduled = !1, this._latestFrame && this.flushFrame(this._latestFrame);
		}));
	}
	flushFrame(e) {
		let t = Number(this.properties?.numLeds ?? 8), n = [];
		for (let r = 0; r < t; r++) {
			let t = e[r * 3 + 0] ?? 0, i = e[r * 3 + 1] ?? 0, a = e[r * 3 + 2] ?? 0;
			n.push(`#${t.toString(16).padStart(2, "0")}${i.toString(16).padStart(2, "0")}${a.toString(16).padStart(2, "0")}`);
		}
		this.ctx && typeof this.ctx.publish == "function" && this.ctx.publish("pixels", JSON.stringify(n));
	}
}, c = {
	manifest: a,
	manifestFactory: o,
	PluginClass: s
};
//#endregion
export { s as Ws2812StripPlugin, i as createWs2812Manifest, c as default, a as ws2812Manifest, o as ws2812ManifestFactory };
