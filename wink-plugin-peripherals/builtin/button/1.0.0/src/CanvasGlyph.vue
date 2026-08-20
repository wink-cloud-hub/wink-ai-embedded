<script setup lang="ts">
import '@wokwi/elements';

defineProps<{
  color: string;
  label: string;
  xray: boolean;
}>();

defineEmits<{
  buttonPress: [];
  buttonRelease: [];
}>();
</script>

<template>
  <wokwi-pushbutton
    :color="color"
    :label="label"
    :xray="xray"
    @button-press="$emit('buttonPress')"
    @button-release="$emit('buttonRelease')"
  />
</template>
