<script lang="ts">
import type { FiddleControlDefinition } from "@canvas-wasm-fiddles/canvas-worker";
import InputControl from "./InputControl.svelte";

let {
  controls,
  onInput,
}: {
  controls: FiddleControlDefinition[];
  onInput: (key: string, value: string | File) => void;
} = $props();
</script>

{#if controls.length > 0}
  <section class="fiddle-controls" aria-label="Fiddle controls">
    <h3>Controls</h3>
    <div class="control-list">
      {#each controls as control (control.key)}
        <InputControl {control} {onInput} />
      {/each}
    </div>
  </section>
{/if}
