import {
  normalizeManifest,
  resolvePluginIdentity,
  BaseSimulationPlugin,
  type PeripheralManifest,
  type ManifestFactory,
} from '@wink-ai/unisim';

declare const __PLUGIN_TYPE__: string | undefined;
declare const __PLUGIN_VERSION__: string | undefined;
declare const __PLUGIN_CATEGORY__: string | undefined;

const identity = resolvePluginIdentity(
  import.meta.url,
  typeof __PLUGIN_TYPE__ !== 'undefined' ? __PLUGIN_TYPE__ : 'gps',
  typeof __PLUGIN_VERSION__ !== 'undefined' ? __PLUGIN_VERSION__ : '1.0.0',
  typeof __PLUGIN_CATEGORY__ !== 'undefined' ? __PLUGIN_CATEGORY__ : 'comm',
);

export type GpsVariant = 'nmea_uart' | 'nmea_i2c';

export type GpsProps = {
  baudRate: number;
  updateIntervalMs: number;
  latitude: number;
  longitude: number;
  fix: boolean;
  uartPort: number;
  variant: GpsVariant;
};

export type GpsState = {
  latitude: number;
  longitude: number;
  fix: boolean;
  lastSentence: string;
};

export function formatNmeaChecksum(sentenceWithoutDollar: string): string {
  let checksum = 0;
  for (let i = 0; i < sentenceWithoutDollar.length; i++) {
    checksum ^= sentenceWithoutDollar.charCodeAt(i);
  }
  const hex = checksum.toString(16).toUpperCase().padStart(2, '0');
  return `$${sentenceWithoutDollar}*${hex}\r\n`;
}

function formatLat(lat: number): { val: string; dir: string } {
  const dir = lat >= 0 ? 'N' : 'S';
  const abs = Math.abs(lat);
  const deg = Math.floor(abs);
  const min = (abs - deg) * 60;
  const degStr = deg.toString().padStart(2, '0');
  const minStr = min.toFixed(4).padStart(7, '0');
  return { val: `${degStr}${minStr}`, dir };
}

function formatLng(lng: number): { val: string; dir: string } {
  const dir = lng >= 0 ? 'E' : 'W';
  const abs = Math.abs(lng);
  const deg = Math.floor(abs);
  const min = (abs - deg) * 60;
  const degStr = deg.toString().padStart(3, '0');
  const minStr = min.toFixed(4).padStart(7, '0');
  return { val: `${degStr}${minStr}`, dir };
}

export function buildGprmcSentence(lat: number, lng: number, fix: boolean, timeSec = 0): string {
  const status = fix ? 'A' : 'V';
  const latFmt = formatLat(lat);
  const lngFmt = formatLng(lng);

  const hh = Math.floor((timeSec / 3600) % 24)
    .toString()
    .padStart(2, '0');
  const mm = Math.floor((timeSec / 60) % 60)
    .toString()
    .padStart(2, '0');
  const ss = Math.floor(timeSec % 60)
    .toString()
    .padStart(2, '0');
  const timeStr = `${hh}${mm}${ss}.00`;
  const dateStr = '100826';

  const raw = `GPRMC,${timeStr},${status},${latFmt.val},${latFmt.dir},${lngFmt.val},${lngFmt.dir},0.0,0.0,${dateStr},,,${status === 'A' ? 'A' : 'N'}`;
  return formatNmeaChecksum(raw);
}

export function createGpsManifest(variantName: GpsVariant = 'nmea_uart'): PeripheralManifest {
  return normalizeManifest({
    type: identity.type,
    version: identity.version,
    category: identity.category,
    displayName: 'GPS Receiver Module',
    description: 'GPS receiver module (NMEA 0183 via UART / I2C)',
    timingModel: 'event-driven',
    pins: [
      {
        name: 'TX',
        pinType: 'digital_out',
        role: 'signal',
        aliases: ['tx', 'out'],
        required: true,
      },
      { name: 'RX', pinType: 'digital_in', role: 'signal', aliases: ['rx', 'in'], required: false },
      { name: 'VCC', pinType: 'vcc', role: 'power', aliases: ['5v', 'vcc'], required: false },
      { name: 'GND', pinType: 'gnd', role: 'ground', aliases: ['gnd'], required: false },
    ],
    properties: {
      baudRate: { type: 'number', default: 9600, min: 1200, max: 115200 },
      updateIntervalMs: { type: 'number', default: 1000, min: 100, max: 10000, unit: 'ms' },
      latitude: { type: 'number', default: 31.2304, min: -90, max: 90, unit: 'deg' },
      longitude: { type: 'number', default: 121.4737, min: -180, max: 180, unit: 'deg' },
      fix: { type: 'boolean', default: true },
      uartPort: { type: 'number', default: 0 },
      variant: { type: 'string', default: variantName },
    },
    stateChannels: {
      latitude: { type: 'number', default: 31.2304, unit: 'deg' },
      longitude: { type: 'number', default: 121.4737, unit: 'deg' },
      fix: { type: 'boolean', default: true },
      lastSentence: { type: 'string', default: '' },
    },
    events: {
      SET_LOCATION: {
        description: 'Set simulated GPS coordinates and fix state',
        params: {
          latitude: { type: 'number', required: false, min: -90, max: 90 },
          longitude: { type: 'number', required: false, min: -180, max: 180 },
          fix: { type: 'boolean', required: false },
        },
      },
    },
  });
}

export const gpsManifest: PeripheralManifest = createGpsManifest();
export const gpsManifestFactory: ManifestFactory = (variant?: string) =>
  createGpsManifest((variant as GpsVariant) || 'nmea_uart');

export class GpsPlugin extends BaseSimulationPlugin<GpsState, GpsProps> {
  override get type(): string {
    return identity.type;
  }
  readonly manifest = gpsManifest;
  static readonly manifest = gpsManifest;

  private _lastSentUs = -1n;
  private _latitude = 31.2304;
  private _longitude = 121.4737;
  private _fix = true;
  private _uartPort = 0;
  private _updateIntervalUs = 1000_000n;

  protected override onBound(
    ctx: any,
    _pinMapping: Record<string, number>,
    props: GpsProps,
  ): Partial<GpsState> {
    this.onPropsUpdated(props);
    this._lastSentUs = -1n;
    return {
      latitude: this._latitude,
      longitude: this._longitude,
      fix: this._fix,
      lastSentence: '',
    };
  }

  onDestroy(): void {}

  onPropsUpdated(props: GpsProps): void {
    if (props.latitude !== undefined) this._latitude = Number(props.latitude);
    if (props.longitude !== undefined) this._longitude = Number(props.longitude);
    if (props.fix !== undefined) this._fix = Boolean(props.fix);
    if (props.uartPort !== undefined) this._uartPort = Number(props.uartPort);
    const intervalMs = Math.max(100, Number(props.updateIntervalMs ?? 1000));
    this._updateIntervalUs = BigInt(Math.floor(intervalMs * 1000));

    this.ctx?.publish('latitude', this._latitude as any);
    this.ctx?.publish('longitude', this._longitude as any);
    this.ctx?.publish('fix', this._fix as any);
  }

  onStep(atUs: bigint): void {
    if (!this.ctx) return;
    if (this._lastSentUs < 0n || atUs - this._lastSentUs >= this._updateIntervalUs) {
      this._lastSentUs = atUs;
      this.emitNmeaSentence(atUs);
    }
  }

  private emitNmeaSentence(atUs: bigint): void {
    const timeSec = Number(atUs / 1000000n);
    const sentence = buildGprmcSentence(this._latitude, this._longitude, this._fix, timeSec);
    this.ctx?.publish('lastSentence', sentence as any);

    const encoder = new TextEncoder();
    const data = encoder.encode(sentence);
    if (this.ctx?.bus?.uart) {
      this.ctx.bus.uart.toMcu(this._uartPort, data);
    } else if (typeof (this.ctx as any)?.writeUart === 'function') {
      (this.ctx as any).writeUart(this._uartPort, data);
    }
  }

  _location(params: { latitude?: number; longitude?: number; fix?: boolean }): void {
    this._SET_LOCATION(params);
  }

  _setLocation(params: { latitude?: number; longitude?: number; fix?: boolean }): void {
    this._SET_LOCATION(params);
  }

  _SET_LOCATION(params: { latitude?: number; longitude?: number; fix?: boolean }): void {
    if (params.latitude !== undefined) this._latitude = Number(params.latitude);
    if (params.longitude !== undefined) this._longitude = Number(params.longitude);
    if (params.fix !== undefined) this._fix = Boolean(params.fix);

    this.ctx?.publish('latitude', this._latitude as any);
    this.ctx?.publish('longitude', this._longitude as any);
    this.ctx?.publish('fix', this._fix as any);
  }
}

export default {
  manifest: gpsManifest,
  manifestFactory: gpsManifestFactory,
  PluginClass: GpsPlugin,
};
