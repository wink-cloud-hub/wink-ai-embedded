import { describe, expect, test } from 'bun:test';

/**
 * Unit tests verifying the pointer capture, sustained hold,
 * and Ctrl-click sticky gesture state-machine for the Button UI.
 */
describe('Button UI Gesture & Pointer Capture State Machine', () => {
  function createButtonGestureHarness() {
    let isPressed = false;
    let isSticky = false;
    let capturedPointerId: number | null = null;
    const emittedEvents: string[] = [];

    const harness = {
      get isPressed() {
        return isPressed;
      },
      get isSticky() {
        return isSticky;
      },
      get capturedPointerId() {
        return capturedPointerId;
      },
      emittedEvents,

      onPointerDown(e: { button: number; pointerId: number }) {
        if (e.button !== 0) return;
        if (isSticky) {
          isSticky = false;
          isPressed = false;
          emittedEvents.push('buttonRelease');
          return;
        }

        capturedPointerId = e.pointerId;
        isPressed = true;
        emittedEvents.push('buttonPress');
      },

      onPointerUp(e: { pointerId: number; ctrlKey?: boolean; metaKey?: boolean }) {
        if (capturedPointerId === e.pointerId) {
          capturedPointerId = null;
        }

        if (!isPressed) return;

        if (e.ctrlKey || e.metaKey) {
          isSticky = true;
          return;
        }

        isPressed = false;
        emittedEvents.push('buttonRelease');
      },

      onPointerCancel(e: { pointerId: number }) {
        if (capturedPointerId === e.pointerId) {
          capturedPointerId = null;
        }
        if (isPressed && !isSticky) {
          isPressed = false;
          emittedEvents.push('buttonRelease');
        }
      },
    };

    return harness;
  }

  test('normal press and release: emits press on down, release on up', () => {
    const btn = createButtonGestureHarness();

    btn.onPointerDown({ button: 0, pointerId: 1 });
    expect(btn.isPressed).toBe(true);
    expect(btn.capturedPointerId).toBe(1);
    expect(btn.emittedEvents).toEqual(['buttonPress']);

    btn.onPointerUp({ pointerId: 1, ctrlKey: false });
    expect(btn.isPressed).toBe(false);
    expect(btn.capturedPointerId).toBe(null);
    expect(btn.emittedEvents).toEqual(['buttonPress', 'buttonRelease']);
  });

  test('sustained long-press: stays pressed across time without bouncing', () => {
    const btn = createButtonGestureHarness();

    btn.onPointerDown({ button: 0, pointerId: 1 });
    expect(btn.isPressed).toBe(true);

    // Simulate 3 seconds passing (e.g. 3000ms), no events should be emitted
    for (let ms = 0; ms < 3000; ms += 100) {
      expect(btn.isPressed).toBe(true);
      expect(btn.emittedEvents).toEqual(['buttonPress']);
    }

    btn.onPointerUp({ pointerId: 1, ctrlKey: false });
    expect(btn.isPressed).toBe(false);
    expect(btn.emittedEvents).toEqual(['buttonPress', 'buttonRelease']);
  });

  test('ctrl-click latch: stays pressed after mouseup, releases on next click', () => {
    const btn = createButtonGestureHarness();

    // Ctrl + Click down
    btn.onPointerDown({ button: 0, pointerId: 1 });
    expect(btn.isPressed).toBe(true);
    expect(btn.emittedEvents).toEqual(['buttonPress']);

    // Release with Ctrl held
    btn.onPointerUp({ pointerId: 1, ctrlKey: true });
    expect(btn.isSticky).toBe(true);
    expect(btn.isPressed).toBe(true);
    // buttonRelease must NOT have been emitted!
    expect(btn.emittedEvents).toEqual(['buttonPress']);

    // Second click (unlocks sticky)
    btn.onPointerDown({ button: 0, pointerId: 2 });
    expect(btn.isSticky).toBe(false);
    expect(btn.isPressed).toBe(false);
    expect(btn.emittedEvents).toEqual(['buttonPress', 'buttonRelease']);
  });

  test('pointerCancel safely releases button unless sticky', () => {
    const btn = createButtonGestureHarness();

    btn.onPointerDown({ button: 0, pointerId: 1 });
    expect(btn.isPressed).toBe(true);

    btn.onPointerCancel({ pointerId: 1 });
    expect(btn.isPressed).toBe(false);
    expect(btn.emittedEvents).toEqual(['buttonPress', 'buttonRelease']);
  });
});
