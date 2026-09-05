/**
 * 7-segment font tables and decoder functions.
 * Bit mapping (0-7):
 * bit 0 (0x01): A (top horizontal)
 * bit 1 (0x02): B (top-right vertical)
 * bit 2 (0x04): C (bottom-right vertical)
 * bit 3 (0x08): D (bottom horizontal)
 * bit 4 (0x10): E (bottom-left vertical)
 * bit 5 (0x20): F (top-left vertical)
 * bit 6 (0x40): G (center horizontal)
 * bit 7 (0x80): DP (decimal point)
 */

export const SEG_BIT = Object.freeze({
  A: 1 << 0,
  B: 1 << 1,
  C: 1 << 2,
  D: 1 << 3,
  E: 1 << 4,
  F: 1 << 5,
  G: 1 << 6,
  DP: 1 << 7,
});

export const CHAR_TO_SEG_MASK: Readonly<Record<string, number>> = Object.freeze({
  '0': SEG_BIT.A | SEG_BIT.B | SEG_BIT.C | SEG_BIT.D | SEG_BIT.E | SEG_BIT.F, // 0x3F
  '1': SEG_BIT.B | SEG_BIT.C, // 0x06
  '2': SEG_BIT.A | SEG_BIT.B | SEG_BIT.D | SEG_BIT.E | SEG_BIT.G, // 0x5B
  '3': SEG_BIT.A | SEG_BIT.B | SEG_BIT.C | SEG_BIT.D | SEG_BIT.G, // 0x4F
  '4': SEG_BIT.B | SEG_BIT.C | SEG_BIT.F | SEG_BIT.G, // 0x66
  '5': SEG_BIT.A | SEG_BIT.C | SEG_BIT.D | SEG_BIT.F | SEG_BIT.G, // 0x6D
  '6': SEG_BIT.A | SEG_BIT.C | SEG_BIT.D | SEG_BIT.E | SEG_BIT.F | SEG_BIT.G, // 0x7D
  '7': SEG_BIT.A | SEG_BIT.B | SEG_BIT.C, // 0x07
  '8': SEG_BIT.A | SEG_BIT.B | SEG_BIT.C | SEG_BIT.D | SEG_BIT.E | SEG_BIT.F | SEG_BIT.G, // 0x7F
  '9': SEG_BIT.A | SEG_BIT.B | SEG_BIT.C | SEG_BIT.D | SEG_BIT.F | SEG_BIT.G, // 0x6F
  'A': SEG_BIT.A | SEG_BIT.B | SEG_BIT.C | SEG_BIT.E | SEG_BIT.F | SEG_BIT.G, // 0x77
  'a': SEG_BIT.A | SEG_BIT.B | SEG_BIT.C | SEG_BIT.E | SEG_BIT.F | SEG_BIT.G, // 0x77
  'B': SEG_BIT.C | SEG_BIT.D | SEG_BIT.E | SEG_BIT.F | SEG_BIT.G, // 0x7C ('b')
  'b': SEG_BIT.C | SEG_BIT.D | SEG_BIT.E | SEG_BIT.F | SEG_BIT.G, // 0x7C
  'C': SEG_BIT.A | SEG_BIT.D | SEG_BIT.E | SEG_BIT.F, // 0x39
  'c': SEG_BIT.D | SEG_BIT.E | SEG_BIT.G, // 0x58
  'D': SEG_BIT.B | SEG_BIT.C | SEG_BIT.D | SEG_BIT.E | SEG_BIT.G, // 0x5E ('d')
  'd': SEG_BIT.B | SEG_BIT.C | SEG_BIT.D | SEG_BIT.E | SEG_BIT.G, // 0x5E
  'E': SEG_BIT.A | SEG_BIT.D | SEG_BIT.E | SEG_BIT.F | SEG_BIT.G, // 0x79
  'e': SEG_BIT.A | SEG_BIT.D | SEG_BIT.E | SEG_BIT.F | SEG_BIT.G, // 0x79
  'F': SEG_BIT.A | SEG_BIT.E | SEG_BIT.F | SEG_BIT.G, // 0x71
  'f': SEG_BIT.A | SEG_BIT.E | SEG_BIT.F | SEG_BIT.G, // 0x71
  'H': SEG_BIT.B | SEG_BIT.C | SEG_BIT.E | SEG_BIT.F | SEG_BIT.G, // 0x76
  'h': SEG_BIT.C | SEG_BIT.E | SEG_BIT.F | SEG_BIT.G, // 0x74
  'L': SEG_BIT.D | SEG_BIT.E | SEG_BIT.F, // 0x38
  'l': SEG_BIT.D | SEG_BIT.E | SEG_BIT.F, // 0x38
  'n': SEG_BIT.C | SEG_BIT.E | SEG_BIT.G, // 0x54
  'N': SEG_BIT.A | SEG_BIT.B | SEG_BIT.C | SEG_BIT.E | SEG_BIT.F, // 0x37
  'O': SEG_BIT.A | SEG_BIT.B | SEG_BIT.C | SEG_BIT.D | SEG_BIT.E | SEG_BIT.F, // 0x3F ('0')
  'o': SEG_BIT.C | SEG_BIT.D | SEG_BIT.E | SEG_BIT.G, // 0x5C
  'P': SEG_BIT.A | SEG_BIT.B | SEG_BIT.E | SEG_BIT.F | SEG_BIT.G, // 0x73
  'p': SEG_BIT.A | SEG_BIT.B | SEG_BIT.E | SEG_BIT.F | SEG_BIT.G, // 0x73
  'r': SEG_BIT.E | SEG_BIT.G, // 0x50
  'R': SEG_BIT.E | SEG_BIT.G, // 0x50
  't': SEG_BIT.D | SEG_BIT.E | SEG_BIT.F | SEG_BIT.G, // 0x78
  'T': SEG_BIT.D | SEG_BIT.E | SEG_BIT.F | SEG_BIT.G, // 0x78
  'U': SEG_BIT.B | SEG_BIT.C | SEG_BIT.D | SEG_BIT.E | SEG_BIT.F, // 0x3E
  'u': SEG_BIT.C | SEG_BIT.D | SEG_BIT.E, // 0x1C
  '-': SEG_BIT.G, // 0x40
  '_': SEG_BIT.D, // 0x08
  ' ': 0x00,
});

const SEG_MASK_TO_CHAR = new Map<number, string>();
// Populate canonical mappings
SEG_MASK_TO_CHAR.set(0x00, ' ');
SEG_MASK_TO_CHAR.set(0x3F, '0');
SEG_MASK_TO_CHAR.set(0x06, '1');
SEG_MASK_TO_CHAR.set(0x5B, '2');
SEG_MASK_TO_CHAR.set(0x4F, '3');
SEG_MASK_TO_CHAR.set(0x66, '4');
SEG_MASK_TO_CHAR.set(0x6D, '5');
SEG_MASK_TO_CHAR.set(0x7D, '6');
SEG_MASK_TO_CHAR.set(0x07, '7');
SEG_MASK_TO_CHAR.set(0x7F, '8');
SEG_MASK_TO_CHAR.set(0x6F, '9');
SEG_MASK_TO_CHAR.set(0x77, 'A');
SEG_MASK_TO_CHAR.set(0x7C, 'b');
SEG_MASK_TO_CHAR.set(0x39, 'C');
SEG_MASK_TO_CHAR.set(0x58, 'c');
SEG_MASK_TO_CHAR.set(0x5E, 'd');
SEG_MASK_TO_CHAR.set(0x79, 'E');
SEG_MASK_TO_CHAR.set(0x71, 'F');
SEG_MASK_TO_CHAR.set(0x76, 'H');
SEG_MASK_TO_CHAR.set(0x74, 'h');
SEG_MASK_TO_CHAR.set(0x38, 'L');
SEG_MASK_TO_CHAR.set(0x54, 'n');
SEG_MASK_TO_CHAR.set(0x5C, 'o');
SEG_MASK_TO_CHAR.set(0x73, 'P');
SEG_MASK_TO_CHAR.set(0x50, 'r');
SEG_MASK_TO_CHAR.set(0x78, 't');
SEG_MASK_TO_CHAR.set(0x3E, 'U');
SEG_MASK_TO_CHAR.set(0x1C, 'u');
SEG_MASK_TO_CHAR.set(0x40, '-');
SEG_MASK_TO_CHAR.set(0x08, '_');

/**
 * Decode a 7-segment segment mask (bit0=A..bit6=G, bit7=DP) to a character.
 * Unknown segment combinations return '?'.
 */
export function decodeSegMask(mask: number): string {
  const glyphMask = mask & 0x7f;
  return SEG_MASK_TO_CHAR.get(glyphMask) ?? '?';
}
