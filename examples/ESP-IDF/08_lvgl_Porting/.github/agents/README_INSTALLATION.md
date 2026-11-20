# README_INSTALLATION.md

# Bike Computer AI Agents — Installation and Usage Guide

## 1. Overview

This document explains how to integrate the Bike Computer AI agent configuration into an existing firmware project using Visual Studio Code and GitHub Copilot.

The package provides:

- A central Orchestrator agent.
- Specialized development agents.
- Shared project instructions.
- Architecture documentation.
- Technical specifications.
- A development roadmap.

The agents are designed to support supervised development of the Bike Computer firmware.

They must follow the project's approval and execution policies.

## 2. Requirements

The development environment should include:

| Requirement | Version / Configuration |
|---|---|
| Operating system | Windows |
| IDE | Visual Studio Code |
| AI assistant | GitHub Copilot |
| Firmware framework | ESP-IDF 5.3.0 |
| Programming language | C |
| RTOS | FreeRTOS |
| Graphics library | LVGL 8.4.0 |
| Target MCU | ESP32-S3 |

The exact versions installed in the existing project must be checked before making changes.

## 3. Installation Structure

Copy the agent configuration and documentation into the root directory of the existing Bike Computer repository.

The recommended structure is:

```text
bike-computer/
│
├── .github/
│   ├── copilot-instructions.md
│   │
│   └── agents/
│       ├── orchestrator.agent.md
│       ├── firmware.agent.md
│       ├── hardware.agent.md
│       ├── gnss.agent.md
│       ├── data-hub.agent.md
│       ├── ble-sensors.agent.md
│       ├── connectivity.agent.md
│       ├── cycling-core.agent.md
│       ├── ui-ux.agent.md
│       ├── quality.agent.md
│       ├── test-build.agent.md
│       └── documentation.agent.md
│
├── ARCHITECTURE.md
├── DATA_CONTRACTS.md
├── HARDWARE_PROFILE.md
├── GNSS_SPECIFICATION.md
├── BLE_SENSORS_SPECIFICATION.md
├── CYCLING_CORE_SPECIFICATION.md
├── ACTIVITY_RECORDER_SPECIFICATION.md
├── STORAGE_SPECIFICATION.md
├── CONNECTIVITY_SPECIFICATION.md
├── NAVIGATION_SPECIFICATION.md
├── POWER_MANAGEMENT_SPECIFICATION.md
├── ROADMAP.md
├── README_INSTALLATION.md
│
├── components/
├── main/
├── CMakeLists.txt
├── sdkconfig
└── README.md
```

Do not overwrite existing project files without reviewing their contents.

In particular, preserve the existing firmware README, ESP-IDF configuration, component definitions, and source files.

## 4. Installing the Files

### Step 1 — Open the project

Open the existing Bike Computer repository in Visual Studio Code.

Make sure the workspace root is the repository root, not a subdirectory such as `main/` or `components/`.

### Step 2 — Create the agent directory

Inside the repository, create:

```text
.github/agents/
```

If `.github/` already exists, preserve its contents.

### Step 3 — Copy the shared instructions

Place:

```text
.github/copilot-instructions.md
```

at the root of the `.github` directory.

These instructions define the common rules for all Copilot agents.

### Step 4 — Copy the agent definitions

Place all `*.agent.md` files in:

```text
.github/agents/
```

Do not rename the agent files unless the corresponding references are updated.

### Step 5 — Copy the documentation

Place the architecture, specifications, hardware profile, and roadmap documents in the repository root.

Review existing files before copying.

If a file with the same name already exists, compare its content before replacing it.

### Step 6 — Verify the structure

Check that all files are in the expected locations.

The agent definitions must be inside `.github/agents/`.

The shared Copilot instructions must be inside `.github/`.

## 5. Configuring GitHub Copilot in VS Code

GitHub Copilot supports repository-level instructions and custom agents.

The exact interface may vary according to the installed VS Code and Copilot versions.

### 5.1 Enable Agent Mode

1. Open the Bike Computer workspace.
2. Open GitHub Copilot Chat.
3. Select Agent Mode if available.
4. Check whether the custom agents are available.
5. Confirm that repository instructions are being recognized.

### 5.2 Select a custom agent

Use the agent selector in Copilot Chat to select the desired specialized agent.

The intended agents are:

| Agent | Main responsibility |
|---|---|
| Orchestrator | Coordinates project work |
| Firmware | General firmware implementation |
| Hardware | Board and peripheral integration |
| GNSS | GNSS communication and positioning |
| Data Hub | Unified application data |
| BLE Sensors | BLE cycling sensors |
| Connectivity | Wi-Fi and network services |
| Cycling Core | Cycling calculations |
| UI/UX | LVGL and user interface |
| Quality | Architecture and code review |
| Test/Build | Testing and build validation |
| Documentation | Technical documentation |

If an agent does not appear, verify its file location and the installed Copilot version.

### 5.3 Agent collaboration

The Orchestrator is the central coordination point.

Specialized agents do not automatically execute a complete multi-agent workflow.

The intended collaboration model is:

1. Ask the Orchestrator to analyze a task.
2. Review the proposed plan.
3. Select the appropriate specialized agent.
4. Request the specialized analysis or implementation.
5. Review the result.
6. Obtain approval for each file modification.
7. Request Quality review when required.
8. Return to the Orchestrator for integration coordination.

Manual handoffs are expected unless a supported orchestration feature is explicitly configured.

## 6. First-Time Validation

Before asking an agent to modify firmware, perform a read-only validation.

Suggested initial request:

```text
Read-only task.

Inspect the Bike Computer repository and verify that:

1. The shared Copilot instructions are available.
2. All custom agent definitions are present.
3. The architecture documentation is available.
4. The hardware profile is available.
5. The technical specifications are available.
6. The roadmap is available.

Do not modify, create, delete, rename, or move any files.

Do not execute commands.

Report missing files, inconsistencies, and documentation conflicts.
```

The goal is to verify that the agent configuration is recognized without changing the existing project.

## 7. Recommended First Interaction

After validating the installation, select the Orchestrator and use the following prompt:

```text
We are starting work on the Bike Computer firmware.

Use the installed project instructions and documentation.

The project uses:
- ESP-IDF 5.3.0
- ESP32-S3
- C
- FreeRTOS
- LVGL 8.4.0

Development policies:
- Supervised autonomy.
- Central coordination.
- Project-level agent scope.
- Approval required for every individual file modification.
- Quality review for major changes.

Start with a read-only assessment of the existing repository.

Do not modify files.
Do not execute commands.
Do not build, test, or flash firmware.

Return:
1. Current architecture.
2. Existing functionality.
3. Critical defects.
4. Hardware uncertainties.
5. Architectural inconsistencies.
6. Recommended development sequence.
7. Proposed first implementation task.
```

This establishes a baseline before implementation begins.

## 8. Operating Rules

All agents must follow the following project rules.

### 8.1 Supervised autonomy

Agents may independently:

- Analyze source code.
- Inspect project documentation.
- Identify defects.
- Propose solutions.
- Design interfaces.
- Prepare implementation plans.

Agents must not assume authorization to execute implementation actions.

### 8.2 File approval

The project uses `approval_each`.

Explicit approval is required before every individual:

- File creation.
- File modification.
- File deletion.
- File rename.
- File move.

Approval for one file does not authorize changes to another file.

A multi-file task must be broken down into individually approved file operations.

### 8.3 Command execution

No commands may be executed without explicit approval.

This includes:

- ESP-IDF build commands.
- Compilation.
- Unit tests.
- Integration tests.
- Formatting.
- Static analysis.
- Flashing.
- Erasing flash.
- Hardware diagnostics.
- Git operations that modify repository state.

Read-only inspection is permitted within the project's established permissions.

### 8.4 Hardware safety

No physical hardware operation may be performed without explicit approval.

Agents must not:

- Change GPIO configuration without verification.
- Send undocumented commands to the GNSS receiver.
- Assume electrical characteristics.
- Restore the original RS485 functionality.
- Change peripheral wiring.
- Perform unapproved firmware flashing.

### 8.5 Quality review

The project uses the `major` review policy.

Quality review is required for major or high-risk changes, including:

- Architectural changes.
- Data model changes.
- Concurrency changes.
- Storage format changes.
- GNSS protocol changes.
- BLE lifecycle changes.
- Power management changes.
- Security-sensitive connectivity changes.

## 9. Recommended Development Workflow

Use the following workflow for every implementation task:

### Step 1 — Define the task

Describe the requested feature or defect.

The request should include:

- Objective.
- Expected behavior.
- Constraints.
- Relevant hardware.
- Acceptance criteria.

### Step 2 — Orchestrator analysis

Ask the Orchestrator to:

- Analyze the request.
- Identify dependencies.
- Select the appropriate specialized agent.
- Define the implementation plan.
- Identify files that may need modification.
- Identify risks.
- Define validation requirements.

No files should be changed at this stage.

### Step 3 — Specialized agent

Select the relevant agent.

Ask it to inspect the existing implementation and propose a solution.

If implementation is requested, the agent must identify each target file and wait for explicit approval before modifying it.

### Step 4 — Review the changes

Inspect the proposed changes.

Verify:

- Correctness.
- Architecture consistency.
- Hardware compatibility.
- Memory impact.
- Task and synchronization safety.
- Error handling.
- Documentation updates.

### Step 5 — Quality review

For major changes, ask the Quality agent to review the implementation.

The Quality agent should identify:

- Critical defects.
- Architectural violations.
- Potential regressions.
- Missing tests.
- Security risks.
- Resource management issues.

### Step 6 — Validation

Use the Test/Build agent to propose a validation plan.

Commands and tests must only be executed after explicit approval.

### Step 7 — Documentation

Ask the Documentation agent to update relevant documentation.

Every documentation file modification requires its own approval.

### Step 8 — Integration summary

Return to the Orchestrator.

Request a summary of:

- Implemented files.
- Approved changes.
- Validation results.
- Remaining issues.
- Next recommended task.

## 10. Troubleshooting

### Agents are not visible

Check:

- The repository is opened at its root.
- Agent files are inside `.github/agents/`.
- File names use the `.agent.md` extension.
- The installed Copilot version supports custom agents.
- VS Code has refreshed the workspace configuration.

### Shared instructions are not applied

Check that:

```text
.github/copilot-instructions.md
```

exists at the repository root.

Verify that the current workspace is the intended repository.

### An agent does not follow project rules

Check:

- The shared instructions.
- The selected agent definition.
- The task prompt.
- The approval policy.

Stop the task if an agent attempts an unauthorized modification or command execution.

### Agent collaboration does not happen automatically

Manual handoffs are part of the intended central coordination model.

Ask the Orchestrator which specialized agent should handle the next step, then select that agent explicitly.

### Documentation conflicts with source code

The existing implementation must be inspected.

Do not blindly modify source code to match documentation.

Report the discrepancy and request an architectural decision from the Orchestrator.

## 11. Maintenance

The configuration package should evolve together with the firmware.

Update the documentation when:

- Hardware changes.
- A new component is introduced.
- An API changes.
- A data contract changes.
- A new feature is implemented.
- An architectural decision is revised.
- A development phase is completed.

Keep the shared instructions concise and place detailed technical information in the relevant specification documents.

Review the agent definitions periodically to ensure that their responsibilities remain consistent with the architecture.

## 12. Installation Completion Checklist

- [ ] Repository opened at the correct root.
- [ ] `.github/copilot-instructions.md` installed.
- [ ] All custom agent definitions installed.
- [ ] `ARCHITECTURE.md` installed.
- [ ] `DATA_CONTRACTS.md` installed.
- [ ] `HARDWARE_PROFILE.md` installed.
- [ ] `GNSS_SPECIFICATION.md` installed.
- [ ] `BLE_SENSORS_SPECIFICATION.md` installed.
- [ ] `CYCLING_CORE_SPECIFICATION.md` installed.
- [ ] `ACTIVITY_RECORDER_SPECIFICATION.md` installed.
- [ ] `STORAGE_SPECIFICATION.md` installed.
- [ ] `CONNECTIVITY_SPECIFICATION.md` installed.
- [ ] `NAVIGATION_SPECIFICATION.md` installed.
- [ ] `POWER_MANAGEMENT_SPECIFICATION.md` installed.
- [ ] `ROADMAP.md` installed.
- [ ] Custom agents recognized by VS Code.
- [ ] Shared instructions recognized.
- [ ] Read-only validation completed.
- [ ] No existing project files unintentionally overwritten.

## 13. Final Notes

This package defines the development process and architectural direction of the Bike Computer project.

It does not replace:

- Existing firmware documentation.
- ESP-IDF documentation.
- Official hardware documentation.
- Protocol specifications.
- Actual source-code verification.

The repository remains the authoritative source for the current implementation.

When documentation and implementation disagree, agents must identify the discrepancy and request clarification rather than making assumptions.

The primary objective is to build a reliable, maintainable, and extensible cycling computer through controlled, incremental development.