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
- [Tuning](#tuning)
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

1. **Round 1 - live learning:** a short locked countdown introduces the match. Bounded patterns saved from earlier runs seed the enemy immediately; on a fresh profile, the predictor begins retraining after three committed actions during this round.
2. **Intermission - reveal:** the collected dataset is analyzed and the HUD shows one evidence-backed observation plus the enemy's intended tactical adjustment. If the evidence gate is not met, the HUD says that baseline tactics are being retained instead of inventing a habit.
3. **Round 2 - adapting:** the enemy tests a bounded learned counter. Its normal range, line-of-sight, stamina, cooldown, action-phase, seeded gate, cooldown, and per-round counter-budget rules still apply.
4. **Round 3 - adapted:** the HUD explicitly asks the player to counter-adapt by changing the habit the enemy learned. Strong new evidence may replace the prior profile; sparse or conflicting evidence leaves baseline behavior in place.
5. **Match complete and restart:** the final screen shows the three-round score and winner. Pressing the restart action clears match-scoped counter outcomes, projectiles, action/round timers, buffered input, feedback, and procedural presentation state. The bounded prior-run player-pattern history remains available for the next Round 1.

The player can use light and heavy attacks, block, dodge in three directions, heal, move, and jump. Accepted combat actions progress through explicit `Windup`, `Active`, and `Recovery` phases, with dedicated `Blocking`, `Dodging`, `Staggered`, and `Dead` states. Damage, healing, projectile creation, dash movement, and dodge invulnerability occur in their intended phase rather than at input or decision time. The default enemy has twice the player's health, 1.5 times the player's stamina, and twice the matching light/heavy attack damage; secondary enemy attacks derive from that same multiplied damage baseline. The enemy uses distance, line of sight, stamina, health, committed recent actions, cooldown availability, and learned player tendencies when choosing a response.

Above individual action utilities, the enemy commits briefly to an `Observe`, `Approach`, `Orbit`, `Pressure`, `Retreat`, `Defend`, `Punish`, or `Recover` tactic. That state adjusts spacing and action scores within a hard bound while leaving the existing utility scorer authoritative. Range hysteresis and state commitment prevent rapid side-to-side churn; low stamina produces a real spacing/recovery interval, actual player attack windups can trigger defense, and projectiles require a clear visibility trace. Close pressure can start one seeded light-light or light-heavy sequence, but range, line of sight, stamina, threat, deadline, cooldown, target loss, round state, or any incompatible action exits it after at most one follow-up.

At startup, the newest 128 valid committed-action patterns are loaded from a versioned local SaveGame slot. The payload contains only predictor context and action labels—no identity, health, timestamps, raw input, or counter outcomes—and uses FIFO retention so newer play gradually replaces older style. At each playable round boundary, the strongest round-supported predictor context is converted into an `FAdaptiveTacticalProfile`. Repeated blocking creates heavier guard and repositioning pressure; healing creates closer pursuit and interrupt priority; repeated attacks increase defense and punish intent; backward dodges increase chase/ranged pressure; and lateral dodges can bias the enemy's committed orbit direction. Every movement and utility contribution is independently clamped. Prediction-specific counters still pass the existing confidence threshold and seeded application chance, then consume a short cooldown and a finite per-round budget, so the enemy changes style without answering every action. All final range, line-of-sight, stamina, cooldown, phase, and availability checks remain in the combat executor path.

Every committed enemy utility action now receives one stable outcome ID and one terminal result: hit, miss, blocked, dodged, interrupted heal, or invalidated before its Active frame. The decision layer associates that raw executor result with the immutable prediction, confidence, combat snapshot, counter opportunity, and round that produced it; this match-scoped history is separate from both `IPlayerActionPredictor` and damage ownership. A deterministic round analyzer groups actual adaptive attempts by predicted action and chosen counter. It requires at least three samples, applies symmetric smoothing, and contributes at most a small independently clamped secondary utility adjustment during a counter opportunity that already passed the profile's seeded gate, cooldown, and budget. One success therefore has no gameplay effect, and outcome learning cannot make an unavailable action executable.

Player locomotion uses responsive acceleration, firm braking, faster travel-facing rotation, bounded air control, and a low analog minimum for useful gamepad speed control. The spring arm adds subtle position lag and responsive rotation smoothing. Directional dodges resolve from the current camera yaw (with a player-relative fallback). During only the final 140 ms of `Recovery`, one discrete combat input may be buffered; it then uses the normal commit path, so stamina, cooldowns, snapshots, and behavior labels do not change until the action actually starts. Windup and Active phases cannot be canceled this way, a second input cannot replace the buffered action, and pending input is cleared by stagger, death, combat/input lock, round reset, or invalid state. Melee attacks also apply a small bounded facing correction toward a living nearby target inside a forward cone, never toward targets behind the player.

Both primitive bodies now use a code-driven greybox presentation layer. Idle breathing, movement steps, forward/strafe lean, action anticipation and strikes, guard and dodge silhouettes, hit recoil, death collapse, telegraph colors, hit flash, and invulnerability pulses are derived from gameplay state. Resolved hits, blocks, dodges, misses, and deaths add distinct short scale/color pulses and center-screen result labels. Player and enemy neutral colors are intentionally different. The capsule, combat components, and phase timers remain authoritative; presentation only offsets the collision-free visible mesh and can be disabled without changing combat results.

Combat feedback adds small camera offsets for light and heavy player attacks, received hits, blocks, and directional dodges. Successful heavy impacts use a stronger impulse than light hits. Damage can launch a living character a short configurable distance away from the instigator; heavy knockback is bounded by a hard maximum and blocking reduces it. The implementation deliberately uses no hit stop or global time dilation, so action phases, input buffering, cooldowns, stamina regeneration, decision intervals, and round timers remain on their existing clocks.

The always-visible HUD shows player and enemy health/stamina bars, current round and match phase, pre-round and intermission countdowns, live-learning/adapting/adapted labels, evidence-backed observation and adjustment text, recent combat-result text, and prediction availability. Match completion shows the aggregate score, winner, and restart prompt. Detailed prediction, dataset, tactical, outcome, and reveal telemetry is hidden by default and can be toggled separately for development; the same reveal and evidence status are written to the output log.

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
Derive a bounded, explainable round tactical profile
        |
        v
Predict the player's likely next action
        |
        v
Select a committed tactical state
        |
        v
Blend bounded tactic and prediction weights into utility scores
        |
        v
Apply bounded repetition fatigue and recent-result memory
        |
        v
Sample a seeded, score-weighted action from the near-best set
        |
        v
Record the committed action's resolved outcome once
        |
        v
Aggregate bounded counter effectiveness between rounds
```

The predictor learns how the player responds to combinations of the enemy's previous action, distance band, relative position, and the player's previous action. It first tries the exact `(previous enemy action, distance, relative position, previous player action)` response table. If that context is unseen or sparse, it falls back in order to `(previous enemy action, distance, relative position)`, `(previous enemy action, distance)`, the broader enemy-action table, and then the global player-action frequency learned from every valid sample. Samples with no previous player action enter the broader tables without creating an artificial `None` refinement. Every level uses the same evidence threshold and fixed action order for deterministic ties.

The model does **not** use an external machine-learning framework or a neural network. Prediction is exposed through `IPlayerActionPredictor`, so a statistical or model-backed implementation can replace the current predictor without changing the enemy decision component.

The action scorer is deterministic and side-effect free. Prediction confidence is scaled by a tuning influence before it modifies the normal combat utility scores. Separate deterministic tactical-state, adaptive-profile, counter-effectiveness, recent-player-pattern, repetition, offense/defense-balance, recent-result, and selection policies supply independently bounded inputs. Once at least three committed light attacks dominate the newest six actions, the pattern channel increasingly boosts block/dodge and suppresses damage racing, reaching its independent cap at a 75% light-attack share. The scorer retains raw utility for ranking while exposing a clamped display score, so two values displayed as `1.0` do not become an artificial fixed-order tie. The selection policy finds the best raw utility, excludes actions outside a small near-best window, and uses a seeded temperature-weighted roll inside that credible set. Higher utilities remain substantially more likely; clearly inferior or unavailable actions never enter the set. The original deterministic best-action API remains available for tests and diagnostics.

Only successfully committed enemy utility actions enter a five-action fatigue history. Immediate repeats receive the largest single penalty, repeated use accumulates additional bounded fatigue, and repeated offensive-family use receives a much smaller adjustment. Fatigue never marks an action unavailable, so a sole legal response can still be selected and the explicit two-hit sequence remains capped by `FEnemyAttackSequenceState`.

A separate six-commit cadence policy targets a 45% defensive share with an 8% dead zone. Sustained offense raises block/dodge utility and lowers attack utility; sustained defense applies the inverse so the enemy does not turtle indefinitely. It waits for three committed actions, caps its adjustment at `0.35`, and never changes stamina, cooldown, threat, range, line-of-sight, phase, or executor availability rules.

Ordinary terminal results also enter a separate six-result recent-combat window after their action ID is validated and correlated. A single blocked, dodged, missed, or pre-Active-invalidated action causes only a small response; repeated failures of the same action create a stronger but independently capped penalty. Success reinforcement is deliberately smaller. This window is neither predictor training data nor adaptive-counter effectiveness history, and it is cleared at round, target, and clean-match boundaries.

Committed player light/heavy attack and heal `Windup` transitions can schedule one extra evaluation after a human-scale reaction delay. The event comes from `UCombatComponent` after commit and never reads buffered input. At delivery time the player action must still be in `Windup`, `Active`, or `Recovery`, the enemy must still be idle, and the normal scorer and executor gates still apply. An urgent cooldown prevents repeated phase-timer evaluations from creating perfect responses; normal 0.85-second decisions continue unchanged otherwise.

A profile still requires minimum round, context, support, and confidence evidence, and the round's dominant committed action must agree with the predictor. Outcome effectiveness needs multiple matching adaptive attempts and remains a smaller secondary channel. Tactics and all memory channels therefore do not replace availability, line-of-sight, resource, cooldown, phase, seeded prediction gate, profile counter cooldown/budget, or executor checks.

## Features

- C++-owned third-person player and enemy characters
- Runtime-created Enhanced Input mapping context
- Reusable health and stamina actor components
- Phase-driven player combat with attacks, blocking, dodging, healing,
  cooldowns, stamina costs, stagger, and death cleanup
- Tuned player acceleration, braking, rotation, air control, and analog movement
- Subtle configurable spring-arm position lag and camera rotation smoothing
- Camera-relative directional dodges with a player-relative fallback
- One-action late-Recovery combat input buffer with commit-time resource,
  cooldown, snapshot, and behavior-tracking side effects
- Bounded forward-cone melee facing assistance that ignores targets behind the player
- Enemy combat execution separated from enemy decision-making
- Shared `Idle`/`Windup`/`Active`/`Recovery`/`Blocking`/`Dodging`/
  `Staggered`/`Dead` lifecycle with presentation/debug phase events
- Active-frame damage and effects protected by a single-use execution guard
- Asset-free procedural greybox animation driven by locomotion, phases, damage,
  invulnerability, reset, and death state
- Distinct light, heavy, projectile, dash, interrupt, block, and dodge silhouettes
  and color telegraphs, with optional debug-only prediction accents
- Distinct short hit, block, dodge, miss, and death pulses plus readable result text
- Configurable, hard-capped light/heavy knockback with reduced block displacement
- Timer-free camera offsets for player attacks, received hits, blocks, and dodges
- Always-visible player/enemy health and stamina, short pre-round countdowns,
  intermission countdowns, and explicit prediction-disabled/enabled status
- Evidence-backed intermission observations and intended tactical adjustments,
  with honest baseline fallback when contextual support is insufficient
- Clearly labeled baseline, adapting, and adapted/counter-adapt rounds
- Aggregate match-complete score and keyboard/gamepad clean-restart action
- Blueprint-assignable animation cues for optional future skeletal meshes or
  montages without making animation authoritative
- Reliable enemy locomotion with AI possession, navigation when available,
  and a collision-aware direct fallback for nav-free greybox maps
- Configurable approach, retreat, preferred-spacing, and orbit movement with
  smooth facing and stuck recovery
- Deterministic tactical states with commitment, distance hysteresis, stable
  orbit direction, resource-aware spacing, defense, and punish behavior
- Bounded tactical utility weights layered above the existing scorer, plus one
  limited seeded light-light or light-heavy close-range sequence
- Projectile line-of-sight gating and executor-owned stamina/cooldown availability
- Timer-driven enemy intent and utility-based combat action selection
- Pre-action combat snapshots containing player, enemy, distance, position, timing, and action history data
- Labeled player-behavior dataset collection
- Replaceable player-action predictor interface
- Enemy-action, distance, relative-position, and previous-player-action-conditioned prediction with deterministic tie handling
- Hierarchical fallback from exact snapshot context through position, distance, and enemy action to global frequency
- Prediction-aware enemy action scoring
- Raw-utility-aware near-best candidate selection with deterministic seeded,
  temperature-weighted variation and a stable best-action diagnostic
- Five-commit bounded repetition fatigue with a strong immediate-repeat cost,
  accumulated repeat resistance, and no availability override
- Six-result duplicate-safe recent outcome memory, weak single-failure response,
  stronger repeated-failure adjustment, and per-round reset
- Edge-triggered light/heavy/heal reactions sourced only from committed action
  phases, with a fair delay, cooldown, stale-threat check, and no cancellation
- Deterministic between-round tactical profiles with explainable evidence,
  preferred counter, spacing, aggression, defense, heal-interrupt, and orbit changes
- Sparse/conflicting-evidence baseline fallback plus seeded, confidence-gated,
  cooldown- and budget-limited adaptive counter opportunities
- ID-correlated hit, miss, block, dodge, heal-interrupt, and pre-Active
  invalidation results with duplicate-event suppression
- Deterministic per-round counter-effectiveness telemetry, a three-sample gate,
  symmetric smoothing, and an independently capped secondary score channel
- Match-scoped outcome history retained across rounds and explicitly cleared on
  new-match initialization
- Three-round match progression with between-round training, reveal, results,
  and full match-scoped restart cleanup
- Reliable greybox combat HUD with detailed adaptive telemetry behind a debug toggle
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
| Restart completed match | `Enter` | Menu / Special Right |

Directional dodge labels are relative to the current camera yaw. If no camera/controller basis is available, they fall back to the player's facing axes.

Press the tilde key (`~`) while playing in the editor to open the Unreal console.

## Tuning

Major vertical-slice values are centralized in **Project Settings > Game > Adaptive Hunt** through `UAdaptiveHuntTuningSettings`. The settings are stored in normal project config when edited; no Data Asset, Blueprint, or downloaded content is required. Every category also has a safe native C++ default and a deterministic sanitization path for non-finite or out-of-range config values.

| Category | Includes |
|---|---|
| Movement | Player speed, acceleration, braking, friction, rotation, air/analog control; enemy spacing, speed, facing, navigation refresh, fallback probe, and stuck recovery |
| Combat Balance | Player health, stamina, and light/heavy damage baselines; enemy health, stamina, and damage multipliers plus secondary-attack damage profiles |
| Action Timing | Player and enemy windup/Active/recovery timelines, block/dodge timing, stagger, and the single-action input-buffer window |
| Camera | Spring-arm framing, position lag, maximum lag distance, and rotation smoothing |
| Feedback | Readability duration, bounded knockback, camera impulses, procedural motion interpolation, hit flash, impact pulse, and death fall |
| Tactical AI | Evaluation cadence, combat range gates, tactical hysteresis/commitment, sequence rules, deterministic seed, near-best score window/temperature, repetition fatigue, offense/defense cadence, recent-result memory, and urgent reaction timing |
| Adaptation | Predictor confidence/application gates, live retraining thresholds, bounded SaveGame history, recent light-pattern response, tactical-profile evidence/bounds, the 2.4-second three-use counter limiter, and outcome-effectiveness smoothing/bounds |
| Match Flow | Pre-round countdown and intermission duration |

Runtime components remain the owners of behavior: the settings catalog only supplies defaults. Combat components still own timing and effects, the decision component still owns utility/tactical selection, locomotion still owns movement execution, and the round manager still owns match timers and reset boundaries.

The native decision defaults use a `0.16` near-best raw-score window and `0.08` selection temperature. Repetition keeps five commits, applies `0.22` immediately plus `0.08` for accumulated repeats, and caps the full channel at `0.42`. Offense/defense balance examines six commits after three samples, targets `0.45` defense with a `0.08` dead zone, and caps at `0.35`. Recent outcomes keep six resolved actions, apply `0.08` per repeated failure with only half strength for the first failure, add at most `0.015` per success, and cap at `0.24`. Threat reactions wait `0.18` seconds and then observe a `0.8`-second urgent cooldown. Every value rejects non-finite config input and is hard-clamped before use.

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
- Combat phase permissions, timing defaults, event exposure, reset generations,
  and single-use active effects
- Greybox presentation cue mapping, required motion poses, distinct telegraphs,
  feedback colors, neutral reset, collision isolation, and animation hooks
- Player movement/camera tuning, camera-relative dodge direction, one-entry input
  buffering and cancellation, duplicate suppression, and melee facing-assist bounds
- Enemy locomotion policy, possession defaults, missing-navigation fallback,
  and stuck recovery
- Enemy tactical state selection, commitment/hysteresis, resource recovery,
  committed-input fairness, bounded weights, spacing, sequence exits, and LOS
- Seeded near-best selection reproducibility and variation, raw-score saturation,
  inferior/unavailable exclusion, bounded repetition fatigue, sole-action fallback,
  recent-outcome failure/window/duplicate handling, and urgent phase fairness
- Combat feedback pulse differentiation, capped knockback, camera impulse sizing,
  transient reset/isolation, resource normalization, round/countdown readability,
  prediction-state labels, and debug-HUD defaults
- Adaptive tactical profile derivation and habit mappings, sparse/conflicting
  evidence fallback, visible bounded scorer/spacing changes, changed-habit
  replacement, seeded gate retention, counter cooldown/budget, and debug explanation
- Snapshot capture and classification
- Dataset recording
- Global and hierarchical snapshot-conditioned frequency prediction
- Enemy action scoring
- Prediction-driven decision changes
- Three-round progression and accumulated learning
- Pre-round countdown transitions, baseline/adapting/adapted labels,
  evidence-safe reveal wording, match scoring, and clean restart progression
- Telemetry analysis and HUD formatting
- Gameplay, learning, snapshot, scoring, prediction, round, and target edge cases
- Central tuning propagation and invalid-value sanitization, vertical-slice timing/
  spacing defaults, reset/death/target-loss timer cleanup, missing navigation,
  insufficient evidence, duplicate terminal events, and debug-command parsing/
  registration

## Debug Console Commands

The player character and project debug layer expose deterministic console commands for greybox verification. Open the console with `~` during play.

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
| `DebugToggleAdaptiveHUD` | Toggle detailed adaptive telemetry; core combat/round UI remains visible |
| `RestartMatch` | Restart after match completion with clean Round 1 match-scoped state |
| `adapthunt.ForceTacticalProfile block` | Force a validated blocking-habit profile; presets are `clear`, `block`, `heal`, `light`, `heavy`, `dodgeback`, `dodgeleft`, and `dodgeright` |
| `adapthunt.ForceActionPhase enemy windup 2` | Hold a non-damaging player/enemy phase for inspection; combatant is `player` or `enemy`, phase is any shared action phase, and duration is optional |
| `adapthunt.ForceRoundTransition playerwins` | Apply a phase-valid match transition: `start`, `playerwins`, `enemywins`, `advance`, or `restart` |
| `adapthunt.ClearPersistentLearning` | Delete prior-run player patterns while leaving current-match samples available for live learning |

Forced tactical profiles use the same pure profile derivation and bounds as learned profiles, but intentionally bypass normal evidence collection for debugging. Forced `Active` phases never execute an attack effect, so the command cannot create unexplained damage. Round commands still obey the progression state machine: for example, `advance` only succeeds during intermission and `restart` only succeeds after match completion.

Runtime diagnostics are written to Unreal's output log under the `LogAdaptHunt` category.

## Architecture

| System | Main type | Responsibility |
|---|---|---|
| Game bootstrap | `AAdaptiveGameMode` | Creates the player/enemy matchup and initializes the round manager |
| Project tuning | `UAdaptiveHuntTuningSettings` | Supplies categorized config-editable movement, action-timing, camera, feedback, tactical, adaptation, and match defaults with asset-free C++ fallbacks |
| Match flow | `URoundManager` | Owns the three-round state machine, resets, intermissions, training, and match completion |
| Adaptation reveal policy | `FAdaptiveAdaptationRevealPolicy` | Converts only validated conditional-pattern/profile provenance into deterministic round labels, observations, and intended-adjustment text |
| Player pawn | `AAdaptivePlayerCharacter` | Owns locomotion, camera, input, and reusable player-facing components |
| Enemy pawn | `AAdaptiveEnemyCharacter` | Owns enemy resources, combat execution, and decision components |
| Enemy locomotion | `UEnemyLocomotionComponent` | Executes approach, retreat, and orbit intent through navigation or collision-aware direct movement |
| Movement policy | `FEnemyMovementPolicy` | Selects deterministic spacing intent without navigation or combat side effects |
| Tactical policy | `FEnemyTacticalPolicy`, `FEnemyTacticalTuning` | Selects committed tactical state, modifies spacing, and supplies bounded utility weights and two-hit sequence rules |
| Adaptive profile policy | `FAdaptiveTacticalProfile`, `FAdaptiveTacticalProfilePolicy` | Derives one explainable round profile from committed data and predictor provenance; supplies bounded style/spacing changes and rate-limited preferred counters |
| Counter outcome policy | `FAdaptiveCounterOutcomeHistory`, `FAdaptiveCounterEffectivenessPolicy` | Stores match-scoped associated results outside the predictor/damage systems and deterministically derives evidence-gated, smoothed, bounded per-round effectiveness |
| Combat phase types | `ECombatActionPhase`, `FCombatActionTiming`, `FCombatActionRuntimeState` | Define shared timing, movement/rotation policy, and single-use Active-frame state |
| Player combat | `UCombatComponent` | Commits player decisions once and executes their timed phases and effects |
| Player responsiveness policy | `AdaptivePlayerResponsiveness`, `FPlayerCombatInputBuffer` | Provides deterministic dodge direction, recovery-window buffering, duplicate suppression, expiry, and forward-cone facing calculations |
| Enemy combat | `UEnemyCombatComponent` | Executes timed actions selected by the enemy brain, owns target-dependent cleanup, and emits one raw ID-tagged terminal result per utility action |
| Greybox presentation | `UGreyboxPresentationComponent` | Observes locomotion, action phases, damage, invulnerability, and death; modifies only the visible mesh transform/material and emits optional animation cues |
| Presentation policy | `AdaptiveGreyboxPresentation` | Maps gameplay state to deterministic, side-effect-free poses and telegraph colors |
| Combat feedback | `UCombatFeedbackComponent` | Observes committed actions, displays resolved outcomes, applies cosmetic camera/body impulses, and owns bounded knockback only |
| Feedback policy | `AdaptiveCombatFeedback` | Provides deterministic pulse, cue priority, camera impulse, label/color, and capped knockback calculations |
| Resources | `UHealthComponent`, `UStaminaComponent` | Provide reusable health, death, stamina, and regeneration behavior |
| State capture | `UCombatSnapshotComponent` | Captures high-level combat state immediately before committed player actions |
| Behavior data | `UPlayerBehaviorTrackerComponent` | Converts observed actions and snapshots into labeled training samples |
| Persistent patterns | `UAdaptivePlayerPatternSaveGame`, `FAdaptivePlayerPatternPolicy` | Store a versioned FIFO of compact committed-action contexts and derive bounded recent light-attack pressure |
| Prediction contract | `IPlayerActionPredictor` | Defines the replaceable training and prediction boundary |
| Current predictor | `FConditionalActionPredictor` | Learns player responses conditioned on the enemy's previous action, distance band, relative position, and the player's previous action |
| Fallback predictor | `FFrequencyActionPredictor` | Supplies a global prior for unseen or sparse contexts |
| Utility model | `FEnemyActionScorer` | Scores available enemy actions without side effects |
| Decision policies | `FEnemyActionSelectionPolicy`, `FEnemyActionRepetitionPolicy`, `FEnemyOffenseDefenseBalancePolicy`, `FEnemyRecentOutcomePolicy`, `FEnemyUrgentDecisionState` | Provide pure near-best weighted choice, bounded commit fatigue, offense/defense cadence correction, fixed-window terminal-result modifiers, and committed-phase reaction timing |
| Enemy brain | `UEnemyDecisionComponent` | Owns seeded runtime selection, short-term memories, urgent scheduling, tactical/utility decisions, LOS and availability context, prediction gates, adaptive counter cooldown/budget, outcome association, and seeded sequence state |
| Telemetry | `FAdaptiveLearningTelemetryAnalyzer` | Produces round-filtered learning summaries |
| Combat/debug display | `AAdaptiveDebugHUD` | Always renders resources and round/prediction state; gates detailed adaptive telemetry behind a toggle |
| Debug command policy | `FAdaptiveDebugCommandPolicy` | Parses safe profile, phase, and round-transition commands and builds validated forced profile presets without taking gameplay ownership |

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
|       |   |-- Debug/              Target dummy and vertical-slice commands
|       |   |-- Game/               Game mode, round manager, and tuning settings
|       |   |-- Presentation/       Deterministic greybox pose and cue policy
|       |   |-- Tests/              Unreal Automation tests
|       |   `-- UI/                 Debug HUD
|       |-- Public/                 Public module headers
|       `-- adaptHunt.Build.cs      Module rules and dependencies
|-- adaptHunt.uproject              Unreal project descriptor
`-- README.md
```

Generated folders such as `Binaries`, `DerivedDataCache`, `Intermediate`, `Saved`, `.vs`, and generated VS Code compile databases are intentionally excluded from version control.

## Current Scope and Limitations

- The experience uses procedurally animated primitive characters and greybox environments; it does not include authored skeletal animation assets.
- The current predictor conditions on the enemy's previous action, distance band, relative position, and the player's previous action; health, stamina, and timing features are captured but not yet used for prediction.
- Predictor retraining occurs during every round and at round boundaries. A bounded local FIFO of compact player patterns persists across runs; it is intentionally a recency-weighted habit history rather than an unbounded player identity or skill rating.
- Reveal text is deliberately concise and limited to committed contextual evidence already present in the analyzer/profile. It does not narrate unsupported causes or inspect live input.
- Adaptation uses one strongest supported tactical profile at a time. Counter effectiveness is intentionally match-scoped, needs repeated matching opportunities, and is not a persistent player skill model.
- Enemy movement uses a compact deterministic tactical layer over a distance-band spacing policy rather than a navigation-heavy behavior tree.
- Near-best sampling creates reproducible variation only among credible utilities;
  it is not a difficulty director and does not force a counter. Short-term memory
  is intentionally small and resets between rounds, so its exact balance still
  needs playtesting against different player timings and spacing habits.
- Urgent reactions begin only from committed `Windup` events and use a `0.18`-second
  default delay. Very short actions may reach `Active` before the response, and an
  action that has fully returned to `Idle` receives no late urgent reaction.
- Presentation uses one collision-free mesh transform and material color per combatant. Future skeletal meshes can consume the exposed cue event, but no montage or animation asset is required or included.
- Combat feedback is intentionally asset-free and silent: it uses primitive body pulses,
  HUD text, and small camera offsets rather than production VFX, audio, or UI widgets.
- Centralized values are vertical-slice defaults, not production balance. Editing
  them can change difficulty, but hard scorer/executor checks and sanitization
  still prevent invalid actions or non-finite runtime values.
- The combat buffer intentionally stores only one discrete action near Recovery end;
  it is not a combo system and does not permit Windup/Active animation canceling.
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
