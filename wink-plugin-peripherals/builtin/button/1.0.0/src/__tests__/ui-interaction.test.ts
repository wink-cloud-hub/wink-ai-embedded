import { describe, expect, test } from 'bun:test';

/**
 * Unit tests verifying the pointer capture, sustained hold,
 * and Ctrl-click sticky gesture state-machine for the Button UI.
 */
describe('Button UI Gesture & Pointer Capture State Machine', () => {
  function createButtonGestureHarness(options: { readonly?: boolean } = {}) {
    let isPressed = false;
    let isSticky = false;
    let capturedPointerId: number | null = null;
    const emittedEvents: string[] = [];
    let preventDefaultCalled = false;
    let stopPropagationCalled = false;

    const readonly = options.readonly ?? true;

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
      get preventDefaultCalled() {
        return preventDefaultCalled;
      },
      get stopPropagationCalled() {
        return stopPropagationCalled;
      },
      emittedEvents,

      onPointerDown(e: {
        button: number;
        pointerId: number;
        preventDefault?: () => void;
        stopPropagation?: () => void;
      }) {
        if (e.button !== 0) return;
        if (!readonly) return;

        if (e.preventDefault) {
          e.preventDefault();
          preventDefaultCalled = true;
        }
        if (e.stopPropagation) {
          e.stopPropagation();
          stopPropagationCalled = true;
        }

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

      onPointerUp(e: {
        pointerId: number;
        ctrlKey?: boolean;
        metaKey?: boolean;
        stopPropagation?: () => void;
      }) {
        if (capturedPointerId === e.pointerId) {
          capturedPointerId = null;
        }

        if (!isPressed) return;

        if (e.stopPropagation) {
          e.stopPropagation();
        }

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

  test('normal press and release: emits press on down, release on up, prevents default', () => {
    const btn = createButtonGestureHarness();
    let pd = false;
    let sp = false;

    btn.onPointerDown({
      button: 0,
      pointerId: 1,
      preventDefault: () => {
        pd = true;
      },
      stopPropagation: () => {
        sp = true;
      },
    });
    expect(btn.isPressed).toBe(true);
    expect(btn.capturedPointerId).toBe(1);
    expect(pd).toBe(true);
    expect(sp).toBe(true);
    expect(btn.emittedEvents).toEqual(['buttonPress']);

    btn.onPointerUp({ pointerId: 1, ctrlKey: false });
    expect(btn.isPressed).toBe(false);
    expect(btn.capturedPointerId).toBe(null);
    expect(btn.emittedEvents).toEqual(['buttonPress', 'buttonRelease']);
  });

  test('readonly=false (edit mode): pointerdown is ignored so canvas drag can take over', () => {
    const btn = createButtonGestureHarness({ readonly: false });
    let pd = false;

    btn.onPointerDown({
      button: 0,
      pointerId: 1,
      preventDefault: () => {
        pd = true;
      },
    });
    expect(btn.isPressed).toBe(false);
    expect(pd).toBe(false);
    expect(btn.capturedPointerId).toBe(null);
    expect(btn.emittedEvents).toEqual([]);
  });

  test('sustained long-press: stays pressed across time without bouncing or dropping', () => {
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
