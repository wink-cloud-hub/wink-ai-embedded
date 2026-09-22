import {
  BaseSimulationPlugin as e,
  normalizeManifest as t,
  resolvePluginIdentity as n,
} from "@wink-ai/unisim-sdk";
//#region src/simulation.ts
var r = n(import.meta.url, "spi_eeprom", "1.0.0", "generic"),
  i = 32768,
  a = 64,
  o = 1,
  s = 2,
  c = 3,
  l = 4,
  u = 5,
  d = 6;
function f() {
  return t({
    type: r.type,
    version: r.version,
    category: r.category,
    displayName: "M95256 SPI EEPROM",
    description: "32 KiB 25/95-series SPI EEPROM with 64-byte page write, WEL latch and WIP window",
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
        name: "SCK",
        pinType: "spi_sck",
        role: "signal",
        required: !0,
      },
      {
        name: "MOSI",
        pinType: "spi_mosi",
        role: "signal",
        required: !0,
      },
      {
        name: "MISO",
        pinType: "spi_miso",
        role: "signal",
        required: !0,
      },
      {
        name: "CS",
        pinType: "spi_cs",
        role: "signal",
        required: !0,
      },
      {
        name: "W",
        pinType: "digital_in",
        role: "signal",
        required: !1,
      },
    ],
    properties: {
      deviceId: {
        type: "string",
        default: "spi_eeprom",
      },
      sizeBytes: {
        type: "number",
        default: i,
      },
      pageSize: {
        type: "number",
        default: a,
      },
      writeCycleUs: {
        type: "number",
        default: 0,
      },
    },
    stateChannels: {
      wel: {
        type: "number",
        default: 0,
      },
      wip: {
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
var p = f(),
  m = () => f(),
  h = class extends e {
    get type() {
      return r.type;
    }
    manifest = p;
    static manifest = p;
    _deviceId = "spi_eeprom";
    _size = i;
    _pageSize = a;
    _writeCycleUs = 0;
    _memory = new Uint8Array(i).fill(255);
    _wel = !1;
    _wipUntilUs = 0n;
    _writeCount = 0;
    _wipGeneration = 0;
    _phase = "idle";
    _cmd = 0;
    _ignored = !1;
    _addrBytes = [];
    _address = 0;
    _writeData = [];
    _unregisterSpi;
    onBound(e, t, n) {
      ((this._deviceId = String(n.deviceId ?? "spi_eeprom")),
        (this._size = Math.max(1, Math.trunc(Number(n.sizeBytes ?? i)))),
        (this._pageSize = Math.max(1, Math.trunc(Number(n.pageSize ?? a)))),
        (this._writeCycleUs = Math.max(0, Math.trunc(Number(n.writeCycleUs ?? 0)))),
        (this._memory = new Uint8Array(this._size).fill(255)),
        (this._wel = !1),
        (this._wipUntilUs = 0n),
        (this._writeCount = 0));
      let r = this.ctx?.bus?.spi;
      (r &&
        typeof r.registerDevice == "function" &&
        (this._unregisterSpi = r.registerDevice({
          deviceId: this._deviceId,
          mode: 0,
          csPin: "CS",
          onTransactionStart: () => this.onTransactionStart(),
          onExchangeByte: (e, t) => this.onExchangeByte(e, t),
          onFrame: (e) => this.onFrame(e),
          onTransactionEnd: () => this.onTransactionEnd(),
        })),
        this.ctx?.publish("wel", 0),
        this.ctx?.publish("wip", 0),
        this.ctx?.publish("writeCount", 0));
    }
    onDestroy() {
      if (this._unregisterSpi) {
        (this._unregisterSpi(), (this._unregisterSpi = void 0));
        return;
      }
      super.onDestroy();
    }
    onReset() {
      ((this._wel = !1),
        (this._wipUntilUs = 0n),
        this._wipGeneration++,
        this._resetFrame(),
        this.ctx?.publish("wel", 0),
        this.ctx?.publish("wip", 0));
    }
    isWelSet() {
      return this._wel;
    }
    isWip() {
      return this._isWipActive();
    }
    onTransactionStart() {
      this._resetFrame();
    }
    onExchangeByte(e, t) {
      if (this._phase === "idle") return 255;
      if (this._phase === "cmd") {
        if (((this._cmd = e & 255), this._isWipActive() && this._cmd !== u))
          return ((this._ignored = !0), (this._phase = "status"), 255);
        switch (this._cmd) {
          case c:
          case s:
            this._phase = "addr";
            break;
          case d:
          case l:
          case u:
          case o:
          default:
            this._phase = "status";
        }
        return 0;
      }
      if (this._phase === "addr")
        return (
          this._addrBytes.push(e & 255),
          this._addrBytes.length === 2 &&
            ((this._address =
              (((this._addrBytes[0] << 8) | this._addrBytes[1]) >>> 0) % this._size),
            (this._phase = "data")),
          0
        );
      if (this._phase === "data") {
        if (this._cmd === c) {
          let e = this._memory[this._address % this._size];
          return ((this._address = (this._address + 1) % this._size), e);
        }
        return (this._cmd === s && this._writeData.push(e & 255), 0);
      }
      return this._cmd === u && !this._ignored ? this._status() : 0;
    }
    onTransactionEnd() {
      let e = this._ignored,
        t = this._cmd;
      (e ||
        (t === d
          ? (this._wel = !0)
          : t === l
            ? (this._wel = !1)
            : t === s &&
              (this._wel &&
                !this._isWipActive() &&
                this._writeData.length > 0 &&
                this._commitWrite(),
              (this._wel = !1))),
        this._resetFrame(),
        this.ctx?.publish("wel", +!!this._wel),
        this.ctx?.publish("wip", +!!this._isWipActive()));
    }
    onFrame(e) {
      this.onTransactionStart();
      let t = new Uint8Array(e.length);
      for (let n = 0; n < e.length; n++) t[n] = this.onExchangeByte(e[n], n);
      return (this.onTransactionEnd(), t);
    }
    serializeState() {
      return {
        wel: +!!this._wel,
        wipUntilUs: this._wipUntilUs.toString(),
        writeCount: this._writeCount,
        memoryBase64: _(this._memory),
      };
    }
    deserializeState(e) {
      if (typeof e.memoryBase64 == "string") {
        let t = v(e.memoryBase64);
        t.length === this._size && (this._memory = t);
      }
      if (((this._wel = e.wel === 1), typeof e.wipUntilUs == "string"))
        try {
          this._wipUntilUs = BigInt(e.wipUntilUs);
        } catch {
          this._wipUntilUs = 0n;
        }
      (typeof e.writeCount == "number" && (this._writeCount = e.writeCount),
        this._wipGeneration++,
        this._resetFrame(),
        this.ctx?.publish("wel", +!!this._wel),
        this.ctx?.publish("wip", +!!this._isWipActive()));
    }
    _status() {
      return (this._wel ? 2 : 0) | !!this._isWipActive();
    }
    _isWipActive() {
      return this.ctx ? this.ctx.nowUs() < this._wipUntilUs : !1;
    }
    _resetFrame() {
      ((this._phase = "cmd"),
        (this._cmd = 0),
        (this._ignored = !1),
        (this._addrBytes = []),
        (this._writeData = []));
    }
    _commitWrite() {
      let e = this._address - (this._address % this._pageSize),
        t = this._address;
      for (let n of this._writeData) {
        let r = (t - e) % this._pageSize;
        ((this._memory[e + r] = n), t++);
      }
      if (
        (this._writeCount++,
        this.ctx?.publish("writeCount", this._writeCount),
        this._writeCycleUs > 0 && this.ctx)
      ) {
        ((this._wipUntilUs = this.ctx.nowUs() + BigInt(this._writeCycleUs)),
          this.ctx.publish("wip", 1));
        let e = ++this._wipGeneration;
        this.ctx.deferUs(BigInt(this._writeCycleUs), () => {
          e === this._wipGeneration && this.ctx?.publish("wip", 0);
        });
      }
    }
  },
  g = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
function _(e) {
  let t = "";
  for (let n = 0; n < e.length; n += 3) {
    let r = e[n],
      i = n + 1 < e.length ? e[n + 1] : 0,
      a = n + 2 < e.length ? e[n + 2] : 0;
    ((t += g[r >> 2]),
      (t += g[((r & 3) << 4) | (i >> 4)]),
      (t += n + 1 < e.length ? g[((i & 15) << 2) | (a >> 6)] : "="),
      (t += n + 2 < e.length ? g[a & 63] : "="));
  }
  return t;
}
function v(e) {
  let t = e.replace(/[^A-Za-z0-9+/]/g, ""),
    n = Math.floor((t.length * 3) / 4),
    r = new Uint8Array(n),
    i = 0;
  for (let e = 0; e < t.length; e += 4) {
    let a = g.indexOf(t[e]),
      o = g.indexOf(t[e + 1]),
      s = e + 2 < t.length ? g.indexOf(t[e + 2]) : -1,
      c = e + 3 < t.length ? g.indexOf(t[e + 3]) : -1;
    (i < n && (r[i++] = ((a << 2) | (o >> 4)) & 255),
      s >= 0 && i < n && (r[i++] = ((o << 4) | (s >> 2)) & 255),
      c >= 0 && i < n && (r[i++] = ((s << 6) | c) & 255));
  }
  return r;
}
var y = {
  manifest: p,
  manifestFactory: m,
  PluginClass: h,
};
//#endregion
export {
  h as SpiEepromPlugin,
  f as createSpiEepromManifest,
  y as default,
  p as spiEepromManifest,
  m as spiEepromManifestFactory,
};
