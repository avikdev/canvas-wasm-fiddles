# Google Analytics setup

The site loads the GA4 Google tag only in production builds that define a
valid `VITE_GA_MEASUREMENT_ID` such as `G-XXXXXXXXXX`. The measurement ID is a
public browser identifier, not a secret.

For a local production build:

```sh
VITE_GA_MEASUREMENT_ID=G-XXXXXXXXXX bun run build
```

For Vercel, add `VITE_GA_MEASUREMENT_ID` to the Production environment, pull
the environment, and rebuild the prebuilt deployment as described in
`docs/vercel-deploy.md`.

## Collected events

| Event | Parameters | Meaning |
| --- | --- | --- |
| `page_view` | `fiddle_key`, virtual page fields | A fiddle became active. The virtual path ends in `/fiddles/<fiddle-key>`. |
| `fiddle_view` | `fiddle_key`, `selection_method` | A fiddle became active, either automatically or through user input. |
| `fiddle_select` | `fiddle_key` | The user clicked a fiddle in the navigation. |
| `fiddle_engagement` | `fiddle_key`, `engagement_reason`, `engagement_time_msec`, `fiddle_time_msec` | Visible-tab time attributed to a fiddle. |
| `save_svg` | `fiddle_key` | The user invoked Save SVG. |
| `save_svg_failure` | `fiddle_key` | An invoked SVG export failed. |

GA4 automatically collects acquisition data such as source, medium, campaign,
and the original external referrer through the standard Google tag.

## One-time GA4 report configuration

In **Admin → Data display → Custom definitions**, create:

1. An event-scoped custom dimension named **Fiddle key** using event parameter
   `fiddle_key`.
2. An event-scoped custom dimension named **Selection method** using event
   parameter `selection_method`.
3. A custom metric named **Fiddle time** using event parameter
   `fiddle_time_msec`, with milliseconds as its unit.

The standard **Reports → Engagement → Pages and screens** report will show one
virtual page per fiddle. For a dedicated fiddle report, create a free-form
Exploration with **Fiddle key** as rows and metrics such as Active users, Event
count, and Fiddle time. Filter by `fiddle_view`, `fiddle_select`, or `save_svg`
when the report should cover one action only. GA4 custom definitions can take
24–48 hours to appear in reports after collection begins.
