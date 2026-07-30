<script lang="ts">
import { cn } from "../../../utils";

let {
  orientation = "horizontal",
  class: className,
}: {
  orientation?: "horizontal" | "vertical";
  class?: string;
} = $props();
</script>

<div
  aria-hidden="true"
  data-orientation={orientation}
  class={cn(
    "shrink-0 bg-border",
    orientation === "horizontal" ? "h-px w-full" : "h-full w-px",
    className,
  )}
></div>
