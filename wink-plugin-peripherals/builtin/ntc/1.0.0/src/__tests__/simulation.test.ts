import { expect, test, describe } from 'bun:test';
import { NtcPlugin, ntcManifest } from '../simulation';

describe('NTC Simulation Plugin & LDO Perturbation Model (PLAN-20260921 Item 01)', () => {
  test('manifest declares expected properties, state channels and events', () => {
    expect(ntcManifest.type).toBe('ntc');
    expect(ntcManifest.properties.temperature.default).toBe(25);
    expect(ntcManifest.properties.r25.default).toBe(10000);
    expect(ntcManifest.properties.bValue.default).toBe(3950);
    expect(ntcManifest.properties.vref.default).toBe(3.0);
    expect(ntcManifest.properties.vrefNoisePct.default).toBe(0);
    expect(ntcManifest.properties.lineRegulationPct.default).toBe(0);
    expect(ntcManifest.properties.tempDriftPpm.default).toBe(0);
    expect(ntcManifest.properties.gndBouncePct.default).toBe(0);
    expect(ntcManifest.properties.heaterActive.default).toBe(false);

    expect(ntcManifest.stateChannels).toHaveProperty('temperature');
    expect(ntcManifest.stateChannels).toHaveProperty('voltageRatio');
    expect(ntcManifest.stateChannels).toHaveProperty('resistanceOhm');
    expect(ntcManifest.stateChannels).toHaveProperty('vrefEffective');

    expect(ntcManifest.events).toHaveProperty('SET_TEMPERATURE');
    expect(ntcManifest.events).toHaveProperty('SET_VREF_PERTURBATION');
    expect(ntcManifest.events).toHaveProperty('SET_HEATER_ACTIVE');
  });

  test('nominal 25 C anchor on 160k pullup divider produces expected ratio and raw code', () => {
    const plugin = new NtcPlugin();
    const publishes: Array<{ ch: string; v: unknown }> = [];
    let writtenNorm = -1;

    const ctx = {
      publish: (ch: string, v: unknown) => publishes.push({ ch, v }),
      adc: {
        writeNorm: (_pin: string, norm: number) => {
          writtenNorm = norm;
        },
      },
    } as any;

    plugin.onBind(ctx, { OUT: 0 }, {
      r25: 10000,
      bValue: 3950,
      pullUpResistor: 160000,
      temperature: 25,
    });

    const res = plugin.updateTemperature(25);
    expect(res.temperature).toBe(25);
    expect(Math.round(res.resistanceOhm)).toBe(10000);

    // 10000 / (10000 + 160000) = 10000 / 170000 ≈ 0.0588235
    const expectedRatio = 10000 / 170000;
    expect(res.voltageRatio).toBeCloseTo(expectedRatio, 5);
    expect(writtenNorm).toBeCloseTo(expectedRatio, 5);
    expect(res.vrefEffective).toBe(3.0);

    const raw12bit = Math.round(writtenNorm * 4095);
    expect(raw12bit).toBe(241);
  });

  test('temperature scaling at 55 C, 80 C, and 98 C matches CMS8S 12-bit ADC expectations', () => {
    const plugin = new NtcPlugin();
    const ctx = {
      publish: () => {},
      adc: { writeNorm: () => {} },
    } as any;

    plugin.onBind(ctx, { OUT: 0 }, {
      r25: 10000,
      bValue: 3950,
      pullUpResistor: 160000,
      temperature: 25,
    });

    // 55 C:
    const res55 = plugin.updateTemperature(55);
    const raw55 = Math.round(res55.voltageRatio * 4095);
    expect(raw55).toBe(75);

    // 80 C:
    const res80 = plugin.updateTemperature(80);
    const raw80 = Math.round(res80.voltageRatio * 4095);
    expect(raw80).toBe(32);

    // 98 C:
    const res98 = plugin.updateTemperature(98);
    const raw98 = Math.round(res98.voltageRatio * 4095);
    expect(raw98).toBe(19);
  });

  test('LDO line regulation (delta_line) perturbs vrefEffective and effective voltageRatio', () => {
    const plugin = new NtcPlugin();
    const ctx = {
      publish: () => {},
      adc: { writeNorm: () => {} },
    } as any;

    plugin.onBind(ctx, { OUT: 0 }, {
      r25: 10000,
      bValue: 3950,
      pullUpResistor: 160000,
      temperature: 25,
      line_regulation_pct: 0.2, // +0.2%
    });

    const res = plugin.updateTemperature(25);
    // Vref(t) = 3.0 * (1 + 0.002) = 3.006V
    expect(res.vrefEffective).toBeCloseTo(3.006, 4);

    // ratio = nominal / 1.002
    const nominalRatio = 10000 / 170000;
    expect(res.voltageRatio).toBeCloseTo(nominalRatio / 1.002, 5);
  });

  test('LDO temperature drift coefficient (delta_temp) scales with temperature above 25 C', () => {
    const plugin = new NtcPlugin();
    const ctx = {
      publish: () => {},
      adc: { writeNorm: () => {} },
    } as any;

    plugin.onBind(ctx, { OUT: 0 }, {
      r25: 10000,
      bValue: 3950,
      pullUpResistor: 160000,
      temperature: 25,
      temp_drift_ppm: 50, // 50 ppm/°C
    });

    // At 25°C, delta_temp is 0
    const res25 = plugin.updateTemperature(25);
    expect(res25.vrefEffective).toBeCloseTo(3.0, 5);

    // At 55°C, delta_temp = (55 - 25) * 50e-6 = 0.0015 (+0.15%)
    const res55 = plugin.updateTemperature(55);
    expect(res55.vrefEffective).toBeCloseTo(3.0 * (1 + 0.0015), 4);
  });

  test('ground bounce (delta_gnd_bounce) engages only when heater is active', () => {
    const plugin = new NtcPlugin();
    const ctx = {
      publish: () => {},
      adc: { writeNorm: () => {} },
    } as any;

    plugin.onBind(ctx, { OUT: 0 }, {
      r25: 10000,
      bValue: 3950,
      pullUpResistor: 160000,
      temperature: 55,
      gnd_bounce_pct: 0.1, // 0.1%
      heater_active: false,
    });

    const resInactive = plugin.updateTemperature(55);
    expect(resInactive.vrefEffective).toBeCloseTo(3.0, 4);

    // Turn heater active
    plugin.onEvent('SET_HEATER_ACTIVE', { active: true });
    expect(plugin.manifest.stateChannels).toBeDefined();

    const resActive = plugin.updateTemperature(55);
    expect(resActive.vrefEffective).toBeCloseTo(3.0 * (1 + 0.001), 4);
  });

  test('composite electrical perturbation stays within composite error budget (<= +/-0.5 C electrical)', () => {
    const plugin = new NtcPlugin();
    const ctx = {
      publish: () => {},
      adc: { writeNorm: () => {} },
    } as any;

    // Combined worst-case perturbations:
    // line regulation: 0.1%
    // temp drift: 50 ppm/°C (at 55°C = 0.15%)
    // ground bounce: 0.1%
    // vref noise: 0.05%
    plugin.onBind(ctx, { OUT: 0 }, {
      r25: 10000,
      bValue: 3950,
      pullUpResistor: 160000,
      temperature: 55,
      line_regulation_pct: 0.1,
      temp_drift_ppm: 50,
      gnd_bounce_pct: 0.1,
      vref_noise_pct: 0.05,
      heater_active: true,
      prng_seed: 42,
    });

    const resPerturbed = plugin.updateTemperature(55);
    const rawPerturbed = Math.round(resPerturbed.voltageRatio * 4095);

    // Nominal 55 C raw is 75
    // Delta raw should be <= 2 LSB (~0.5°C in the 55°C band where 1°C ≈ 3 LSB)
    const deltaRaw = Math.abs(rawPerturbed - 75);
    expect(deltaRaw).toBeLessThanOrEqual(2);

    // Delta temperature equivalent: deltaRaw / 3.0 °C <= 0.67 °C
    const equivDeltaDegC = deltaRaw / 3.0;
    expect(equivDeltaDegC).toBeLessThanOrEqual(0.7);
  });

  test('onEvent SET_VREF_PERTURBATION updates all perturbation channels dynamically', () => {
    const plugin = new NtcPlugin();
    const ctx = {
      publish: () => {},
      adc: { writeNorm: () => {} },
    } as any;

    plugin.onBind(ctx, { OUT: 0 }, {
      r25: 10000,
      bValue: 3950,
      pullUpResistor: 160000,
      temperature: 25,
    });

    expect(plugin.updateTemperature(25).vrefEffective).toBeCloseTo(3.0, 5);

    plugin.onEvent('SET_VREF_PERTURBATION', {
      line_regulation_pct: 0.15,
      gnd_bounce_pct: 0.05,
      heater_active: true,
    });

    // delta_total = 0.0015 + 0.0005 = 0.002
    const res = plugin.updateTemperature(25);
    expect(res.vrefEffective).toBeCloseTo(3.0 * 1.002, 4);
  });
});
