import { BaseSimulationPlugin as e, normalizeManifest as t, resolvePluginIdentity as n } from "@wink-ai/unisim-sdk";
//#region src/simulation.ts
var r = n(import.meta.url, "imu", "1.0.0", "generic"), i = 104, a = 128, o = 55, s = 56, c = 58, l = 59, u = 65, d = 67, f = 107, p = 117, m = 12, h = 0, g = 72;
function _() {
	return t({
		type: r.type,
		version: r.version,
		category: r.category,
		displayName: "Inertial Measurement Unit (6-Axis/9-Axis)",
		description: "High-fidelity 6-Axis/9-Axis IMU sensor supporting MPU-6050/6500/9250 with 3D kinematics",
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
				name: "SCL",
				pinType: "i2c_scl",
				role: "signal",
				required: !0
			},
			{
				name: "SDA",
				pinType: "i2c_sda",
				role: "signal",
				required: !0
			},
			{
				name: "AD0",
				pinType: "digital_in",
				role: "signal",
				required: !1
			},
			{
				name: "INT",
				pinType: "digital_out",
				role: "signal",
				required: !1
			},
			{
				name: "XCL",
				pinType: "i2c_scl",
				role: "signal",
				required: !1
			},
			{
				name: "XDA",
				pinType: "i2c_sda",
				role: "signal",
				required: !1
			}
		],
		properties: {
			variant: {
				type: "string",
				default: "mpu6050_i2c"
			},
			address: {
				type: "number",
				default: i
			},
			roll: {
				type: "number",
				default: 0
			},
			pitch: {
				type: "number",
				default: 0
			},
			yaw: {
				type: "number",
				default: 0
			},
			temperature: {
				type: "number",
				default: 25
			}
		},
		stateChannels: {
			whoAmI: {
				type: "number",
				default: 104
			},
			pwrMgmt1: {
				type: "number",
				default: 1
			},
			regPointer: {
				type: "number",
				default: 0
			},
			accelX: {
				type: "number",
				default: 0
			},
			accelY: {
				type: "number",
				default: 0
			},
			accelZ: {
				type: "number",
				default: 16384
			},
			gyroX: {
				type: "number",
				default: 0
			},
			gyroY: {
				type: "number",
				default: 0
			},
			gyroZ: {
				type: "number",
				default: 0
			},
			tempDegc: {
				type: "number",
				default: 25
			},
			writeCount: {
				type: "number",
				default: 0
			},
			readCount: {
				type: "number",
				default: 0
			}
		},
		events: {}
	});
}
var v = _(), y = () => _(), b = class extends e {
	get type() {
		return r.type;
	}
	manifest = v;
	static manifest = v;
	_variant = "mpu6050_i2c";
	_address = i;
	_registers = new Uint8Array(a);
	_ak8963Regs = /* @__PURE__ */ new Uint8Array(16);
	_regPointer = 0;
	_isAddrPhase = !0;
	_writeCount = 0;
	_readCount = 0;
	_ad0Pin = -1;
	_intPin = -1;
	_roll = 0;
	_pitch = 0;
	_yaw = 0;
	_temperature = 25;
	_unregisterI2c;
	_unregisterAk8963;
	get addr() {
		return this._address;
	}
	_packBigEndian(e, t) {
		let n = t < 0 ? t + 65536 & 65535 : t & 65535;
		this._registers[e] = n >> 8 & 255, this._registers[e + 1] = n & 255;
	}
	_computeKinematics() {
		let e = this._roll * Math.PI / 180, t = this._pitch * Math.PI / 180, n = Math.round(-16384 * Math.sin(t)), r = Math.round(16384 * Math.sin(e) * Math.cos(t)), i = Math.round(16384 * Math.cos(e) * Math.cos(t));
		this._packBigEndian(l, n), this._packBigEndian(61, r), this._packBigEndian(63, i);
		let a = Math.round((this._temperature - 21) * 333.87);
		this._packBigEndian(u, a), this._packBigEndian(d, 0), this._packBigEndian(69, 0), this._packBigEndian(71, 0), this._registers[s] & 1 && (this._registers[c] |= 1, this.ctx && this._intPin >= 0 && this.ctx.setPinLevel(this._intPin, 1)), this._publishThrottledState(n, r, i);
	}
	_publishThrottledState(e, t, n) {
		this.ctx && (this.ctx.publish("whoAmI", this._registers[p]), this.ctx.publish("pwrMgmt1", this._registers[f]), this.ctx.publish("regPointer", this._regPointer), this.ctx.publish("accelX", e), this.ctx.publish("accelY", t), this.ctx.publish("accelZ", n), this.ctx.publish("tempDegc", this._temperature), this.ctx.publish("writeCount", this._writeCount), this.ctx.publish("readCount", this._readCount));
	}
	_resolveDefaultWhoAmI() {
		return this._variant.includes("9250") ? 113 : this._variant.includes("6500") ? 112 : 104;
	}
	_resetRegisters() {
		this._registers.fill(0), this._registers[p] = this._resolveDefaultWhoAmI(), this._registers[f] = 1, this._regPointer = 0, this._isAddrPhase = !0, this._ak8963Regs.fill(0), this._ak8963Regs[h] = g, this._computeKinematics();
	}
	onBound(e, t, n) {
		this._variant = String(n.variant ?? "mpu6050_i2c"), this._ad0Pin = t.AD0 ?? -1, this._intPin = t.INT ?? -1, this._roll = Number(n.roll ?? 0), this._pitch = Number(n.pitch ?? 0), this._yaw = Number(n.yaw ?? 0), this._temperature = Number(n.temperature ?? 25), this._address = n.address === void 0 ? i : Number(n.address) & 127, this._resetRegisters(), this._registerI2cBus(e);
	}
	_registerI2cBus(e) {
		this._unregisterI2c &&= (this._unregisterI2c(), void 0);
		let t = e?.bus?.i2c;
		if (t && typeof t.registerDevice == "function") this._unregisterI2c = t.registerDevice({
			address: this._address,
			onTransfer: (e, t) => this.onTransfer(e, t).readBytes ?? /* @__PURE__ */ new Uint8Array(),
			onAddressPhase: (e) => !0,
			onWriteByte: (e, t) => this.onWriteByte(e, t),
			onReadByte: (e, t) => this.onReadByte(e, t),
			onTransactionStart: () => this.onTransactionStart(),
			onTransactionEnd: () => this.onTransactionEnd()
		});
		else if (typeof e.registerI2cDevice == "function") {
			try {
				e.unregisterI2cDevice?.(this._address);
			} catch {}
			e.registerI2cDevice(this), this._unregisterI2c = () => {
				try {
					e.unregisterI2cDevice?.(this._address);
				} catch {}
			};
		}
		this._checkBypassRegistration(e);
	}
	_checkBypassRegistration(e) {
		let t = !!(this._registers[o] & 2);
		if (this._variant.includes("9250") && t) {
			if (!this._unregisterAk8963) {
				let t = {
					addr: m,
					onTransactionStart: () => {},
					onWriteByte: (e) => !0,
					onReadByte: (e) => this._ak8963Regs[e % 16],
					onTransactionEnd: () => {},
					onTransfer: (e, t) => {
						let n = new Uint8Array(t);
						for (let e = 0; e < t; e++) n[e] = this._ak8963Regs[e % 16];
						return {
							ack: !0,
							readBytes: n
						};
					}
				}, n = e?.bus?.i2c;
				n && typeof n.registerDevice == "function" ? this._unregisterAk8963 = n.registerDevice({
					address: m,
					onTransfer: t.onTransfer,
					onAddressPhase: (e) => !0,
					onWriteByte: t.onWriteByte,
					onReadByte: t.onReadByte,
					onTransactionStart: t.onTransactionStart,
					onTransactionEnd: t.onTransactionEnd
				}) : typeof e.registerI2cDevice == "function" && (e.registerI2cDevice(t), this._unregisterAk8963 = () => {
					try {
						e.unregisterI2cDevice?.(m);
					} catch {}
				});
			}
		} else this._unregisterAk8963 &&= (this._unregisterAk8963(), void 0);
	}
	onPinChange(e, t, n) {
		if (e === this._ad0Pin && this._ad0Pin >= 0) {
			let e = 104 | t === 1;
			e !== this._address && this.ctx && (this._address = e, this._registerI2cBus(this.ctx));
		}
	}
	onPropertyChange(e, t, n) {
		e === "roll" ? this._roll = Number(n ?? 0) : e === "pitch" ? this._pitch = Number(n ?? 0) : e === "yaw" ? this._yaw = Number(n ?? 0) : e === "temperature" ? this._temperature = Number(n ?? 25) : e === "address" && this.ctx && (this._address = Number(n ?? i) & 127, this._registerI2cBus(this.ctx)), this._computeKinematics();
	}
	onReset() {
		this._resetRegisters(), this.ctx && this._intPin >= 0 && this.ctx.setPinLevel(this._intPin, 0);
	}
	onDestroy() {
		this._unregisterI2c &&= (this._unregisterI2c(), void 0), this._unregisterAk8963 &&= (this._unregisterAk8963(), void 0), super.onDestroy();
	}
	onTransactionStart() {
		this._isAddrPhase = !0;
	}
	onTransactionEnd() {
		this._isAddrPhase = !0;
	}
	onWriteByte(e, t) {
		return this._isAddrPhase ? (this._regPointer = e % a, this._isAddrPhase = !1, this.ctx?.publish("regPointer", this._regPointer), !0) : (this._registers[this._regPointer] = e, this._writeCount++, this._regPointer === f && e & 128 ? (this._resetRegisters(), !0) : (this._regPointer === o && this.ctx && this._checkBypassRegistration(this.ctx), this.ctx?.publish("pwrMgmt1", this._registers[f]), this._regPointer = (this._regPointer + 1) % a, this.ctx?.publish("regPointer", this._regPointer), !0));
	}
	onReadByte(e, t) {
		let n = this._registers[this._regPointer];
		return this._regPointer === c && (this._registers[c] = 0, this.ctx && this._intPin >= 0 && this.ctx.setPinLevel(this._intPin, 0)), this._regPointer = (this._regPointer + 1) % a, this._readCount++, this.ctx?.publish("regPointer", this._regPointer), this.ctx?.publish("readCount", this._readCount), n;
	}
	onTransfer(e, t) {
		this.onTransactionStart();
		for (let t = 0; t < e.length; t++) this.onWriteByte(e[t], t);
		let n;
		if (t > 0) {
			n = new Uint8Array(t);
			for (let e = 0; e < t; e++) n[e] = this.onReadByte(e, e < t - 1);
		}
		return this.onTransactionEnd(), {
			ack: !0,
			readBytes: n
		};
	}
}, x = {
	manifest: v,
	manifestFactory: y,
	PluginClass: b
};
//#endregion
export { b as ImuPlugin, _ as createImuManifest, x as default, v as imuManifest, y as imuManifestFactory };
