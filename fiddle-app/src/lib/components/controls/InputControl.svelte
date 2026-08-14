<script lang="ts">
import { Check, ChevronDown, Upload } from "lucide-svelte";
import type { FiddleControlDefinition } from "@canvas-wasm-fiddles/canvas-worker";

let {
  control,
  onInput,
}: {
  control: FiddleControlDefinition;
  onInput: (key: string, value: string | File) => void;
} = $props();

// svelte-ignore state_referenced_locally
let value = $state(control.defaultValue || control.options[0] || "");
let fileInput = $state<HTMLInputElement>();
let fileName = $state("");
let fileError = $state("");
let dragging = $state(false);

function commit() {
  onInput(control.key, String(value));
}

function acceptFile(file: File | undefined) {
  if (!file) return;
  if (!file.type.startsWith("image/")) {
    fileError = "Choose a valid image file.";
    fileName = "";
    return;
  }
  fileError = "";
  fileName = file.name;
  onInput(control.key, file);
}
</script>

<details class="control-row" open>
  <summary>
    <span>{control.title}</span>
    <ChevronDown size={17} aria-hidden="true" />
  </summary>
  <div class="control-body">
    {#if control.type === "bool"}
      <label class="checkbox-control">
        <input
          type="checkbox"
          checked={value === "true"}
          onchange={(event) => {
            value = String(event.currentTarget.checked);
            commit();
          }}
        />
        <span>{value === "true" ? "Enabled" : "Disabled"}</span>
      </label>
    {:else if control.type === "option"}
      <select bind:value onchange={commit} aria-label={control.title}>
        {#each control.options as option}
          <option value={option}>{option}</option>
        {/each}
      </select>
    {:else if control.type === "range"}
      <div class="range-control">
        <input
          type="range"
          min={control.min}
          max={control.max}
          step={control.step}
          bind:value
          onchange={commit}
          aria-label={control.title}
        />
        <output>{value}</output>
      </div>
    {:else if control.type === "text"}
      <input
        class="text-control"
        type="text"
        bind:value
        aria-label={control.title}
        onblur={commit}
        onkeydown={(event) => {
          if (event.key === "Enter") {
            event.preventDefault();
            commit();
          }
        }}
      />
    {:else if control.type === "para"}
      <div class="paragraph-control">
        <textarea bind:value aria-label={control.title} rows="4" onblur={commit}></textarea>
        <button type="button" aria-label={`Apply ${control.title}`} title="Apply" onclick={commit}>
          <Check size={17} />
        </button>
      </div>
    {:else if control.type === "image"}
      <button
        type="button"
        class:dragging
        class="image-drop-control"
        onclick={() => fileInput?.click()}
        ondragover={(event) => {
          event.preventDefault();
          dragging = true;
        }}
        ondragleave={() => (dragging = false)}
        ondrop={(event) => {
          event.preventDefault();
          dragging = false;
          acceptFile(event.dataTransfer?.files[0]);
        }}
      >
        <Upload size={20} />
        <strong>Select Image</strong>
        <span>{fileName || "or drop an image here"}</span>
      </button>
      <input
        class="visually-hidden"
        bind:this={fileInput}
        type="file"
        accept="image/*"
        onchange={(event) => acceptFile(event.currentTarget.files?.[0])}
      />
      {#if fileError}<p class="control-error">{fileError}</p>{/if}
    {/if}
  </div>
</details>
