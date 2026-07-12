- Health and stamina
- Player and enemy combat
- Snapshot capture and classification
- Dataset recording
- Frequency prediction
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
| Current predictor | `FFrequencyActionPredictor` | Learns global player-action frequencies between rounds |
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
- The current predictor learns global action frequency; it does not yet condition its prediction on snapshot features.
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
