# Deploy to Vercel from a local build

This project uses Vercel's prebuilt deployment flow. Vercel receives the output
created locally in `.vercel/output`; it does not need access to the Git
repository.

## One-time setup

Run all commands from the repository root.

1. Install dependencies, sign in to Vercel, and create a project:

   ```sh
   bun install
   bunx vercel login
   bunx vercel project add
   ```

   When prompted, choose the desired account or team and name the project, for
   example `amber-fiddles`. Do not connect a Git repository.

2. Link this local checkout to the new project:

   ```sh
   bunx vercel link
   ```

   Select the project created above. This writes local project metadata under
   `.vercel/`, which is ignored by Git. The committed `vercel.json` supplies
   the build command (`bun run build:vercel`) and output directory
   (`fiddle-app/dist`).

3. Add production environment variables or secrets, if needed:

   ```sh
   bunx vercel env add API_SECRET production --sensitive
   ```

   The CLI prompts for the value, keeping it out of shell history. Repeat the
   command for each secret. Use `vercel env add NAME production` without
   `--sensitive` for a normal variable.

   This is a static browser application. A value named with Vite's `VITE_`
   prefix and read through `import.meta.env` is embedded in the JavaScript
   bundle and is visible to visitors, so it must not contain a secret. A true
   secret must only be consumed by server-side code, such as a Vercel Function;
   the current app has no server-side runtime.

   To enable Google Analytics, add the public GA4 measurement ID and follow
   the report setup in `docs/google-analytics.md`:

   ```sh
   bunx vercel env add VITE_GA_MEASUREMENT_ID production
   ```

## Build and deploy

Pull the latest production settings and environment variables, build on the
local machine, then upload only the prebuilt output:

```sh
bunx vercel pull --yes --environment=production
bunx vercel build --prod
bunx vercel deploy --prebuilt --prod
```

`vercel build --prod` creates `.vercel/output`. The production deploy command
uploads that directory without sending the repository source through Vercel's
remote build system.

For a preview deployment, omit `--prod` consistently:

```sh
bunx vercel pull --yes --environment=preview
bunx vercel build
bunx vercel deploy --prebuilt
```

## Add `fiddles.amberplot.com`

Associate the subdomain with the Vercel project, replacing the example project
name if necessary:

```sh
bunx vercel domains add fiddles.amberplot.com amber-fiddles
bunx vercel domains inspect fiddles.amberplot.com
```

At the DNS provider for `amberplot.com`, create the CNAME record reported by
`vercel domains inspect` (the host will normally be `fiddles`). Use the exact
target Vercel reports rather than copying a possibly stale value from this
document. Run the inspect command again after DNS propagates; Vercel provisions
TLS automatically after the domain verifies.

Once assigned to the project, the custom domain follows the latest production
deployment.

## Repeat-deploy checklist

```sh
bun install
bunx vercel pull --yes --environment=production
bunx vercel build --prod
bunx vercel deploy --prebuilt --prod
```

If an environment variable changes, pull again and rebuild: variables used at
build time do not modify an already-created deployment.

## Vercel references

- [Deploying from a local build](https://vercel.com/docs/cli/deploying-from-cli#deploying-from-local-build-prebuilt)
- [`vercel build`](https://vercel.com/docs/cli/build)
- [Managing environment variables](https://vercel.com/docs/cli/env)
- [Setting up a custom domain](https://vercel.com/docs/domains/set-up-custom-domain)
