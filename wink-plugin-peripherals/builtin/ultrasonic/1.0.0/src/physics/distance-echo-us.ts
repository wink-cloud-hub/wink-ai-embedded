/**
 * Round-trip ultrasonic echo width (µs) from distance (cm) and speed of sound (m/s).
 * HC-SR04: echoUs = round(distanceCm * 2e4 / c); c=343 → 100cm → 5831.
 */
export function distanceCmToEchoUs(distanceCm: number, speedOfSoundMps: number): number {
  const c = speedOfSoundMps || 343;
  return Math.round((distanceCm * 20_000) / c);
}
