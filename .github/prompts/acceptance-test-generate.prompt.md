---
mode: agent
description: "Generate an acceptance testing plan, design, cases, procedures, automation strategy, and traceability from stakeholder-defined acceptance criteria. Validates intended use and user needs (Validation)."
---

# ✅ Acceptance Test Generation Prompt

Use this prompt to create a complete, automated, and traceable set of acceptance tests that validate the system against stakeholder-defined acceptance criteria. Acceptance tests are customer-owned, written before code, automated, end-to-end where feasible, and fully traceable.

Standards and practices:
- IEEE 1012-2016 (Verification & Validation)
- ISO/IEC/IEEE 12207:2017 (Life cycle processes)
- ISO/IEC/IEEE 29148:2018 (Requirements)
- IEEE 829-2008 / ISO/IEC/IEEE 29119 (Test documentation)
- XP Acceptance Test practices (customer tests, Test-First)

---
## 📤 EXPECTED OUTPUT

Produce a single markdown deliverable named: `AT-[Project]-v[Major.Minor]-[YYYYMMDD].md` containing the sections below.

```
# Acceptance Test Plan (ATP)
Project: [Name]
Version: [Major.Minor]
Date: [YYYY-MM-DD]
Integrity Level: [1–4]
Owners: Customer/Acquirer: [Name], QA Lead: [Name]
Status: Draft / Review / Approved

## 1. Purpose and Scope
- Validation objective and out-of-scope items.
- Features and user journeys covered in this plan.

## 2. References
- Requirements Spec: [link]
- User Stories: [link]
- Operational Profile (OP): [link]
- Architecture/Design: [link]
- User Documentation (Guides/Training): [link]

## 3. Acceptance Criteria Summary
| ID | Requirement/User Story | Acceptance Criterion | Rationale |
|----|------------------------|----------------------|-----------|
| AC-001 | REQ-F-021 | User can reset password with rate-limits enforced | Business rule |

## 4. Test Design (End-to-End Journeys)
- Selection method (e.g., OP-driven top journeys, risk-based).
- Journeys list with priorities (P1/P2) and integrity levels.
| Journey ID | Title | Priority | Integrity | Rationale |
|------------|-------|----------|-----------|-----------|

## 5. Acceptance Test Cases (Scenario Format)
- Scenarios specified using structured Given/When/Then with precise assertions.

Example Format:
```
ATC-001: Successful checkout with valid Visa card (P1)
Related: REQ-F-050, Story: ST-CHK-12
Given a logged-in user with items in the cart
And a valid Visa card ending 4242 with available funds
When the user submits the payment
Then the order is created with status "PAID"
And a receipt email is sent to the user within 60 seconds
And the transaction appears in the ledger with amount $X and currency USD
```

Include boundary and failure scenarios:
- Over credit limit
- Expired or unsupported card
- Network timeout with retry rules

## 6. Procedures (Data, Environment, Steps)
- Test data setup (seeds, known-good baselines)
- Environment needs (URLs, accounts, secrets policy)
- Step-by-step procedures (if not fully implicit in automation)

## 7. Automation Strategy
- Tools and frameworks (e.g., Playwright/Cypress, REST client, BDD)
- How tests are executed in CI (triggers, stages)
- Data management and idempotency
- Flake mitigation (retries, timeouts, network isolation)

## 8. Traceability Matrix
| Req/Story | Acceptance Criterion | Test Case ID | Procedure | Result |
|-----------|----------------------|--------------|----------|--------|
| REQ-F-050 | Payment with supported card types | ATC-001 | PROC-001 | PASS/FAIL |

## 9. Risk, Hazard, and Security Checks
- Hazard Analysis items validated by AT
- Security abuse cases (e.g., brute-force lockout)
- Privacy and data handling checks

## 10. Expected Results and Oracles
- Objective oracles for each test (values, time bounds, side-effects)
- Golden master references (e.g., report output comparison)

## 11. Entry/Exit Criteria
Entry:
- Requirements/story acceptance criteria approved by customer
- Automation framework operational in CI
Exit:
- 100% of P1 journeys pass
- No critical defects open
- Documentation checks completed

## 12. Documentation Adequacy Check
- Verify user guides, help, and training materials exist and are accurate
- Validate procedures match operator expectations

## 13. Results and Acceptance Decision
| Test Case | Result | Evidence | Defects |
|-----------|--------|----------|---------|
| ATC-001 | PASS | CI-Run-1234 | - |
Decision: ACCEPT / CONDITIONAL ACCEPT (with actions) / REJECT

## 14. Follow-Up Actions
- List defects and route to corrective-action loop (CAP IDs)
- Plan for next iteration of acceptance testing
```

---
## 🧪 WORKFLOW (Step-by-Step)

1) Collect inputs
- Requirements and user stories with acceptance criteria
- Operational Profile (top journeys, usage frequencies)
- Integrity levels and risk profile
- User documentation drafts

2) Elicit & clarify acceptance criteria (customer-owned)
- Facilitate workshops with acquirer to turn stories into precise ACs
- Ensure criteria are testable (objective values, time bounds)

3) Select journeys and scenarios
- End-to-end journeys prioritized by OP and risk
- Include boundary, failure, and abuse cases

4) Write tests before code (Test-First)
- Create automated acceptance tests early; treat as executable spec
- Avoid coupling to internals; use public interfaces only

5) Automate and integrate in CI
- Make tests deterministic and repeatable; seed data; control clocks
- Establish CI stage for acceptance suite; publish artifacts (videos, logs)

6) Traceability
- Link Req/Story → AC → Test Design → Test Case → Procedure → Result
- Keep matrix up-to-date; fail the build if links are missing

7) Execute and record results
- Capture objective evidence; store in artifacts
- Summarize pass/fail with links to defects

8) Route failures to corrective-action loop
- Create CAP using `corrective-action-loop.prompt.md`
- Re-run acceptance suite after fixes; update traceability and decision

---
## ✅ ALWAYS DO (MANDATORY)

- Customer/acquirer specifies acceptance criteria and owns acceptance tests
- Write acceptance tests before implementation begins
- Automate all acceptance tests; run frequently (ideally on each CI build)
- Exercise the system end-to-end via public interfaces
- Maintain full traceability from requirements to results
- Include boundary, failure, and abuse scenarios
- Validate user documentation and operational feasibility
- Specify objective oracles (numbers, formats, SLAs)
- Conform to project test documentation standards (e.g., IEEE 829/29119)
- Use descriptive names (TestDox style) tied to user value

---
## ❌ NEVER DO (AVOID)

- Rely on manual “Guru checks output”
- Test vague or untestable requirements—refine first
- Skip customer review or ownership
- Create brittle tests coupled to internal representation
- Write code first, tests later
- Write functional tests that invoke internals instead of public interfaces
- Omit documentation adequacy checks
- Keep redundant tests with no added value

---
## 🧩 TEMPLATES

### Scenario Template (Given/When/Then)
```
ATC-[NNN]: [Descriptive Title] (Priority: P1/P2)
Related: [REQ-*, Story-*]
Preconditions:
- [Environment/Data]
Scenario:
Given [initial state]
And [additional preconditions]
When [action]
Then [observable outcome]
And [secondary observable outcome]
Oracles:
- [Precise expected values]
- [Timing/throughput bounds]
Artifacts:
- [Logs/Reports/Screenshots]
```

### Procedure Template
```
PROC-[NNN]: [Procedure Title]
Purpose: [Short purpose]
Steps:
1. [Step]
2. [Step]
3. [Step]
Data:
- [Seed fixtures / accounts]
Environment:
- [URLs, feature flags]
```

### Traceability Matrix Template
```
| Req | Story | AC | Test ID | Procedure | Result |
|-----|-------|----|---------|-----------|--------|
```

---
## 🧪 EXAMPLES

### Payments
- Valid Visa card → PASS
- Unsupported Diners Club card → FAIL with message "Unsupported card"
- Amount over credit limit → DECLINE with reason "Insufficient credit"
- Network timeout → RETRY logic invoked at most 3 times with exponential backoff

### Job Posting Display
- Posting with salary missing → UI must not show the "Salary range" label

### Report Generation
- Generate monthly statement → Compare to golden master PDF checksum; PASS if equal

Example scenario:
```
ATC-045: Decline over-the-limit purchase (P1)
Related: REQ-F-050, ST-CHK-12
Given a user with credit limit $500
And a cart totaling $650
When the user attempts payment
Then the transaction is rejected with code LIMIT_EXCEEDED
And the cart remains intact
And no ledger entry is created
```

---
## 🔗 RELATED PROMPTS
- `requirements-validate.prompt.md`
- `test-gap-filler.prompt.md`
- `traceability-builder.prompt.md`
- `corrective-action-loop.prompt.md`
- `reliability-test-design.prompt.md`

---
## 📘 NOTES
- Store acceptance artifacts under `07-verification-validation/test-cases/acceptance/`
- Name files `AT-[Project]-v[Major.Minor]-[YYYYMMDD].md`
- Acceptance decision recorded in `07-verification-validation/test-results/`

---
**End of Prompt**
