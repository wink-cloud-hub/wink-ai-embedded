import {
  BaseSimulationPlugin as e,
  normalizeManifest as t,
  resolvePluginIdentity as n,
} from "@wink-ai/unisim-sdk";
//#region src/simulation.ts
var r = n(import.meta.url, "i2c_eeprom", "1.0.0", "generic"),
  i = 80,
  a = 32768,
  o = 64;
function s() {
  return t({
    type: r.type,
    version: r.version,
    category: r.category,
    displayName: "AT24C256 I2C EEPROM",
    description: "32 KiB 24C-series I2C EEPROM with 64-byte page write and tWR NACK window",
    timingModel: "event-driven",
    pins: [
      {
        name: "VCC",
        pinType: "vcc",
        role: "power",
        required: !1,
      },
      {
        name: "GND",
        pinType: "gnd",
        role: "ground",
        required: !1,
      },
      {
        name: "SDA",
        pinType: "i2c_sda",
        role: "signal",
        required: !0,
      },
      {
        name: "SCL",
        pinType: "i2c_scl",
        role: "signal",
        required: !0,
      },
      {
        name: "WP",
        pinType: "digital_in",
        role: "signal",
        required: !1,
      },
    ],
    properties: {
      address: {
        type: "number",
        default: i,
      },
      sizeBytes: {
        type: "number",
        default: a,
      },
      pageSize: {
        type: "number",
        default: o,
      },
      writeCycleUs: {
        type: "number",
        default: 0,
      },
    },
    stateChannels: {
      addressPointer: {
        type: "number",
        default: 0,
      },
      busy: {
        type: "number",
        default: 0,
      },
      writeCount: {
        type: "number",
        default: 0,
      },
    },
    events: {},
  });
}
var c = s(),
  l = () => s(),
  u = class extends e {
    get type() {
      return r.type;
    }
    manifest = c;
    static manifest = c;
    _address = i;
    _size = a;
    _pageSize = o;
    _writeCycleUs = 0;
    _memory = new Uint8Array(a);
    _pointer = 0;
    _writeAddress = 0;
    _addressPending = !1;
    _writeData = [];
    _addressBytes = [];
    _phase = "idle";
    _busyUntilUs = 0n;
    _writeCount = 0;
    _busyGeneration = 0;
    _unregisterI2c;
    get addr() {
      return this._address;
    }
    get addressPointer() {
      return this._pointer;
    }
    onBound(e, t, n) {
      ((this._address = Number(n.address ?? i) & 127),
        (this._size = Math.max(1, Math.trunc(Number(n.sizeBytes ?? a)))),
        (this._pageSize = Math.max(1, Math.trunc(Number(n.pageSize ?? o)))),
        (this._writeCycleUs = Math.max(0, Math.trunc(Number(n.writeCycleUs ?? 0)))),
        (this._memory = new Uint8Array(this._size)),
        (this._pointer = 0),
        (this._writeCount = 0));
      let r = this.ctx?.bus?.i2c;
      (r && typeof r.registerDevice == "function"
        ? (this._unregisterI2c = r.registerDevice({
            address: this._address,
            onTransfer: (e, t) =>
              this.onTransfer(e, t).readBytes ?? /* @__PURE__ */ new Uint8Array(),
            onAddressPhase: (e) => this.onAddressPhase(e),
            onWriteByte: (e, t) => this.onWriteByte(e, t),
            onReadByte: (e, t) => this.onReadByte(e, t),
            onTransactionStart: () => this.onTransactionStart(),
            onTransactionEnd: () => this.onTransactionEnd(),
          }))
        : this.ctx && this.ctx.registerI2cDevice(this),
        this.ctx?.publish("addressPointer", 0),
        this.ctx?.publish("busy", 0),
        this.ctx?.publish("writeCount", 0));
    }
    onDestroy() {
      if (this._unregisterI2c) {
        (this._unregisterI2c(), (this._unregisterI2c = void 0));
        return;
      }
      super.onDestroy();
    }
    onReset() {
      ((this._pointer = 0),
        (this._writeAddress = 0),
        (this._addressPending = !1),
        (this._writeData = []),
        (this._addressBytes = []),
        (this._phase = "idle"),
        (this._busyUntilUs = 0n),
        this._busyGeneration++,
        this.ctx?.publish("addressPointer", 0),
        this.ctx?.publish("busy", 0));
    }
    onAddressPhase(e) {
      return !this.ctx || this.ctx.nowUs() >= this._busyUntilUs;
    }
    onTransactionStart() {
      (this._finalizePending(!1),
        (this._phase = "address"),
        (this._addressBytes = []),
        (this._writeData = []),
        (this._addressPending = !1));
    }
    onTransactionEnd() {
      (this._finalizePending(!0), (this._phase = "idle"));
    }
    onWriteByte(e, t) {
      return this.onAddressPhase(0)
        ? this._phase === "address"
          ? (this._addressBytes.push(e & 255),
            this._addressBytes.length === 2 &&
              ((this._writeAddress =
                (((this._addressBytes[0] << 8) | this._addressBytes[1]) >>> 0) % this._size),
              (this._addressPending = !0),
              (this._phase = "data")),
            !0)
          : ((this._addressPending = !1), this._writeData.push(e & 255), !0)
        : !1;
    }
    onReadByte(e, t) {
      let n = this._memory[this._pointer % this._size];
      return (
        (this._pointer = (this._pointer + 1) % this._size),
        this.ctx?.publish("addressPointer", this._pointer),
        n
      );
    }
    onTransfer(e, t) {
      if ((this.onTransactionStart(), !this.onAddressPhase(0)))
        return (
          this.onTransactionEnd(),
          {
            ack: !1,
            nackBits: 1,
            nackBitsTruncated: !1,
            stretchUs: void 0,
          }
        );
      let n = !0,
        r = 0,
        i = !1;
      for (let t = 0; t < e.length; t++)
        this.onWriteByte(e[t], t) || ((n = !1), t < 32 ? (r |= 1 << t) : (i = !0));
      let a;
      if (n && t > 0) {
        (this._finalizePending(!1), (a = new Uint8Array(t)));
        for (let e = 0; e < t; e++) a[e] = this.onReadByte(e, e < t - 1);
      }
      return (
        this.onTransactionEnd(),
        {
          ack: n,
          readBytes: a,
          nackBits: r >>> 0,
          nackBitsTruncated: i,
          stretchUs: this._writeCycleUs > 0 ? this._writeCycleUs : void 0,
        }
      );
    }
    serializeState() {
      return {
        addressPointer: this._pointer,
        busyUntilUs: this._busyUntilUs.toString(),
        memoryBase64: f(this._memory),
        writeCount: this._writeCount,
      };
    }
    deserializeState(e) {
      if (typeof e.memoryBase64 == "string") {
        let t = p(e.memoryBase64);
        t.length === this._size && (this._memory = t);
      }
      if (
        (typeof e.addressPointer == "number" && (this._pointer = e.addressPointer % this._size),
        typeof e.busyUntilUs == "string")
      )
        try {
          this._busyUntilUs = BigInt(e.busyUntilUs);
        } catch {
          this._busyUntilUs = 0n;
        }
      (typeof e.writeCount == "number" && (this._writeCount = e.writeCount),
        (this._phase = "idle"),
        this._busyGeneration++,
        this.ctx?.publish("addressPointer", this._pointer),
        this.ctx?.publish("busy", +(this._busyUntilUs > 0n)));
    }
    _finalizePending(e) {
      if (this._writeData.length > 0 && e) {
        let e = this._writeAddress - (this._writeAddress % this._pageSize),
          t = this._writeAddress;
        for (let n of this._writeData) {
          let r = (t - e) % this._pageSize;
          ((this._memory[e + r] = n), t++);
        }
        if (
          ((this._pointer = t % this._size),
          this._writeCount++,
          (this._addressPending = !1),
          (this._addressBytes = []),
          (this._writeData = []),
          this.ctx?.publish("addressPointer", this._pointer),
          this.ctx?.publish("writeCount", this._writeCount),
          this._writeCycleUs > 0 && this.ctx)
        ) {
          ((this._busyUntilUs = this.ctx.nowUs() + BigInt(this._writeCycleUs)),
            this.ctx.publish("busy", 1));
          let e = ++this._busyGeneration;
          this.ctx.deferUs(BigInt(this._writeCycleUs), () => {
            e === this._busyGeneration && this.ctx?.publish("busy", 0);
          });
        }
        return;
      }
      (this._addressPending &&
        ((this._pointer = this._writeAddress % this._size),
        (this._addressPending = !1),
        this.ctx?.publish("addressPointer", this._pointer)),
        (this._addressBytes = []),
        e && (this._writeData = []));
    }
  },
  d = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
function f(e) {
  let t = "";
  for (let n = 0; n < e.length; n += 3) {
    let r = e[n],
      i = n + 1 < e.length ? e[n + 1] : 0,
      a = n + 2 < e.length ? e[n + 2] : 0;
    ((t += d[r >> 2]),
      (t += d[((r & 3) << 4) | (i >> 4)]),
      (t += n + 1 < e.length ? d[((i & 15) << 2) | (a >> 6)] : "="),
      (t += n + 2 < e.length ? d[a & 63] : "="));
  }
  return t;
}
function p(e) {
  let t = e.replace(/[^A-Za-z0-9+/]/g, ""),
    n = Math.floor((t.length * 3) / 4),
    r = new Uint8Array(n),
    i = 0;
  for (let e = 0; e < t.length; e += 4) {
    let a = d.indexOf(t[e]),
      o = d.indexOf(t[e + 1]),
      s = e + 2 < t.length ? d.indexOf(t[e + 2]) : -1,
      c = e + 3 < t.length ? d.indexOf(t[e + 3]) : -1;
    (i < n && (r[i++] = ((a << 2) | (o >> 4)) & 255),
      s >= 0 && i < n && (r[i++] = ((o << 4) | (s >> 2)) & 255),
      c >= 0 && i < n && (r[i++] = ((s << 6) | c) & 255));
  }
  return r;
}
var m = {
  manifest: c,
  manifestFactory: l,
  PluginClass: u,
};
//#endregion
export {
  u as I2cEepromPlugin,
  s as createI2cEepromManifest,
  m as default,
  c as i2cEepromManifest,
  l as i2cEepromManifestFactory,
};
