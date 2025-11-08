---
id: REL-CIL-001
phase: 05-implementation
status: draft
schema: internal/cil/v1
related:
  sfmea: REL-SFMEA-001
revision:
  created: 2025-11-08
  updated: 2025-11-08
  version: 0.1.0
---

# Critical Items List (CIL) – IEEE 1588-2019 PTP Library

Source SFMEA: `sfmea-ptp-initial.md` (REL-SFMEA-001). Inclusion threshold RPN ≥ 170.

| FM ID | Description (Summary) | RPN | Mitigation Actions | Owner | Target Date | Status |
|-------|-----------------------|-----|--------------------|-------|-------------|--------|
| FM-001 | Incorrect offset sign | 192 | Add timestamp ordering assertions + reversed timestamps test | impl | 2025-11-10 | open |
| FM-002 | Scaled arithmetic overflow | 189 | Clamp + log + validation counters (implemented) add overflow test | impl | 2025-11-09 | in-progress |
| FM-004 | Tie token misuse selecting wrong master | 210 | Distinct telemetry/log (implemented), add forced tie post-check test | impl | 2025-11-09 | done |
| FM-007 | Stale health report emission gap | 168* | Heartbeat emit in tick + observer presence test | impl | 2025-11-12 | open |
| FM-008 | Over-permissive sync heuristic | 180 | Add min offset count threshold & validations gating | impl | 2025-11-13 | open |
| FM-014 | Integer division rounding bias | 192 | Add rounding characterization test & optional compensation toggle | impl | 2025-11-14 | open |
| FM-018 | Foreign master list overflow | 168* | Bounds check + overflow counter + test | impl | 2025-11-11 | open |
| FM-019 | Forced tie not telemetry-tagged | 175 | Implemented distinct log + health flag (done) add observer test | impl | 2025-11-09 | in-progress |

*Included despite RPN < 170 due to reliability impact and operational frequency (engineering judgment override).

## Mitigation Tracking Notes

- Validation counters now increment on offset clamp (FM-002) and BMCA forced tie (FM-004/FM-019) – need tests asserting counters.
- Health report extended with `bmcaTieForcedLast` – add test to assert true when tie consumed.
- Next patch: introduce ordering assertions for T1<T2 and T3<T4 in synchronization path (FM-001).
- Heartbeat emission: propose `health::emit()` call in periodic `tick()` after metrics snapshot (FM-007).

## Next Steps

1. Implement remaining mitigations by target dates.
2. Add new unit tests referencing FM IDs in test header comments for traceability.
3. Update SFMEA residual RPN after each mitigation.
4. Promote CIL to `status: approved` after review.

---
