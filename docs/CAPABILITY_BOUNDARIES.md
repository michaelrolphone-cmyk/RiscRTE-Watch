# Capabilities describe functions, not device models

A capability identifier is a public function contract. Driver ID, compatible chip, physical pins, bus and power-rail binding are implementation details selected by a board profile. Applications discover a function through its generic capability; adding another implementation does not add another application code path.

The existing generic battery contract remains `board.battery@1` and `risc_battery_gauge_api_v1`. There is no X4-, T-Watch- or Waveshare-specific battery capability. Existing application IDs and storage namespaces remain stable; those are not capability identifiers.

## Independent interfaces

| Capability | Function |
|---|---|
| `board.battery@1` | Battery voltage, percentage and status/unknown indication |
| `input.power-key@1` | Logical short/long press events, not GPIO or PMIC bits |
| `power.sleep@1` | Sleep preparation, entry, wake observation and resume |
| `display.output@1` | Dimensions, formats and frame presentation |
| `display.power@1` | Display sleep preparation and restoration |
| `rtc.clock@2` | Civil time and alarms |
| `motion.accel@1` | Acceleration and optional declared wake/tap operations |
| `audio.input@1`, `audio.output@1` | PCM streams independent of transport |

Home 0.13.0 acquires battery, key, sleep and display power independently. A battery implementation needs only the base gauge table. Sleep can operate without a battery. Missing key input produces no key events; missing sleep or display-power service keeps Home awake rather than preventing it from opening. An admitted but incompatible interface is an error, not fabricated support.

The app no longer casts `board.battery` to a T-Watch PMU suffix or `display.output` to a private panel-power suffix. Hardware lookup uses instance zero (unique authorized implementation), never a hard-coded physical instance.

## Compatibility and provisioning

Legacy physical drivers retain their deployed combined tables for older apps. The RiscRTE-Drivers compatibility adapters expose those tables as `power.sleep`, `input.power-key` and `display.power`. These adapters own no hardware; ordinary dependency lifetime keeps the underlying driver mapped. New physical providers can implement each generic interface directly without emulating a battery prefix.

The shared ELF depends on generic contracts, not adapter IDs. Both devices consume identical ELF bytes per application role. The app source manifest describes its complete capability vocabulary; device-generated manifests and grants select the supported subset. The Runtime currently requires each declared manifest requirement to have a matching boot grant, so unsupported optional functions are omitted from the device manifest, not advertised falsely.

Home 0.13.0 needs the three new generic services provisioned to retain key/sleep functions. It is not a drop-in replacement inside an older sealed firmware image: stage the app, adapters and boot manifests before rebuilding paired metadata. Complete device assembly and physical qualification are separate from these component tests.

Old saved tap settings remain a data-compatibility detail. Unknown driver-declared profiles use the validated driver default/range instead of failing a BMA-only whitelist. New persistent formats should be normalized or profile-keyed rather than adding sensor-named fields to shared application policy.

## Verification

Production-client tests cover all 16 service-presence combinations, an exact-size base-only battery allocation, battery-free sleep, malformed interface versions/extents and failed grant release. Production sleep tests cover paired restoration and retained failure. Tap tests include an arbitrary provider profile. Boundary tests distinguish capability IDs from legitimate device-specific driver IDs and reject private PMU/panel casts and fixed physical lookups in active Home code.

Host, ASan/UBSan and target ELF validation are software evidence, not hardware performance or peripheral-parity claims.
