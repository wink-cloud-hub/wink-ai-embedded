import { distanceCmToEchoUs } from '../distance-echo-us';

describe('distanceCmToEchoUs', () => {
  test('calculates correct echo width for 100cm', () => {
    expect(distanceCmToEchoUs(100, 343)).toBe(5831);
  });

  test('handles custom speed of sound', () => {
    expect(distanceCmToEchoUs(100, 686)).toBe(2915);
  });

  test('defaults to 343 m/s if zero or empty', () => {
    expect(distanceCmToEchoUs(100, 0)).toBe(5831);
  });
});
