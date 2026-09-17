import { BaseSimulationPlugin as e, normalizeManifest as t, resolvePluginIdentity as n } from "@wink-ai/unisim-sdk";
//#region src/simulation.ts
var r = n(import.meta.url, "gps", "1.0.0", "generic");
function i(e) {
	let t = 0;
	for (let n = 0; n < e.length; n++) t ^= e.charCodeAt(n);
	return `$${e}*${t.toString(16).toUpperCase().padStart(2, "0")}\r\n`;
}
function a(e) {
	let t = e >= 0 ? "N" : "S", n = Math.abs(e), r = Math.floor(n), i = (n - r) * 60;
	return {
		val: `${r.toString().padStart(2, "0")}${i.toFixed(4).padStart(7, "0")}`,
		dir: t
	};
}
function o(e) {
	let t = e >= 0 ? "E" : "W", n = Math.abs(e), r = Math.floor(n), i = (n - r) * 60;
	return {
		val: `${r.toString().padStart(3, "0")}${i.toFixed(4).padStart(7, "0")}`,
		dir: t
	};
}
function s(e, t, n, r = 0) {
	let s = n ? "A" : "V", c = a(e), l = o(t);
	return i(`GPRMC,${`${Math.floor(r / 3600 % 24).toString().padStart(2, "0")}${Math.floor(r / 60 % 60).toString().padStart(2, "0")}${Math.floor(r % 60).toString().padStart(2, "0")}.00`},${s},${c.val},${c.dir},${l.val},${l.dir},0.0,0.0,100826,,,${s === "A" ? "A" : "N"}`);
}
function c(e = "nmea_uart") {
	return t({
		type: r.type,
		version: r.version,
		category: r.category,
		displayName: "GPS Receiver Module",
		description: "GPS receiver module (NMEA 0183 via UART / I2C)",
		timingModel: "event-driven",
		pins: [
			{
				name: "TX",
				pinType: "digital_out",
				role: "signal",
				aliases: ["tx", "out"],
				required: !0
			},
			{
				name: "RX",
				pinType: "digital_in",
				role: "signal",
				aliases: ["rx", "in"],
				required: !1
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
			baudRate: {
				type: "number",
				default: 9600,
				min: 1200,
				max: 115200
			},
			updateIntervalMs: {
				type: "number",
				default: 1e3,
				min: 100,
				max: 1e4,
				unit: "ms"
			},
			latitude: {
				type: "number",
				default: 31.2304,
				min: -90,
				max: 90,
				unit: "deg"
			},
			longitude: {
				type: "number",
				default: 121.4737,
				min: -180,
				max: 180,
				unit: "deg"
			},
			fix: {
				type: "boolean",
				default: !0
			},
			uartPort: {
				type: "number",
				default: 0
			},
			variant: {
				type: "string",
				default: e
			}
		},
		stateChannels: {
			latitude: {
				type: "number",
				default: 31.2304,
				unit: "deg"
			},
			longitude: {
				type: "number",
				default: 121.4737,
				unit: "deg"
			},
			fix: {
				type: "boolean",
				default: !0
			},
			lastSentence: {
				type: "string",
				default: ""
			}
		},
		events: { SET_LOCATION: {
			description: "Set simulated GPS coordinates and fix state",
			params: {
				latitude: {
					type: "number",
					required: !1,
					min: -90,
					max: 90
				},
				longitude: {
					type: "number",
					required: !1,
					min: -180,
					max: 180
				},
				fix: {
					type: "boolean",
					required: !1
				}
			}
		} }
	});
}
var l = c(), u = (e) => c(e || "nmea_uart"), d = class extends e {
	get type() {
		return r.type;
	}
	manifest = l;
	static manifest = l;
	_lastSentUs = -1n;
	_latitude = 31.2304;
	_longitude = 121.4737;
	_fix = !0;
	_uartPort = 0;
	_updateIntervalUs = 1000000n;
	onBound(e, t, n) {
		return this.onPropsUpdated(n), this._lastSentUs = -1n, {
			latitude: this._latitude,
			longitude: this._longitude,
			fix: this._fix,
			lastSentence: ""
		};
	}
	onDestroy() {}
	onPropsUpdated(e) {
		e.latitude !== void 0 && (this._latitude = Number(e.latitude)), e.longitude !== void 0 && (this._longitude = Number(e.longitude)), e.fix !== void 0 && (this._fix = !!e.fix), e.uartPort !== void 0 && (this._uartPort = Number(e.uartPort));
		let t = Math.max(100, Number(e.updateIntervalMs ?? 1e3));
		this._updateIntervalUs = BigInt(Math.floor(t * 1e3)), this.ctx?.publish("latitude", this._latitude), this.ctx?.publish("longitude", this._longitude), this.ctx?.publish("fix", this._fix);
	}
	onStep(e) {
		this.ctx && (this._lastSentUs < 0n || e - this._lastSentUs >= this._updateIntervalUs) && (this._lastSentUs = e, this.emitNmeaSentence(e));
	}
	emitNmeaSentence(e) {
		let t = Number(e / 1000000n), n = s(this._latitude, this._longitude, this._fix, t);
		this.ctx?.publish("lastSentence", n);
		let r = new TextEncoder().encode(n);
		this.ctx?.bus?.uart ? this.ctx.bus.uart.toMcu(this._uartPort, r) : typeof this.ctx?.writeUart == "function" && this.ctx.writeUart(this._uartPort, r);
	}
	_location(e) {
		this._SET_LOCATION(e);
	}
	_setLocation(e) {
		this._SET_LOCATION(e);
	}
	_SET_LOCATION(e) {
		e.latitude !== void 0 && (this._latitude = Number(e.latitude)), e.longitude !== void 0 && (this._longitude = Number(e.longitude)), e.fix !== void 0 && (this._fix = !!e.fix), this.ctx?.publish("latitude", this._latitude), this.ctx?.publish("longitude", this._longitude), this.ctx?.publish("fix", this._fix);
	}
}, f = {
	manifest: l,
	manifestFactory: u,
	PluginClass: d
};
//#endregion
export { d as GpsPlugin, s as buildGprmcSentence, c as createGpsManifest, f as default, i as formatNmeaChecksum, l as gpsManifest, u as gpsManifestFactory };
