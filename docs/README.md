# Documentation

Engineering documentation for [Desk-Buddy](../README.md).

| Document | Covers | Subject status |
|---|---|---|
| [architecture.md](architecture.md) | System diagram, firmware layers, control flow, scheduling model, state ownership, extension points | Mixed — marked per item |
| [hardware.md](hardware.md) | Bill of materials, XIAO ESP32C3 GPIO constraints, panel and module properties, power budget, assembly notes | Mixed — marked per part |
| [wiring.md](wiring.md) | Power distribution, connection tables per subsystem, pin allocation, what has been tested together, bring-up checklist | All three subsystems **verified individually**; **no two verified operating together** |
| [display.md](display.md) | Panel configuration, face geometry, animation architecture, blink state machine, timing | **Implemented and verified** |
| [audio.md](audio.md) | Hardware verification record, DFPlayer selection rationale, microSD layout, UART protocol, proposed interface, integration plan, risks | Standalone playback **verified**; **not integrated** into the firmware |
| [troubleshooting.md](troubleshooting.md) | Incident log and fault symptom tables — including the reversed-polarity incident that destroyed a DFPlayer Mini | Record |
| [project-roadmap.md](project-roadmap.md) | V1 / V1.1 / V2 phases, exit criteria, backlog, non-goals, dependency graph | Plan |
| [media/](media/README.md) | Photos and animation GIFs | None captured yet |

## Reading order

- **Reproducing the build:** [hardware.md](hardware.md) → [wiring.md](wiring.md)
  → the README's setup section. Read the rail-polarity warning at the top of
  wiring.md first; skipping it cost this project a DFPlayer Mini.
- **Something is broken:** [troubleshooting.md](troubleshooting.md).
- **Understanding the firmware:** [architecture.md](architecture.md) →
  [display.md](display.md).
- **Continuing the work:** [project-roadmap.md](project-roadmap.md) →
  [audio.md](audio.md).

## Documentation conventions

- Every claim about hardware behaviour is marked as **verified on hardware**,
  **in progress**, or **planned**. Nothing unbuilt is described in the present
  tense.
- Figures that were calculated rather than measured say so explicitly — the
  SPI transfer times in [display.md](display.md) and the current draws in
  [hardware.md](hardware.md) are both estimates.
- Design decisions are recorded with their costs, not only their
  justifications.
