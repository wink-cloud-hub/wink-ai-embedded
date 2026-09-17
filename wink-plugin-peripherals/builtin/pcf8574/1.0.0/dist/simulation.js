import { BaseSimulationPlugin as e, LogicStates as t, normalizeManifest as n, resolvePluginIdentity as r } from "@wink-ai/unisim-sdk";
//#region src/simulation.ts
var i = r(import.meta.url, "pcf8574", "1.0.0", "generic");
function a() {
	return n({
		type: i.type,
		version: i.version,
		category: i.category,
		displayName: "PCF8574 I2C IO Expander",
		description: "8-bit I2C I/O expander",
		timingModel: "event-driven",
		pins: [
			{
				name: "VCC",
				pinType: "vcc",
				role: "power",
				required: !1
			},
			{
				name: "GND",
				pinType: "gnd",
				role: "ground",
				required: !1
			},
			{
				name: "SDA",
				pinType: "i2c_sda",
				role: "signal",
				required: !0
			},
			{
				name: "SCL",
				pinType: "i2c_scl",
				role: "signal",
				required: !0
			},
			{
				name: "INT",
				pinType: "digital_out",
				role: "signal",
				required: !1
			},
			{
				name: "P0",
				pinType: "gpio",
				role: "gpio",
				required: !1
			},
			{
				name: "P1",
				pinType: "gpio",
				role: "gpio",
				required: !1
			},
			{
				name: "P2",
				pinType: "gpio",
				role: "gpio",
				required: !1
			},
			{
				name: "P3",
				pinType: "gpio",
				role: "gpio",
				required: !1
			},
			{
				name: "P4",
				pinType: "gpio",
				role: "gpio",
				required: !1
			},
			{
				name: "P5",
				pinType: "gpio",
				role: "gpio",
				required: !1
			},
			{
				name: "P6",
				pinType: "gpio",
				role: "gpio",
				required: !1
			},
			{
				name: "P7",
				pinType: "gpio",
				role: "gpio",
				required: !1
			}
		],
		properties: {
			address: {
				type: "number",
				default: 32
			},
			variant: {
				type: "string",
				default: "pcf8574_i2c"
			}
		},
		stateChannels: { latch: {
			type: "number",
			default: 255
		} },
		events: {}
	});
}
var o = a(), s = () => a(), c = class extends e {
	get type() {
		return i.type;
	}
	manifest = o;
	static manifest = o;
	_i2cAddress = 32;
	_outputLatch = 255;
	_pPins = [];
	get providerId() {
		return `io_expander:${this.ctx?.instanceId ?? "pcf8574"}`;
	}
	readVirtualPin(e) {
		return this.readInputs() & 1 << e ? t.HIGH : t.LOW;
	}
	writeVirtualPin(e, n) {
		n === t.HIGH ? this._outputLatch |= 1 << e : this._outputLatch &= ~(1 << e), this.updateOutputs(), this.ctx?.publish("latch", this._outputLatch);
	}
	_unregisterI2c;
	onBound(e, t, n) {
		this._i2cAddress = Number(n.address ?? 32), this._pPins = [
			t.P0 === void 0 ? void 0 : "P0",
			t.P1 === void 0 ? void 0 : "P1",
			t.P2 === void 0 ? void 0 : "P2",
			t.P3 === void 0 ? void 0 : "P3",
			t.P4 === void 0 ? void 0 : "P4",
			t.P5 === void 0 ? void 0 : "P5",
			t.P6 === void 0 ? void 0 : "P6",
			t.P7 === void 0 ? void 0 : "P7"
		];
		let r = this.ctx?.bus?.i2c;
		r && typeof r.registerDevice == "function" ? this._unregisterI2c = r.registerDevice({
			address: this._i2cAddress,
			onTransfer: (e, t) => this.onTransfer(e, t).readBytes ?? /* @__PURE__ */ new Uint8Array()
		}) : this.ctx && typeof this.ctx.registerI2cDevice == "function" && this.ctx.registerI2cDevice(this), this.updateOutputs();
	}
	onDestroy() {
		this._unregisterI2c ? (this._unregisterI2c(), this._unregisterI2c = void 0) : this.ctx?.bus?.i2c && typeof this.ctx.bus.i2c.unregisterDevice == "function" ? this.ctx.bus.i2c.unregisterDevice(this._i2cAddress) : this.ctx && typeof this.ctx.unregisterI2cDevice == "function" && this.ctx.unregisterI2cDevice(this._i2cAddress);
	}
	get addr() {
		return this._i2cAddress;
	}
	onTransfer(e, t) {
		e.length > 0 && (this._outputLatch = e[0], this.updateOutputs(), this.ctx?.publish("latch", this._outputLatch));
		let n;
		return t > 0 && (n = new Uint8Array([this.readInputs()])), {
			ack: !0,
			readBytes: n
		};
	}
	readInputs() {
		let e = 0;
		for (let n = 0; n < 8; n++) {
			let r = this._pPins[n];
			if (r) {
				let i = this.ctx?.gpio ? this.ctx.gpio.read(r) : typeof this.ctx?.readPin == "function" ? this.ctx.readPin(r) : t.HIGH;
				(i === t.HIGH || i === !0) && (e |= 1 << n);
			} else e |= 1 << n;
		}
		return e;
	}
	updateOutputs() {
		for (let e = 0; e < 8; e++) {
			let n = this._pPins[e];
			if (n) {
				let r = !!(this._outputLatch & 1 << e), i = r ? t.HIGH : t.LOW;
				this.ctx?.gpio ? this.ctx.gpio.write(n, i) : typeof this.ctx?.writePin == "function" && this.ctx.writePin(n, r);
			}
		}
	}
}, l = {
	manifest: o,
	manifestFactory: s,
	PluginClass: c
};
//#endregion
export { c as Pcf8574Plugin, a as createPcf8574Manifest, l as default, o as pcf8574Manifest, s as pcf8574ManifestFactory };
