import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);
const rootDir = path.resolve(__dirname, '..');

const args = process.argv.slice(2);
const type = args[0];
const version = args[1] || '1.0.0';
const category = args[2] || 'sensor';

if (!type) {
  console.error('\x1b[31m[ERROR] Missing peripheral type name.\x1b[0m');
  console.log('Usage: bun run create <peripheral_type> [version] [category]');
  console.log('Example: bun run create soil_moisture 1.0.0 sensor');
  process.exit(1);
}

if (!/^[a-z][a-z0-9_]*$/.test(type)) {
  console.error('\x1b[31m[ERROR] Invalid type name. Must be snake_case starting with a letter (e.g. "my_sensor").\x1b[0m');
  process.exit(1);
}

const targetDir = path.join(rootDir, 'builtin', type, version);
if (fs.existsSync(targetDir)) {
  console.error(`\x1b[31m[ERROR] Peripheral already exists at: ${targetDir}\x1b[0m`);
  process.exit(1);
}

const srcDir = path.join(targetDir, 'src');
fs.mkdirSync(srcDir, { recursive: true });

const pascalCaseName = type
  .split('_')
  .filter(Boolean)
  .map(w => w.charAt(0).toUpperCase() + w.slice(1))
  .join('');

const displayName = type
  .split('_')
  .filter(Boolean)
  .map(w => w.charAt(0).toUpperCase() + w.slice(1))
  .join(' ');

// 1. vite.config.sim.ts
fs.writeFileSync(
  path.join(targetDir, 'vite.config.sim.ts'),
  `import { definePeripheralSimConfig } from '@wink-ai/unisim-ui/vite';\n\nexport default definePeripheralSimConfig({ type: '${type}' });\n`,
  'utf8'
);

// 2. vite.config.ui.ts
fs.writeFileSync(
  path.join(targetDir, 'vite.config.ui.ts'),
  `import { definePeripheralUiConfig } from '@wink-ai/unisim-ui/vite';\n\nexport default definePeripheralUiConfig({ type: '${type}' });\n`,
  'utf8'
);

// 3. tsconfig.json
fs.writeFileSync(
  path.join(targetDir, 'tsconfig.json'),
  JSON.stringify(
    {
      extends: '../../../tsconfig.json',
      compilerOptions: {
        moduleResolution: 'bundler',
        jsx: 'preserve',
      },
      include: ['src/**/*'],
    },
    null,
    2
  ) + '\n',
  'utf8'
);

// 4. README.md
fs.writeFileSync(
  path.join(targetDir, 'README.md'),
  `# ${displayName} (${type})\n\n- Type: \`${type}\`\n- Version: \`${version}\`\n- Category: \`${category}\`\n\n## Build\n\`\`\`bash\nbun run build\n\`\`\`\n`,
  'utf8'
);

// 5. src/simulation.ts
fs.writeFileSync(
  path.join(srcDir, 'simulation.ts'),
  `import {
  normalizeManifest,
  resolvePluginIdentity,
  BaseSimulationPlugin,
  type PluginContext,
  type PeripheralManifest,
  type ManifestFactory,
} from '@wink-ai/unisim';

const identity = resolvePluginIdentity(import.meta.url, '${type}', '${version}', '${category}');

export interface ${pascalCaseName}Props {
  label?: string;
}

export interface ${pascalCaseName}State {
  active: boolean;
  value: number;
}

export function create${pascalCaseName}Manifest(): PeripheralManifest {
  return normalizeManifest({
    type: identity.type,
    version: identity.version,
    category: identity.category,
    displayName: '${displayName}',
    description: '${displayName} peripheral module',
    timingModel: 'event-driven',
    pins: [
      { name: 'SIG', pinType: 'digital_out', required: true },
      { name: 'VCC', pinType: 'vcc', required: false },
      { name: 'GND', pinType: 'gnd', required: false },
    ],
    properties: {
      label: { type: 'string', default: '${displayName}' },
    },
    stateChannels: {
      active: { type: 'boolean', default: false, description: 'Active status' },
      value: { type: 'number', default: 0, description: 'Sensor reading or actuator value' },
    },
    events: {
      SET_ACTIVE: {
        description: 'Toggle sensor active state',
        params: { active: { type: 'boolean', required: true } },
      },
    },
  });
}

export const ${type}Manifest = create${pascalCaseName}Manifest();
export const ${type}ManifestFactory: ManifestFactory = () => create${pascalCaseName}Manifest();

export class ${pascalCaseName}Plugin extends BaseSimulationPlugin<${pascalCaseName}State, ${pascalCaseName}Props> {
  readonly manifest = ${type}Manifest;
  private active = false;
  private value = 0;

  protected override onBound(
    _ctx: PluginContext<${pascalCaseName}State>,
    _pinMapping: Record<string, number>,
    _props: ${pascalCaseName}Props,
  ): Partial<${pascalCaseName}State> {
    this.active = false;
    this.value = 0;
    this.ctx?.publish('active', false);
    this.ctx?.publish('value', 0);
    this.ctx?.writePin('SIG', false);
    return { active: false, value: 0 };
  }

  /**
   * Manifest SET_ACTIVE event handler.
   * mapEventToMethod automatically strips 'SET_' prefix and camelCases -> _active
   */
  _active(active: boolean): void {
    this.active = active;
    this.ctx?.publish('active', active);
    this.ctx?.writePin('SIG', active);
  }

  onDestroy(): void {
    this.ctx?.releasePin('SIG');
  }
}

export default {
  manifest: ${type}Manifest,
  manifestFactory: ${type}ManifestFactory,
  PluginClass: ${pascalCaseName}Plugin,
};
`,
  'utf8'
);

// 6. src/CanvasGlyph.vue
fs.writeFileSync(
  path.join(srcDir, 'CanvasGlyph.vue'),
  `<script setup lang="ts">
defineProps<{
  active?: boolean;
  value?: number;
  label?: string;
}>();
</script>

<template>
  <div class="glyph-container" :class="{ 'is-active': active }">
    <div class="status-indicator" />
    <span class="glyph-label">{{ label || '${displayName}' }}</span>
  </div>
</template>

<style scoped>
.glyph-container {
  width: 80px;
  height: 60px;
  background: #1e293b;
  border: 1px solid #475569;
  border-radius: 6px;
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  gap: 4px;
  box-sizing: border-box;
  user-select: none;
}
.glyph-container.is-active {
  border-color: #38bdf8;
}
.status-indicator {
  width: 12px;
  height: 12px;
  border-radius: 50%;
  background: #64748b;
  transition: all 0.2s ease;
}
.glyph-container.is-active .status-indicator {
  background: #22c55e;
  box-shadow: 0 0 8px #22c55e;
}
.glyph-label {
  font-size: 11px;
  color: #94a3b8;
  font-weight: 500;
}
</style>
`,
  'utf8'
);

// 7. src/WorldWidget.vue
fs.writeFileSync(
  path.join(srcDir, 'WorldWidget.vue'),
  `<script setup lang="ts">
defineProps<{
  active?: boolean;
  value?: number;
  label?: string;
}>();
</script>

<template>
  <div class="world-widget">
    <div class="world-title">{{ label || '${displayName}' }}</div>
    <div class="world-state">Active: {{ active ? 'YES' : 'NO' }}</div>
  </div>
</template>

<style scoped>
.world-widget {
  padding: 8px 12px;
  background: rgba(15, 23, 42, 0.85);
  border: 1px solid rgba(148, 163, 184, 0.2);
  border-radius: 6px;
  color: #f8fafc;
  font-size: 12px;
}
.world-title {
  font-weight: 600;
  color: #38bdf8;
  margin-bottom: 4px;
}
.world-state {
  color: #94a3b8;
}
</style>
`,
  'utf8'
);

// 8. src/definition.ts
fs.writeFileSync(
  path.join(srcDir, 'definition.ts'),
  `import {
  definePeripheral,
  type PeripheralDefinition,
  type PeripheralPropsSchema,
} from '@wink-ai/unisim-ui';
import { resolvePluginIdentity } from '@wink-ai/unisim';

import CanvasGlyph from './CanvasGlyph.vue';
import WorldWidget from './WorldWidget.vue';

const identity = resolvePluginIdentity(import.meta.url, '${type}', '${version}', '${category}');

const propsSchema: PeripheralPropsSchema = {
  label: { type: 'string', default: '${displayName}', description: 'Display label' },
};

export const ${type}Definition: PeripheralDefinition = definePeripheral({
  type: identity.type,
  size: { width: 80, height: 60 },
  wireColor: '#38bdf8',
  pinsOverlay: {
    SIG: { relX: -5, relY: 20, wireNet: 'primary' },
    VCC: { relX: -5, relY: 35, wireNet: 'vcc', defaultConnection: 'VCC' },
    GND: { relX: -5, relY: 50, wireNet: 'gnd', defaultConnection: 'GND' },
  },
  props: propsSchema,
  canvas: CanvasGlyph,
  world: WorldWidget,
  ui: {
    canvasProps: (comp, ctx) => {
      const ch = ctx?.pluginChannels?.[comp.id] || {};
      return {
        label: comp.props?.label ?? '${displayName}',
        active: Boolean(ch.active),
        value: typeof ch.value === 'number' ? ch.value : 0,
      };
    },
    worldProps: (comp, ctx) => {
      const ch = ctx?.pluginChannels?.[comp.id] || {};
      return {
        label: comp.props?.label ?? '${displayName}',
        active: Boolean(ch.active),
        value: typeof ch.value === 'number' ? ch.value : 0,
      };
    },
  },
});

export default ${type}Definition;
`,
  'utf8'
);

console.log('\n\x1b[32m=========================================================\x1b[0m');
console.log(`\x1b[32m [SUCCESS] Created peripheral skeleton: ${type} (${version})\x1b[0m`);
console.log('\x1b[32m=========================================================\x1b[0m');
console.log(`\x1b[36mLocation: ${targetDir}\x1b[0m\n`);
console.log('\x1b[33mFiles created:\x1b[0m');
console.log('  - vite.config.sim.ts');
console.log('  - vite.config.ui.ts');
console.log('  - tsconfig.json');
console.log('  - README.md');
console.log('  - src/simulation.ts');
console.log('  - src/definition.ts');
console.log('  - src/CanvasGlyph.vue');
console.log('  - src/WorldWidget.vue');
console.log('\n\x1b[37mNext step: run "bun run build" to compile.\x1b[0m\n');
