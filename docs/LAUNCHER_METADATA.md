# Shared Home launcher metadata

The active shared `default.json` names the application **Home** and explicitly
sets `launcher_visible: false`. The persisted identity remains `twatch-clock`,
the executable remains `default.elf`, and the configured default application,
settings namespaces, capabilities, routes, serial behavior and ELF code are not
renamed or migrated. The shared README has always identified this role as Home;
existing product composition also hides default/springboard implementations.

This fixes a verified metadata omission, not a claimed hardware reproduction.
Runtime initializes visibility to true and falls back from absent `display_name`
to `id`. Both the exact Waveshare 0.1.64 and T-Watch 1.1.46 stores therefore expose
`default.elf` as `twatch-clock`. The T-Watch additionally exposes its explicitly
named Clock shortcut. Waveshare deliberately hides that return shim. Neither
historical artifact is rewritten by this change.

## Preserve product-owned presentation and authority

`stage_product.py` copies only the new default's explicit `display_name` and
`launcher_visible` presentation fields. Its existing version/capability staging
behavior is otherwise unchanged. Every other existing manifest field survives,
including optional capabilities, icons, descriptions, lifecycle, supported file
types and minimum firmware. Older bundles without presentation fields retain the
product's existing values.

The `twatch-clock-return` ID, `clock.elf` path, zero-service source and shared
Clock template are unchanged. The device's existing Clock name, icon and
visibility remain authoritative: true on T-Watch 1.1.46 and false on Waveshare
0.1.64. The stager does not turn the hidden shim back on. Its report now includes
SHA-256 digests of both exact staged role manifests.

New candidate manifest identities are 0.19.4 (ordinary), 0.20.5 (automatic
provisioning) and 0.20.6 (serial transfer). These version increments identify the
new metadata; they do not change product pins or introduce a general version
policy. Independent builds from base `401fcfd360825ec1cf4ad89e736093aa6ac4c5cc`
and the candidate have byte-identical ELFs in all three modes. The transfer ELF
also matches the delivered 0.1.64 Home SHA-256 exactly. Full hashes are in
`evidence/launcher-metadata/qualification.json`.

## Qualification

Run the focused contract and real Runtime presentation checks:

```sh
python3 tests/test_build_inputs.py
python3 tests/test_stage_product.py
python3 tests/test_launcher_metadata_contract.py
python3 scripts/test_launcher_metadata.py --runtime ../RiscRTE \
  --artifacts dist --output /tmp/launcher-proof --sanitize
```

For an extracted, unsealed store, retain the composition report and bind the
probe to those exact staged manifest and ELF digests:

```sh
python3 scripts/stage_product.py --store extracted-store --apps dist \
  --profile device-profile.json --provider-capacity 40 --output staged-store \
  > stage-receipt.json
python3 scripts/test_launcher_metadata.py --runtime ../RiscRTE \
  --artifacts dist --store staged-store --stage-receipt stage-receipt.json \
  --output /tmp/staged-launcher-proof --sanitize
```

A stage manifest is intentionally not required to equal the raw app manifest:
product grants and retained fields can differ. The probe checks its stage-derived
digest instead, and does not replace it with a stripped shared manifest.

The probe compiles unchanged production `Runtime::appCatalogMetadata` and
`Runtime::appCatalogRefresh`, then uses test-only private access to seed admitted
policy slots from the actual manifests. It exercises production name fallback,
visibility normalization and catalog selection. It does **not** test native
policy admission, provider grants, target instruction execution, device display
rendering, or physical hardware. No admission rules are weakened. The existing
production Runtime return-module lifecycle test separately passes.

The source template and all three actual generated candidates are checked.
Negative controls remove presentation, mutate identity/visibility, drop supported
fields, substitute a version/ELF, and alter stage-derived manifests. The legacy
negative control produces a visible `twatch-clock`; the corrected candidate does
not. Exact staged inventories from both released product stores change from 24
to 23 visible entries, with only `twatch-clock` removed and each Clock visibility
preserved. These are host composition candidates, not sealed or flashable builds.

The normal and ASan/UBSan checks passed. Local LeakSanitizer cannot run under the
executor's ptrace boundary; the initial failures were retained, and the local
sanitized runs used `ASAN_OPTIONS=detect_leaks=0`. CI keeps normal leak detection.
The full Python discovery suite retains one pre-existing failure in
`test_shared_default_proof.ProofTests.test_backend_manifest`: it expects the old
error text `backend-specific contract`, while the verifier reports
`Optional removable-volume declaration differs from selected provisioning feature`.
That same assertion fails on the exact base commit; this patch does not change it.

No product pin, delivered image, release, held Runtime PR, hardware test, or
native product fixture is changed. Publication/CI and physical qualification are
separate from this local evidence.
