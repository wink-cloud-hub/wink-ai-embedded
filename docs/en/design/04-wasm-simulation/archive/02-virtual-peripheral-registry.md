# 4.2 UniSim Virtual Circuit Specification & SchemaForm Data-Driven Configuration

<!-- i18n-meta
source: docs/zh/design/04-wasm-simulation/archive/02-virtual-peripheral-registry.md
translated: 2026-08-17
glossary-version: v1.0
translator: AI-assisted
sync-status: up-to-date
-->

To support visual dragging, multi-board circuit wiring, and dynamic peripheral property configuration within a low-code platform (fully synchronized with AI generation), the Wink-AI platform introduces the **UniSim (Unified Simulation Project) Virtual Peripheral & Circuit Topology Specification**.

---

## 1. Project Topology & Circuit Schematic Storage Specification (`sim-project.json`)

Circuit netlists and canvas configurations are organized using a flat object model, capturing coordinates, physical parameters, and electrical wire connections across multi-board setups and peripheral components.

### 1.1 Circuit Description Schema Example

```json
{
  "$schema": "https://unisim-spec.org/v1/sim-project.schema.json",
  "version": 1,
  "projectName": "Multi-Board IoT Gateway",
  "boards": [
    {
      "id": "gateway_esp32",
      "type": "board-esp32-s3",
      "x": 100,
      "y": 150,
      "sourceDir": "src/gateway-esp32",
      "settings": {
        "baudRate": 115200,
        "flashSize": 8388608
      }
    },
    {
      "id": "node_nano",
      "type": "board-arduino-nano",
      "x": 500,
      "y": 150,
      "sourceDir": "src/node-nano"
    }
  ],
  "components": [
    {
      "id": "led_status",
      "type": "generic-led",
      "x": 350,
      "y": 280,
      "rotation": 90,
      "properties": {
        "color": "#ff0000",
        "currentLimitResistor": 220
      }
    }
  ],
  "connections": [
    {
      "id": "wire_1",
      "from": "node_nano:D13",
      "to": "led_status:Anode",
      "color": "red",
      "signalType": "digital",
      "routing": {
        "mode": "orthogonal",
        "path": ["v15", "h-30", "*"]
      }
    },
    {
      "id": "wire_2",
      "from": "gateway_esp32:TX0",
      "to": "node_nano:RX",
      "color": "blue",
      "signalType": "uart",
      "routing": {
        "mode": "custom",
        "points": [
          { "x": 180, "y": 190 },
          { "x": 340, "y": 190 },
          { "x": 480, "y": 170 }
        ]
      }
    }
  ]
}
```

---

## 2. Peripheral Metadata Form Design Based on SchemaForm

To minimize manual form authoring, peripheral metadata definitions align natively with the frontend **SchemaForm** specification (based on `@yo-cloud/yo-ux-vue`).

### 2.1 Peripheral Definition Specification Example (`peripheral-definition.json`)
Within peripheral definitions, the `properties` field declares an array of `DynamicItemSchemaType[]`. When a user selects the peripheral on the canvas, the property inspector passes this array directly to `<SchemaForm>` for zero-conversion dynamic rendering.

```json
{
  "$schema": "https://unisim-spec.org/v1/peripheral-definition.schema.json",
  "id": "generic-led",
  "tagName": "wokwi-led",
  "name": {
    "en": "Light Emitting Diode",
    "zh": "发光二极管 (LED)"
  },
  "category": "output",
  "visual": {
    "thumbnail": "<svg width=\"64\" height=\"64\">...</svg>",
    "dimensions": { "width": 24, "height": 36 }
  },
  "pins": [
    { "name": "Anode", "label": "A", "type": "digital_io", "description": "Anode (+)" },
    { "name": "Cathode", "label": "C", "type": "gnd", "description": "Cathode (-)" }
  ],
  "properties": [
    {
      "prop": "color",
      "label": "LED Color",
      "compType": "Select",
      "compProps": {
        "placeholder": "Select illumination color",
        "options": [
          { "label": "Red", "value": "red" },
          { "label": "Green", "value": "green" },
          { "label": "Yellow", "value": "yellow" },
          { "label": "Blue", "value": "blue" }
        ]
      },
      "defaultValue": "red",
      "rules": [{ "required": true, "message": "Color is required", "trigger": "change" }]
    },
    {
      "prop": "currentLimitResistor",
      "label": "Current Limit Resistor (Ω)",
      "compType": "Slider",
      "compProps": {
        "min": 0,
        "max": 10000,
        "step": 10
      },
      "defaultValue": 220
    }
  ]
}
```

### 2.2 Frontend Rendering Integration Code

```vue
<template>
  <el-card class="property-editor-card" shadow="never">
    <template #header>
      <div class="header-title">
        <span>Peripheral Property Configuration ({{ activeComponent.id }})</span>
      </div>
    </template>
    
    <!-- Dynamic Form Component -->
    <SchemaForm
      :schemas="activeComponentMeta.properties"
      v-model:data="activeComponent.properties"
      :form-props="formProps"
      :default-item-span="24"
    />
  </el-card>
</template>

<script setup lang="ts">
import { ref } from 'vue';
import { SchemaForm, type DynamicItemSchemaType } from "@yo-cloud/yo-ux-vue";

const formProps = {
  labelPosition: 'top',
  size: 'default'
};

interface ComponentInstance {
  id: string;
  type: string;
  properties: Record<string, any>;
}

const activeComponent = ref<ComponentInstance>({
  id: "led_status",
  type: "generic-led",
  properties: {
    color: "red",
    currentLimitResistor: 220
  }
});

const activeComponentMeta = ref({
  properties: [
    {
      prop: "color",
      label: "LED Color",
      compType: "Select",
      compProps: {
        options: [
          { label: "Red", value: "red" },
          { label: "Green", value: "green" },
          { label: "Yellow", value: "yellow" },
          { label: "Blue", value: "blue" }
        ]
      }
    },
    {
      prop: "currentLimitResistor",
      label: "Current Limit Resistor (Ω)",
      compType: "Slider",
      compProps: { min: 0, max: 1000, step: 10 }
    }
  ] as DynamicItemSchemaType[]
});
</script>
```

---

## 3. Adaptive Wire Routing Design (Adaptive Routing)

When components are moved across the canvas, wires automatically route around obstacles. The routing engine provides two complementary modes:
1. **Orthogonal (Automatic Manhattan Routing)**: Default mode where wires maintain horizontal and vertical segments. The engine records relative routing tokens and recalculates paths in real time when components move:
   * `v[N]`: Route vertically by N pixels.
   * `h[N]`: Route horizontally by N pixels.
   * `*`: Convergence alignment separator between source and target segments.
2. **Custom (Explicit Breakpoints)**: When users manually drag wire breakpoints (adding handles), the wire switches to `custom` mode, storing an absolute coordinate list `points: {x, y}[]` to preserve manual routing layouts.

---

## 4. Virtual Peripheral Driver Registry Architecture (WasmPeripheralRegistry)

To synchronize Web DOM visual states (e.g. Web Component `<wokwi-led>`) with Wasm logical pin levels, we define a JS/TS virtual peripheral registry. It incorporates a **4-value logic state** and **3-level drive strength** arbitration model to resolve pin conflicts and wired-AND logic.

```typescript
/** 4-value logic states */
export type LogicState = 0 | 1 | 'Z' | 'X';

/** Drive strength levels */
export enum DriveStrength {
  SUPPLY = 3, // Direct power rails (VCC/GND, push-pull outputs)
  PULL   = 2, // Resistor pull-up/down (External I2C pullups, internal MCU pullups)
  WEAK   = 1, // Weak / Hi-Z (Open-drain released state, high-impedance inputs)
}

export interface PeripheralLifecycle {
  /** 
   * Physical power rail / domain binding (e.g. '3V3_SYS', '5V_PERIPHERAL')
   * Automatically isolates power when host MCU shuts down or protection trips
   */
  powerDomain: string;

  /** 
   * Voltage ramp delay from 0V to operating voltage (microseconds)
   * During this window, peripheral returns WINK_ERR_BUSY to all bus operations
   */
  powerUpDelayUs?: number;

  /** Power-on reset callback */
  onPowerOn?: () => Promise<void>;

  /** Power-off / Hot-unplug callback */
  onPowerOff?: () => void;

  /** Soft reset callback */
  onReset?: () => void;

  /** Property mutation callback */
  onPropertyChange?: (key: string, oldValue: any, newValue: any) => void;
}

/**
 * PinArbiter Interface (4-value logic based on drive strength)
 * Supports:
 * - Open-drain / wired-AND behavior (I2C, 1-Wire)
 * - Bus contention detection
 * - High-impedance (Hi-Z) handling
 * - Analog component voltage estimation
 */
export interface PinArbiter {
  readPin(pin: number): LogicState; // 0 | 1 | 'Z' | 'X'
  getResolvedVoltage(pin: number): number;
  onPinChange(pin: number, callback: (pin: number, state: LogicState) => void): () => void;
  setDriver(pin: number, driver: PinDriver): void;
  removeDriver(pin: number, driverId: string): void;
}

/**
 * Pin driver definition
 */
export interface PinDriver {
  /** Unique driver ID */
  id: string;
  /** Current logic state */
  state: LogicState;
  /** Drive strength */
  strength: DriveStrength;
}

export interface PeripheralSimulationLogic extends PeripheralLifecycle {
  /**
   * Invoked when connected pin logic state mutates
   */
  onPinStateChange?: (pinName: string, state: LogicState) => void;

  /**
   * Invoked at simulation startup to bind UI events and relay telemetry back to Wasm
   * @param element Peripheral DOM element
   * @param pinArbiter Pin level arbitration manager
   * @param getMappedPin Resolves logical GPIO pin mapped to a peripheral pin
   * @param componentId Unique instance ID
   */
  attachEvents?: (
    element: HTMLElement,
    pinArbiter: PinArbiter,
    getMappedPin: (partPinName: string) => number | null,
    componentId: string
  ) => () => void; // Returns cleanup teardown function
}

class PeripheralRegistry {
  private registry = new Map<string, PeripheralSimulationLogic>();

  register(type: string, logic: PeripheralSimulationLogic) {
    this.registry.set(type, logic);
  }

  get(type: string) {
    return this.registry.get(type);
  }
}

export const WasmPeripheralRegistry = new PeripheralRegistry();
```

### 4.1 Four Typical Virtual Driver Implementations

#### 1. LED Indicator (Digital Output)
```typescript
WasmPeripheralRegistry.register('generic-led', {
  powerDomain: '3V3_SYS',
  powerUpDelayUs: 0, // LEDs ready instantaneously

  attachEvents: (element, pinManager, getMappedPin, componentId) => {
    const anodePin = getMappedPin('Anode');
    const cathodePin = getMappedPin('Cathode');
    const driverId = `${componentId}:led_drv`;

    // 1. Register component pin impedance (high-impedance input mode)
    if (anodePin !== null) {
      pinManager.setDriver(anodePin, driverId, 'Z', DriveStrength.WEAK);
    }
    if (cathodePin !== null) {
      pinManager.setDriver(cathodePin, driverId, 'Z', DriveStrength.WEAK);
    }

    const updateLed = () => {
      // 2. Compute LED illumination and brightness from resolved voltage drop
      const anodeVoltage = anodePin !== null ? pinManager.getResolvedVoltage(anodePin) : 0;
      const cathodeVoltage = cathodePin !== null ? pinManager.getResolvedVoltage(cathodePin) : 0;
      
      const voltageAcrossLed = Math.max(0, anodeVoltage - cathodeVoltage - 1.8); // 1.8V forward voltage drop
      const brightness = Math.min(1, voltageAcrossLed / 1.5);
      
      (element as any).value = brightness > 0.1;
      (element as any).brightness = brightness;
    };

    let unsubAnode = () => {};
    let unsubCathode = () => {};
    if (anodePin !== null) {
      unsubAnode = pinManager.onPinChange(anodePin, updateLed);
    }
    if (cathodePin !== null) {
      unsubCathode = pinManager.onPinChange(cathodePin, updateLed);
    }

    return () => {
      unsubAnode();
      unsubCathode();
      if (anodePin !== null) pinManager.removeDriver(anodePin, driverId);
      if (cathodePin !== null) pinManager.removeDriver(cathodePin, driverId);
    };
  }
});
```

#### 2. Push Button (Digital Input & Interrupt Triggering)
```typescript
WasmPeripheralRegistry.register('pushbutton', {
  powerDomain: '3V3_SYS',

  attachEvents: (element, pinManager, getMappedPin, componentId) => {
    const gpioPin = getMappedPin('1.l') ?? getMappedPin('2.l');
    if (gpioPin === null) return () => {};
    const driverId = `${componentId}:btn_drv`;

    // Default to Hi-Z when unpressed, governed by pull-up resistors (Active Low wiring)
    pinManager.setDriver(gpioPin, driverId, 'Z', DriveStrength.WEAK);

    const onPress = () => {
      // Pull to ground directly upon press (Level 0 with SUPPLY strength)
      pinManager.setDriver(gpioPin, driverId, 0, DriveStrength.SUPPLY);
      (element as any).pressed = true;
    };
    const onRelease = () => {
      // Revert to weak Hi-Z upon release
      pinManager.setDriver(gpioPin, driverId, 'Z', DriveStrength.WEAK);
      (element as any).pressed = false;
    };

    element.addEventListener('button-press', onPress);
    element.addEventListener('button-release', onRelease);

    return () => {
      element.removeEventListener('button-press', onPress);
      element.removeEventListener('button-release', onRelease);
      pinManager.removeDriver(gpioPin, driverId);
    };
  }
});
```

#### 3. Potentiometer (ADC Analog Input)
```typescript
WasmPeripheralRegistry.register('potentiometer', {
  powerDomain: '3V3_SYS',

  attachEvents: (element, pinManager, getMappedPin, componentId) => {
    const adcPin = getMappedPin('SIG');
    if (adcPin === null) return () => {};
    const driverId = `${componentId}:pot_drv`;

    const onValueChange = (e: Event) => {
      const percent = (e.target as any).value; // Slider ratio 0.0 ~ 1.0
      const simulatedVoltage = percent * 3.3; // Convert to 3.3V ADC reference voltage
      
      pinManager.setAnalogVoltage?.(adcPin, driverId, simulatedVoltage);
    };

    element.addEventListener('input', onValueChange);
    return () => {
      element.removeEventListener('input', onValueChange);
      pinManager.removeDriver?.(adcPin, driverId);
    };
  }
});
```

#### 4. Robotic Servo Joint (PWM Output & WebGL Integration)
```typescript
WasmPeripheralRegistry.register('servo-motor', {
  powerDomain: '5V_PERIPHERAL',
  powerUpDelayUs: 5000, // 5ms capacitor charging stabilization delay

  attachEvents: (element, pinManager, getMappedPin, componentId) => {
    const pwmChannel = getMappedPin('PWM');
    if (pwmChannel === null) return () => {};

    const unsubPwm = pinManager.onPwmChange(pwmChannel, (dutyCycle) => {
      // 0.5ms - 2.5ms duty cycle mapped to 0° - 180° rotation angle
      const minAngle = 0;
      const maxAngle = 180;
      const targetAngle = minAngle + (dutyCycle / 100) * (maxAngle - minAngle);

      // Dispatch event to Three.js canvas to update 3D mesh matrix
      window.dispatchEvent(new CustomEvent('servo-rotate', {
        detail: { componentId, angle: targetAngle }
      }));
    });

    return () => {
      unsubPwm();
    };
  }
});
```

And in Vue 3 3D WebGL viewport:
```javascript
window.addEventListener('servo-rotate', (e) => {
  const { componentId, angle } = e.detail;
  const joint = robot3DModel.findJointById(componentId);
  if (joint) {
     joint.rotation.y = THREE.MathUtils.degToRad(angle);
  }
});
```

### 4.2 Pin Arbitration Architecture (Phase 0)

To accurately simulate real-world circuit dynamics (including open-drain buses, pull-up/down resistors, and contention), UniSim utilizes a **4-value logic arbitration system based on drive strength**.

#### 4.2.1 Core Concepts

**4-Value Logic States:**

| State | Meaning | Voltage |
|---|---|---|
| `0` | Logic Low | 0.0V |
| `1` | Logic High | 3.3V |
| `'Z'` | High Impedance / Floating | 0.0V (Default, component customizable) |
| `'X'` | Contention / Unknown | 1.65V (Midpoint) |

**Drive Strength Levels:**

| Level | Value | Use Cases |
|---|---|---|
| `SUPPLY` | 3 | Push-pull GPIO outputs, direct VCC/GND rails |
| `PULL` | 2 | External I2C pullup/down resistors (4.7kΩ) |
| `WEAK` | 1 | Internal MCU pullups, open-drain released states |

#### 4.2.2 Arbitration Algorithm

1. Drivers in `'Z'` state are ignored (Hi-Z does not drive the net).
2. Determine maximum strength among remaining active drivers.
3. If all maximum-strength drivers share identical states $\rightarrow$ that state resolves as the net level.
4. If maximum-strength drivers present conflicting states $\rightarrow$ `'X'` (Contention, warning logged).
5. If no active drivers remain $\rightarrow$ `'Z'` (Floating).

#### 4.2.3 I2C Wired-AND Example

```typescript
// I2C bus with external pullup resistor
pinArbiter.setDriver(6, {
  id: 'board:i2c-pullup-sda',
  state: 1,
  strength: DriveStrength.PULL
});

// MCU SDA in open-drain mode pulling low
pinArbiter.setDriver(6, {
  id: 'mcu:sda',
  state: 0,
  strength: DriveStrength.SUPPLY
});

pinArbiter.readPin(6); // Returns 0 (Wired-AND: Low level wins)

// MCU releases bus
pinArbiter.setDriver(6, {
  id: 'mcu:sda',
  state: 'Z',
  strength: DriveStrength.SUPPLY
});

pinArbiter.readPin(6); // Returns 1 (Pullup wins)
```

#### 4.2.4 Driver Development Example

```typescript
const updateLed = () => {
  // Query resolved voltage directly for analog brightness estimation
  const voltage = pinArbiter.getResolvedVoltage(anodePin); // 0.0-3.3V
  element.brightness = Math.min(1, Math.max(0, voltage / 3.3));
  
  // Or inspect discrete logic state
  const state = pinArbiter.readPin(anodePin); // 0 | 1 | 'Z' | 'X'
  if (state === 'X') {
    console.warn('Pin contention detected');
  }
};
```
