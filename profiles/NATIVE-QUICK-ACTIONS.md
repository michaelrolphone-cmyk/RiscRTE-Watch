# Explicit Home Quick successor

`build.py --native-quick-actions` requires --clock-edit-authority and its existing
native alarm/time prerequisites. It selects native-quick-actions.json separately
from all prior source profiles, adds PORTABLE_QUICK_CAPABILITY_SAFE and
PORTABLE_QUICK_SAFE_CONTENT, and records native_quick_actions=true. The product
builder maps its --quick-actions / quick_actions receipt to this selection.

The product must separately enable the existing Runtime preference broker and
grant Home storage.preferences API1 namespace1. Merely selecting this client or
its optional manifest dependency does not enable a broker. Existing strong copied
preferences remain selected; no legacy KV fallback or atomic namespace migration.
Settings raw namespace1 writers coordinate through the existing broker revision
contract. Synchronous copied facade does not promise atomic multi-key publication.

Layout uses only provider safe bounds, with full-screen top-edge opening. There
is no implicit circular-display guess. Missing Bluetooth/Contexts stay visibly
unavailable; Wi-Fi policy can operate independently. Airplane is disabled unless
both providers are checked. Wi-Fi policy On does not imply association. Volume
is labeled preference-only for the selected visual-only alarm profile.

Radio-provider bool/status failures retain prior release-then-fail behavior;
this is not proof of hardware custody after an invalid peripheral response.
Copied preference unknown/retained results stop subsequent callbacks as before.
No radio/audio hardware validation or public release is implied.

## Pre-mutation feedback

Selected Quick paints PREVIEW - NOT SAVED during slider edits. Before every user
persistence action (including quick-tap Silent/DND/radios), it completes a
SAVE NOT CONFIRMED frame, then checks bounded input/PWR/exit/alarm cancellation.
No captured modal revision is rebased. Failure to complete presentation retains
custody without writing or speculative display cleanup. Clean cancellation
restores confirmed levels and drops queued edits. Successful acknowledgement
or an ordinary non-retained error permits the next normal frame to clear pending;
unknown/retained results leave the last pending frame untouched. This is not a
promise to repaint an error after authority is uncertain.

A retained unknown may already have persisted. SAVE NOT CONFIRMED deliberately
makes no non-persistence claim; PREVIEW - NOT SAVED is only the pre-write drag.
