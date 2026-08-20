import { I2cPeripheralPlugin as e, createThrottlePublish as t, normalizeManifest as n, normalizeVariantKey as r, resolvePluginIdentity as i } from "@wink-ai/unisim";
//#region builtin/mono_oled/1.0.0/src/simulation.ts
var a = i(import.meta.url, "mono_oled", "1.0.0", "display"), o = 128, s = 64, c = 1024, l = 16000n, u = {
	ssd1306_i2c: {
		displayName: "SSD1306 0.96\" I2C OLED Display",
		pins: [
			{
				name: "DATA",
				pinType: "i2c_sda",
				required: !0,
				busGroup: "i2c0",
				description: "I2C SDA",
				aliases: ["sda"]
			},
			{
				name: "CLK",
				pinType: "i2c_scl",
				required: !0,
				busGroup: "i2c0",
				description: "I2C SCL",
				aliases: ["scl"]
			},
			{
				name: "3V3",
				pinType: "vcc",
				direction: "power",
				voltage: "3.3V",
				description: "Power 3.3V",
				required: !1
			},
			{
				name: "GND",
				pinType: "gnd",
				direction: "ground",
				description: "Ground",
				required: !1
			}
		]
	},
	ssd1306_spi: {
		displayName: "SSD1306 0.96\" SPI OLED Display",
		pins: [
			{
				name: "CLK",
				pinType: "spi_sck",
				required: !0,
				busGroup: "spi0",
				description: "SPI Clock",
				aliases: ["sck", "scl"]
			},
			{
				name: "DIN",
				pinType: "spi_mosi",
				required: !0,
				busGroup: "spi0",
				description: "SPI MOSI Data",
				aliases: ["mosi", "sda"]
			},
			{
				name: "CS",
				pinType: "spi_cs",
				required: !1,
				busGroup: "spi0",
				description: "SPI Chip Select (-1 if dedicated)"
			},
			{
				name: "DC",
				pinType: "gpio",
				required: !0,
				description: "Data/Command Control"
			},
			{
				name: "RES",
				pinType: "gpio",
				required: !1,
				description: "Reset Pin (-1 if hardwired)"
			},
			{
				name: "3V3",
				pinType: "vcc",
				direction: "power",
				voltage: "3.3V",
				description: "Power 3.3V",
				required: !1
			},
			{
				name: "GND",
				pinType: "gnd",
				direction: "ground",
				description: "Ground",
				required: !1
			}
		]
	}
};
function d(e) {
	return r(e)?.includes("spi") ? "ssd1306_spi" : "ssd1306_i2c";
}
function f(e = "ssd1306_i2c") {
	let t = u[e] ?? u.ssd1306_i2c;
	return n({
		type: a.type,
		version: a.version,
		category: a.category,
		displayName: t.displayName,
		description: "Mono OLED display (SSD1306/SH1106) with a throttled framebuffer channel",
		timingModel: "event-driven",
		pins: t.pins,
		properties: {
			variant: {
				type: "string",
				default: e
			},
			panel_ic: {
				type: "string",
				default: "ssd1306"
			},
			i2cAddr: {
				type: "number",
				default: 60,
				min: 3,
				max: 119
			},
			i2cBus: {
				type: "number",
				default: 0,
				min: 0,
				max: 1
			}
		},
		requirements: { i2c: [{
			bus: 0,
			address: "0x3C"
		}] },
		stateChannels: {
			fb: {
				type: "string",
				show: !1,
				description: "Uint8Array binary framebuffer snapshot (128x64, page-major)"
			},
			width: {
				type: "number",
				default: 128,
				description: "Framebuffer width in pixels"
			},
			height: {
				type: "number",
				default: 64,
				description: "Framebuffer height in pixels"
			},
			colorFormat: {
				type: "string",
				default: "mono-ssd1306",
				show: !1,
				description: "Pixel layout format (mono-ssd1306)"
			},
			displayKind: {
				type: "string",
				default: "ssd1306_fb",
				show: !1,
				description: "Worker display collection kind (ssd1306_fb)"
			}
		},
		events: {}
	});
}
var p = f("ssd1306_i2c"), m = (e) => f(d(e)), h = class extends e {
	_manifest = p;
	static manifest = p;
	get manifest() {
		return this._manifest;
	}
	applyManifest(e) {
		this._manifest = e;
	}
	framebuffer = new Uint8Array(c);
	height = s;
	colStart = 0;
	colEnd = 127;
	colCursor = 0;
	pageStart = 0;
	pageEnd = s / 8 - 1;
	pageCursor = 0;
	addressMode = 0;
	throttle = t({
		ctx: () => this.ctx,
		intervalUs: l,
		publish: () => this.publishFramebuffer()
	});
	onDestroy() {
		this.throttle.reset(), super.onDestroy();
	}
	onI2cBound(e, t) {
		this.publishInitialState();
	}
	publishInitialState() {
		this.ctx?.publish("displayKind", "ssd1306_fb"), this.ctx?.publish("width", o), this.ctx?.publish("height", this.height), this.ctx?.publish("colorFormat", "mono-ssd1306"), this.ctx?.publish("fb", new Uint8Array(this.framebuffer));
	}
	onI2cTransfer(e, t) {
		return this.handleI2cTransfer(e, t);
	}
	handleI2cTransfer(e, t) {
		return e.length > 1 && (e[0] === 0 || e[0] === 128 ? this.parseCommands(e.subarray(1)) : e[0] === 64 && this.writeData(e.subarray(1))), { ack: !0 };
	}
	parseCommands(e) {
		for (let t = 0; t < e.length;) {
			let n = e[t];
			n === 33 && t + 2 < e.length ? (this.colStart = Math.min(e[t + 1], 127), this.colEnd = Math.min(e[t + 2], 127), this.colCursor = this.colStart, t += 3) : n === 34 && t + 2 < e.length ? (this.pageStart = Math.min(e[t + 1], this.maxPage), this.pageEnd = Math.min(e[t + 2], this.maxPage), this.pageCursor = this.pageStart, t += 3) : n === 32 && t + 1 < e.length ? (this.addressMode = e[t + 1] & 3, t += 2) : n === 168 && t + 1 < e.length ? (this.height = e[t + 1] === 31 ? 32 : s, this.pageEnd = this.maxPage, this.pageCursor = Math.min(this.pageCursor, this.maxPage), t += 2) : n >= 176 && n <= 183 ? (this.pageCursor = Math.min(n - 176, this.maxPage), t++) : n <= 15 ? (this.colCursor = this.colCursor & 240 | n & 15, t++) : (n >= 16 && n <= 31 && (this.colCursor = this.colCursor & 15 | (n & 15) << 4), t++);
		}
	}
	writeData(e) {
		for (let t of e) {
			let e = this.pageCursor * o + this.colCursor;
			e < c && (this.framebuffer[e] = t), this.advanceCursor();
		}
		this.throttle.request();
	}
	advanceCursor() {
		if (this.addressMode === 0) {
			++this.colCursor > this.colEnd && (this.colCursor = this.colStart, ++this.pageCursor > this.pageEnd && (this.pageCursor = this.pageStart));
			return;
		}
		++this.colCursor >= o && (this.colCursor = 0);
	}
	publishFramebuffer() {
		this.ctx?.publish("displayKind", "ssd1306_fb"), this.ctx?.publish("fb", new Uint8Array(this.framebuffer)), this.ctx?.publish("width", o), this.ctx?.publish("height", this.height), this.ctx?.publish("colorFormat", "mono-ssd1306");
	}
	get maxPage() {
		return this.height / 8 - 1;
	}
}, g = {
	manifest: p,
	manifestFactory: m,
	PluginClass: h
};
//#endregion
export { u as MONO_OLED_PIN_VARIANTS, h as MonoOledPlugin, f as createMonoOledManifest, g as default, p as monoOledManifest, m as monoOledManifestFactory };
