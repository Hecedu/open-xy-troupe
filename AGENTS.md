# Machine development

This repository holds Troupe (formerly Recital; permanent ID `troupe`).
The machine is at the repository root. Its ID, namespace and artwork IDs use
`troupe`; old Recital project mappings do not migrate automatically.
Read README.md and use the [Open-XY SDK](https://github.com/etcetc000/open-xy)
normally checked out beside this repository as `../open-xy`. Read the README's
SDK dependency and review-status notes before running its full checks.

1. Read the [machine API](https://github.com/etcetc000/open-xy/blob/staging/docs/api.md)
   and the relevant [core machines](https://github.com/etcetc000/open-xy-core-machines):
   Karplus or 2op FM for synths, Ring Mod or Ping Pong for effects, Timestretch
   for sample engines. They are the worked examples; this repository holds the
   user's implementation. Preserve licences and attribution when reusing code.
2. Implement through public `<opxy/...>` headers. Keep code, metadata, artwork
   and sound tests here. Do not edit the SDK to make the machine work; explain
   missing interfaces and propose a separate SDK PR against `staging`.
   Contributor code has no assembly, raw firmware addresses, OS headers or
   platform conditionals. Audio callbacks must not allocate, block, log, do I/O
   or call graphics.
3. Run `python ../open-xy/sdk.py test --machine .` and extend `tests/` for the
   machine's promised sound and behavior. Offer browser audition where supported.
   Use the host CPU estimate to guide iteration; it is not a hardware measurement.
4. After local testing and audition, propose hardware validation. Follow the SDK's
   [build guide](https://github.com/etcetc000/open-xy/blob/staging/docs/build.md)
   and [hardware profiling guide](https://github.com/etcetc000/open-xy/blob/staging/docs/profiling.md).
   Prepare a profiling-enabled build from committed source, request a device test
   if not already authorized, and wait for confirmation of installation/reboot.
   Collect three complete runs, generate the report and explain its scope.
   Never invent a CPU number from host checks. If hardware is unavailable, report
   that validation remains pending; a maintainer can run the batch.
5. If publication is requested, follow
   [catalog submission](https://github.com/etcetc000/open-xy/blob/staging/docs/catalog.md#submit-a-machine).
   New or changed draft/published entries require the hardware `cpu` record for
   the exact pinned commit. A new machine revision needs a new measurement.
   The catalog PR targets `staging` and changes only `catalog/machines.json`.

Keep generated outputs outside this repository. Firmware, stock content and
firmware-containing packages must stay outside every checkout, even ignored
folders. Device operations require an explicit user instruction; building and
testing locally do not authorize installing firmware or running diagnostics.

Preserve source contracts, frozen fixture bindings and historical golden values.
Troupe's rename baseline is Recital commit
`e88f57662e22ea5ecab08e25e91239d6b7d94020`, using SDK revision
`1663f60d21c24bf7feafda28f01cd82efdf008d4`. Do not substitute an arbitrary
prototype or infer hardware approval for the new identity from earlier results.
Run the Python machine checks in README.md as well as `sdk.py test` when its
SDK dependencies are available. Follow any enclosing workspace's reference and
hardware-approval policy; keep preserved hardware records immutable and private.
