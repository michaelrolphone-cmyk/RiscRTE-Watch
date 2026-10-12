# One Home implementation, device-owned composition

Home 0.15.0 builds `default.elf` once. The `clock.elf` compatibility route is the
1,232-byte zero-service return module, not a second copy of Home or its assets.
The Runtime unloads the child and starts the configured default afresh. The
production Runtime test executes Home -> child -> actual return module -> Home,
including separate host loader initialization/finalization and repeated runs.

The default binary is unchanged from shared Home 0.14.2; this version changes
packaging and the return role. It retains the same first-frame, unset-RTC and
optional-service behavior. No device model is compiled into either role.

## Staging

`scripts/stage_product.py` accepts one verified shared build and a device-owned
profile. It copies the same app bytes into either store, preserves application
IDs, all other apps/providers, namespace/file grants and board configuration,
and adds only the explicitly selected generic services. The return role needs
no grants. Unsupported optional services are not invented.

Example (paths are local checkouts/build directories):

```sh
python scripts/stage_product.py --store ../device/build/store \
  --apps dist --services ../drivers/dist/legacy-power \
  --profile ../device/profiles/shared-home.json \
  --provider-capacity 40 --output ../device/build/staged-store
```

The result is an **unsealed store, not a flashable image**. A device assembler
must rebind its native/cohort/bank metadata, run the production Runtime validator
against its actual native export table, and then construct and verify its image.
The stager does not rewrite old user data or modify a live device.

The full T-Watch baseline has 25 applications and 31 provider instances. The
independent key/sleep/display-power adapters bring that to 34, so the 32-slot
native build is insufficient. Its device profile selects the common 40-slot
Runtime target with existing crown recovery. The Waveshare bring-up profile
retains its deliberately smaller capability selection and no automatic sleep.

The 19 composition tests cover different board IDs and physical instance numbers,
byte-identical app reuse, original payload preservation, optional service absence,
provider capacity, duplicate grants, incorrect service functions, package identity,
unsafe paths, immutable inputs, idempotence and creation of a new output only.
The target builder additionally checks the real ELF loader and rejects a return
module with imports or more than 4 KiB of contents.

Physical boot, sleep/current measurements, full Waveshare app/peripheral parity,
and full-height rectangular UI reflow remain separate validation/integration work.
