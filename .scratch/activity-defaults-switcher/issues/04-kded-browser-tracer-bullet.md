# 04: kded6 module — browser tracer bullet

**What to build:** An auto-loaded kded6 module that wires activity changes through the resolver and applies the result for the browser component only, using the writer (03). Proves the full kded → resolve → apply path end to end before scaling to all components.

**Blocked by:** 01 (Resolver + domain model), 02 (Activity identity), 03 (Writer).

**Status:** done

- [x] Module auto-loads under kded6 at session start.
- [x] Switching activities flips the browser default to the current activity's binding.
- [x] Falls back to the baseline activity's browser binding when the activity has none.
- [x] Uses the resolver from 01 (does not re-implement resolution).
- [x] One verifiable end-to-end check: change activity → browser default changes.

**Polish (from review of 02, in this order):**
- [x] Introduce `using ActivityId = QString;` and use it for `Activity::id` and the profiles map key, so "keyed by id" is explicit in the types.
- [x] Drop the now-unused id-only `Activity::operator==` unless a caller needs it.
- [x] Optionally strengthen `componentWithoutDecisionAbsentFromNonEmptyResult` to assert every non-resolved component is absent, not just Email and Terminal.
