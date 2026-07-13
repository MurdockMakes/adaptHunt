# adaptHunt

![Unreal Engine](https://img.shields.io/badge/Unreal%20Engine-5.5-0E1128?logo=unrealengine&logoColor=white)
![Language](https://img.shields.io/badge/language-C%2B%2B-00599C?logo=cplusplus&logoColor=white)
![Platform](https://img.shields.io/badge/platform-Windows-0078D4?logo=windows&logoColor=white)
![Status](https://img.shields.io/badge/status-prototype-orange)

**adaptHunt** is a third-person Unreal Engine combat prototype about fighting an enemy that learns from the player's habits between rounds.

The enemy records committed player actions, builds an interpretable context-aware frequency model, and uses that prediction as one input to a deterministic utility scorer. The result is an adaptive opponent whose decisions can be inspected, tested, and replaced without coupling the learning system to character input or combat execution.

> This repository is an experimental greybox vertical slice. It is intended to demonstrate adaptive combat architecture rather than finished art, content, or production balance.

## Table of Contents

- [Gameplay](#gameplay)
- [How Adaptation Works](#how-adaptation-works)
- [Features](#features)
- [Controls](#controls)
- [Requirements](#requirements)
- [Getting Started](#getting-started)
- [Building from the Command Line](#building-from-the-command-line)
- [Running the Tests](#running-the-tests)
- [Debug Console Commands](#debug-console-commands)
- [Architecture](#architecture)
- [Project Structure](#project-structure)
- [Current Scope and Limitations](#current-scope-and-limitations)
- [Contributing](#contributing)
- [License](#license)

## Gameplay

adaptHunt runs a three-round match between the player and one adaptive enemy.

1. **Round 1 - baseline:** the enemy fights without learned predictions while the game records the player's combat decisions.
2. **Intermission:** the collected action dataset is analyzed and used to train the predictor.
3. **Rounds 2 and 3 - adaptation:** the enemy combines its normal utility scores with the learned prediction when selecting combat actions.
4. **Round reset:** health, stamina, cooldowns, movement, transforms, and lingering projectiles are reset while the behavior dataset remains available for the next round.

The player can use light and heavy attacks, block, dodge in three directions, heal, move, and jump. The enemy uses distance, stamina, health, recent actions, action availability, and learned player tendencies when choosing a response.

## How Adaptation Works

The current learning loop is intentionally small and explainable:

```text
Player commits an action
        |
        v
Capture the pre-action combat state
        |
        v
Store a labeled training sample
        |
        v
Train global and contextual response frequencies
        |
        v
Predict the player's likely next action
        |
        v
Blend prediction confidence into utility scores
        |
        v
Execute the highest-scoring available enemy action
```

The predictor learns how the player responds to combinations of the enemy's previous action, distance band, relative position, and the player's previous action. It first tries the exact `(previous enemy action, distance, relative position, previous player action)` response table. If that context is unseen or sparse, it falls back in order to `(previous enemy action, distance, relative position)`, `(previous enemy action, distance)`, the broader enemy-action table, and then the global player-action frequency learned from every valid sample. Samples with no previous player action enter the broader tables without creating an artificial `None` refinement. Every level uses the same evidence threshold and fixed action order for deterministic ties.

The model does **not** use an external machine-learning framework or a neural network. Prediction is exposed through `IPlayerActionPredictor`, so a statistical or model-backed implementation can replace the current predictor without changing the enemy decision component.

The action scorer is deterministic and side-effect free. Prediction confidence is scaled by a tuning influence before it modifies the normal combat utility scores, keeping the learned behavior inspectable and bounded.

## Features

- C++-owned third-person player and enemy characters
- Runtime-created Enhanced Input mapping context
- Reusable health and stamina actor components
- Player combat with attacks, blocking, dodging, healing, cooldowns, and stamina costs
- Enemy combat execution separated from enemy decision-making
- Timer-driven enemy movement and utility-based action selection
- Pre-action combat snapshots containing player, enemy, distance, position, timing, and action history data
- Labeled player-behavior dataset collection
- Replaceable player-action predictor interface
- Enemy-action, distance, relative-position, and previous-player-action-conditioned prediction with deterministic tie handling
- Hierarchical fallback from exact snapshot context through position, distance, and enemy action to global frequency
- Prediction-aware enemy action scoring
- Three-round match progression with between-round training
- On-screen debug HUD for the adaptive-learning lifecycle
- Unreal Automation coverage for combat, resources, snapshots, prediction, scoring, telemetry, rounds, and edge cases

## Controls

Input mappings are created in C++, so the prototype does not depend on Blueprint input assets.

| Action | Keyboard and mouse | Gamepad |
|---|---|---|
| Move | `W`, `A`, `S`, `D` | Left stick |
| Look | Mouse | Right stick |
| Jump | `Space` | Face button bottom / A |
| Light attack | Left mouse button | Right trigger |
| Heavy attack | `F` | Face button top / Y |
| Block | Hold right mouse button | Hold left trigger |
| Dodge left | `Q` | D-pad left |
| Dodge right | `E` | D-pad right |
| Dodge backward | Left `Shift` | Face button right / B |
| Heal | `R` | D-pad up |

Press the tilde key (`~`) while playing in the editor to open the Unreal console.

## Requirements

- [Unreal Engine 5.5](https://www.unrealengine.com/download)
- Visual Studio 2022 with:
  - **Desktop development with C++**
  - **Game development with C++**
  - A Windows 10 or Windows 11 SDK
- Windows 10 or Windows 11
- A DirectX 12 and Shader Model 6 capable graphics card is recommended
- Git

The project enables Enhanced Input, Lumen, virtual shadows, mesh distance fields, and ray tracing in its default configuration.

## Getting Started

### 1. Clone the repository

```powershell
git clone https://github.com/MurdockMakes/adaptHunt.git
cd adaptHunt
```

### 2. Generate the IDE files

Right-click `adaptHunt.uproject` in File Explorer and select **Generate Visual Studio project files**.

If that option is unavailable, associate `.uproject` files with your Unreal Engine 5.5 installation through the Epic Games Launcher, then try again.

### 3. Build the editor target

Open `adaptHunt.sln` in Visual Studio and select:

```text
Development Editor | Win64
```

Set `adaptHunt` as the startup project and build the solution.

### 4. Open the project

Double-click `adaptHunt.uproject`, or launch it from the Unreal Engine project browser. Allow Unreal to compile project modules if prompted.

Press **Play** in the editor to start the three-round prototype.

## Building from the Command Line

From the repository root in PowerShell, build the editor target with:

```powershell
& "C:\Program Files\Epic Games\UE_5.5\Engine\Build\BatchFiles\Build.bat" `
  adaptHuntEditor Win64 Development `
  "$PWD\adaptHunt.uproject" `
  -WaitMutex -NoHotReloadFromIDE
```

Build the standalone game target with:

```powershell
& "C:\Program Files\Epic Games\UE_5.5\Engine\Build\BatchFiles\Build.bat" `
  adaptHunt Win64 Development `
  "$PWD\adaptHunt.uproject" `
  -WaitMutex -NoHotReloadFromIDE
```

Adjust the engine path if Unreal Engine is installed somewhere else.

## Running the Tests

### Unreal Editor

1. Open the project in Unreal Editor.
2. Select **Tools > Test Automation**.
3. Enable the relevant test filters if the tests are hidden.
4. Search for `adaptHunt`.
5. Select the project tests and choose **Start Tests**.

### Command line

From the repository root in PowerShell:

```powershell
& "C:\Program Files\Epic Games\UE_5.5\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" `
  "$PWD\adaptHunt.uproject" `
  -unattended -nopause -NullRHI `
  '-ExecCmds=Automation RunTests adaptHunt;Quit' `
  '-TestExit=Automation Test Queue Empty' `
  -log
```

The suite is organized by implementation milestone and covers:

- Combat data defaults
- Player movement
- Health and stamina
- Player and enemy combat
- Snapshot capture and classification
- Dataset recording
- Global and hierarchical snapshot-conditioned frequency prediction
- Enemy action scoring
- Prediction-driven decision changes
- Three-round progression and accumulated learning
- Telemetry analysis and HUD formatting
- Gameplay, learning, snapshot, scoring, prediction, round, and target edge cases

## Debug Console Commands

The player character exposes deterministic console commands for greybox verification. Open the console with `~` during play.

| Command | Purpose |
|---|---|
| `DebugDamageSelf 25` | Apply damage to the player |
| `DebugSpendStamina 25` | Spend player stamina |
| `DebugResetResources` | Restore player resources |
| `DebugLightAttack` | Commit a light attack |
| `DebugHeavyAttack` | Commit a heavy attack |
| `DebugToggleBlock` | Toggle the blocking state |
| `DebugDodgeLeft` | Commit a left dodge |
| `DebugDodgeRight` | Commit a right dodge |
| `DebugDodgeBackward` | Commit a backward dodge |
| `DebugHeal` | Commit a heal action |
| `DebugSpawnCombatDummy 250` | Spawn a target dummy at the requested distance |
| `DebugCombatSnapshot` | Capture and log the current learning features |
| `DebugBehaviorDataset` | Log the collected labeled player decisions |
| `DebugResetBehaviorDataset` | Clear the collected behavior dataset |

Runtime diagnostics are written to Unreal's output log under the `LogAdaptHunt` category.

## Architecture

| System | Main type | Responsibility |
|---|---|---|
| Game bootstrap | `AAdaptiveGameMode` | Creates the player/enemy matchup and initializes the round manager |
| Match flow | `URoundManager` | Owns the three-round state machine, resets, intermissions, training, and match completion |
| Player pawn | `AAdaptivePlayerCharacter` | Owns locomotion, camera, input, and reusable player-facing components |
| Enemy pawn | `AAdaptiveEnemyCharacter` | Owns enemy resources, combat execution, and decision components |
| Player combat | `UCombatComponent` | Validates and commits player combat actions |
| Enemy combat | `UEnemyCombatComponent` | Executes the action selected by the enemy brain |
| Resources | `UHealthComponent`, `UStaminaComponent` | Provide reusable health, death, stamina, and regeneration behavior |
| State capture | `UCombatSnapshotComponent` | Captures high-level combat state immediately before committed player actions |
| Behavior data | `UPlayerBehaviorTrackerComponent` | Converts observed actions and snapshots into labeled training samples |
| Prediction contract | `IPlayerActionPredictor` | Defines the replaceable training and prediction boundary |
| Current predictor | `FConditionalActionPredictor` | Learns player responses conditioned on the enemy's previous action, distance band, relative position, and the player's previous action |
| Fallback predictor | `FFrequencyActionPredictor` | Supplies a global prior for unseen or sparse contexts |
| Utility model | `FEnemyActionScorer` | Scores available enemy actions without side effects |
| Enemy brain | `UEnemyDecisionComponent` | Combines combat context, prediction, utility scoring, movement, and action execution |
| Telemetry | `FAdaptiveLearningTelemetryAnalyzer` | Produces round-filtered learning summaries |
| Debug display | `AAdaptiveDebugHUD` | Renders the current round and adaptive-learning state |

The code favors small actor components and plain C++ value types. Deterministic logic is kept separate from Unreal world state wherever practical, which allows much of the behavior to be tested without constructing a full gameplay world.

## Project Structure

```text
adaptHunt/
|-- Config/                         Unreal project configuration
|-- Content/                        Maps and Unreal assets
|-- Source/
|   |-- adaptHunt.Target.cs         Standalone game target
|   |-- adaptHuntEditor.Target.cs   Editor target
|   `-- adaptHunt/
|       |-- Private/
|       |   |-- AI/                 Predictor and utility scorer implementations
|       |   |-- Characters/         Player and enemy character implementations
|       |   |-- Combat/             Projectile implementation
|       |   |-- Components/         Combat, resources, snapshots, tracking, and AI
|       |   |-- Data/               Dataset and telemetry implementations
|       |   |-- Debug/              Combat target dummy
|       |   |-- Game/               Game mode and round manager
|       |   |-- Tests/              Unreal Automation tests
|       |   `-- UI/                 Debug HUD
|       |-- Public/                 Public module headers
|       `-- adaptHunt.Build.cs      Module rules and dependencies
|-- adaptHunt.uproject              Unreal project descriptor
`-- README.md
```

Generated folders such as `Binaries`, `DerivedDataCache`, `Intermediate`, `Saved`, `.vs`, and generated VS Code compile databases are intentionally excluded from version control.

## Current Scope and Limitations

- The experience uses greybox characters and environments.
- The current predictor conditions on the enemy's previous action, distance band, relative position, and the player's previous action; health, stamina, and timing features are captured but not yet used for prediction.
- Learning occurs between rounds and exists for the running match rather than as a persistent player profile.
- Enemy movement uses a simple distance-band policy rather than a navigation-heavy behavior tree.
- Combat values and prediction influence are prototype tuning values, not final balance.
- The project currently targets Unreal Engine 5.5 and Windows as its primary development environment.
- Multiplayer, packaging, save data, accessibility settings, production UI, audio, animation, and content polish are outside the current vertical-slice scope.

## Contributing

Issues and focused pull requests are welcome.

1. Fork the repository.
2. Create a branch from `main`.
3. Keep generated Unreal and IDE files out of commits.
4. Add or update automation coverage for behavior changes.
5. Build `adaptHuntEditor` and run the `adaptHunt` automation suite.
6. Open a pull request describing the behavior change and validation performed.

For substantial architectural changes, open an issue first so the approach can be discussed before implementation.

## License

This repository does not currently include an open-source license. Unless a license is added, the source code and project assets remain **all rights reserved** by the repository owner.

Copyright (c) 2026 [MurdockMakes](https://github.com/MurdockMakes).
