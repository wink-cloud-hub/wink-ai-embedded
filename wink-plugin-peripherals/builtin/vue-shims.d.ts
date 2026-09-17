// Ambient module declarations for the peripheral suite typecheck.
//
// Vue SFCs are compiled by the vite/vue plugins at build time; `tsc --noEmit`
// (the no-engine sandbox gate) only needs a default-export shim for the
// component imports used by `definition.ts`.
declare module '*.vue' {
  import type { DefineComponent } from 'vue';

  const component: DefineComponent<Record<string, unknown>, Record<string, unknown>, unknown>;
  export default component;
}
