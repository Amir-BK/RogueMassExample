# Rogue MASS Example Project

A compact Unreal Engine 5.6 sample showing how to build a data-oriented Sim using **MASS**. It demonstrates train + passenger simulation, headway management, station stops, and chunk-wise updates with clear fragments, tags, and processors.

> UE version: 5.6. Requires Epic’s **MASS** plugins enabled (MassEntity, MassActors, MassAIBehavior, MassGameplay, MassReplication) and this project’s Rogue* modules.

> Author       : Chris Suiter (aka suits) (Rogue Entity)

> Last Updated : 10 Nov 2025

---

## Contents
- [Project Purpose](#project-purpose)
- [Quick Start](#quick-start)
- [What MASS Is](#what-mass-is)
- [Example Project - Rogue MASS Example](#example-project---rogue-mass-example)
  - [Data Model](#data-model)
      - [Fragments](#fragments)
      - [Shared](#shared)
      - [Tags](#tags)
      - [Tags Note](#tags-note)
  - [Subsystems](#subsystems)
    - [Rogue Train World Subsystem](#roguetrainworldsubsystem)
  - [Processors Overview](#processors-overview)
- [MASS Basics](#mass-basics)
  - [Core Building Blocks](#core-building-blocks)
    - [Entities](#entities)
    - [Fragments](#fragments)
    - [Tags](#tags)
    - [Shared Fragments](#shared-fragments)
    - [Archetypes](#archetypes)
    - [Chunks](#chunks)
    - [Entity Views](#entity-views)
  - [Behavior & Flow](#behavior--flow)
    - [Queries](#queries)
    - [Processors](#processors)
    - [Processing Phases](#processing-phases)
    - [Execution Order & Dependencies](#execution-order--dependencies)
    - [Delegates, Signals, and Events](#delegates-signals-and-events)
  - [World/Representation Integration](#worldrepresentation-integration)
    - [Mass Actors & Representation](#mass-actors--representation)
    - [Spawning & Despawning](#spawning--despawning)
    - [Replication](#replication)
  - [Performance Patterns](#performance-patterns)
  - [Common Pitfalls](#common-pitfalls)
- [Glossary](#glossary)

---

## Example Project - Rogue MASS Example
This project is a compact example of building a data-oriented simulation using Unreal Engine's **MASS** framework. It focuses on trains moving along a track, stopping at stations, and managing passenger boarding and unloading.

### Project Purpose

The intent is to illustrate clean architecture and common patterns in **MASS** rather than provide a production-ready system. While giving a starting point for developers new to **MASS**, I hope it also serves as a reference for experienced users looking for best practices. This project was made while learning **MASS** and is open for experimentation and extension, any constructive feedback is welcomed.

### Goals
- Demonstrate clean **MASS** separation of data (fragments), behavior (processors), and creation (archetypes).
- Keep per-frame work cache-friendly and chunk-wise.
- Demonstrate basic **MASS** operations like spawning, movement, states, and rendering/representation.
- Meant for learning and experimentation, not production-ready code.

---

## Quick Start
1. Download and open the project in Unreal Engine 5.6.
2. Load the example map `Content/Maps/L_Example1.umap`.
3. Review data assets under `Content/Mass/Configurations`.
4. Configure simulation parameters in `Project Settings > Rogue MASS Example` (Developer Settings).
5. Play the map to see trains moving, stopping at stations, and passengers boarding/unloading.
6. Use Unreal Entity Debugger via ' " ' key (default - left of enter) to get entity overheads, use shortcut keys to toggle displays.

---

## What MASS Is
**MASS** is a data-oriented ECS framework inside Unreal Engine. It separates:
- **Data**: Small, POD-like structs on entities.
- **Behavior**: Systems called **Processors** that iterate over matching entities in cache-friendly **chunks**.
- **Structure**: **Archetypes** that define which fragments/tags an entity carries.

This enables large-scale simulations with predictable memory access and parallel processing.

---

### Data Model

#### Fragments
- **FRogueStationQueueFragment**: `Grids` for passenger queuing at stations. `WaitingPoints`, `SpawnPoints`, `WaitingGridConfig`.
- **FRogueTrainTrackFollowFragment**: `Alpha` along track, `Speed`, `WorldPos`, `WorldFwd`, 
- **FRogueStationFragment**: `StationIndex` index on track, `DockedTrain` current train at station.
- **FRogueTrainStateFragment**: `bIsStopping`, `bAtStation`, `StationTrainPhase` unload/load phases, `HeadwaySpeedScale`, `StationTimeRemaining` train at station, `PrevAlpha`, `TargetStationIdx`, `PreviousStationIdx`, `TrainLength`.
- **FRogueTrainLinkFragment**: `LeadHandle` train to follow, `CarriageIndex`, `Spacing`.
- **FRogueCarriageFragment**: `Capacity` passengers, `Occupants` entities onboard, `NextAllowedUnloadTime`, `UnloadCursor`.
- **FRoguePassengerFragment**: `OriginStation`, `DestinationStation`, `WaitingPointIdx`, `WaitingSlotIdx`, `VehicleHandle` train assigned to, `Phase` waiting, loading, unloading etc, `Target` move target, `AcceptanceRadius`, `MaxSpeed`, `bWaiting`.
- **FRogueTransformFragment**: world transform (MassGameplay).

#### Shared
- **FRogueTrackSharedFragment** Created on the [RogueTrainWorldSubsystem](#Subsystems), holds the spline/track data, station entities, platform data.

#### Tags
- **FRogueTrainEngineTag**, 
- **FRogueTrainCarriageTag**, 
- **FRogueTrainStationTag**
- **FRogueTrainPassengerTag**
- **FRoguePooledEntityTag**

#### Tags Note
Tags are used in this project however given the unique archetypes, they are not strictly necessary. Normally tags help identify entities that share fragments but differ in behavior. They have been included here for demonstration. A good example of the use of tags would be for purely Transform operations on a specific entity type where the archetype is shared with other entity types that do not need Transform updates.


### Subsystems

#### RogueTrainWorldSubsystem

- Manages global train world state.
- Holds the track spline, station entities, platform data.
- Provides access to track geometry for processors.
- Initializes shared fragments.
- Handles all entity spawning requests and post spawning configuration.
- Manages pooling of passenger entities.
- Provides utility functions for train and passenger management.
- Facilitates communication between processors and global state.
- Handles track configuration and station setup.

---

### Processors Overview

| Processor                         | Entity Type   | Phase                                                                     | Purpose                                                          |
|-----------------------------------|---------------|---------------------------------------------------------------------------|------------------------------------------------------------------|
| RoguePassengerHeightProcessor     | Passenger     | PrePhysics - ExecuteAfter: RoguePassengerMovementProcessor                | Height of passenger entities on platforms                        |
| RoguePassengerMovementProcessor   | Passenger     | PrePhysics - ExecuteInGroup: Movement                                     | All passenger movement and state control                         |
| RoguePassengerSpawnProcessor      | TrainStation  | FrameEnd - ExecuteInGroup: Tasks                                          | Random station spawn enqueue of passenger entities               |
| RogueTrainCarriageFollowProcessor | TrainCarriage | ExecuteInGroup: Movement, ExecuteAfter: RogueTrainEngineMovementProcessor | Carriage train engine follow logic                               |
| RogueTrainHeadwayProcessor        | TrainEngine   | ExecuteGroup: Movement                                                    | Train spacing and braking, collision prevention        |
| RogueTrainEngineMovementProcessor | PrePhysics    | Schedule dwells, clamp speed at stations                                  | Train rail movement                                              |
| RogueTrainStationDetectProcessor  | TrainEngine   | PrePhysics - ExecuteBefore: Avoidance                                     | Train station detection and stop handling                        |
| RogueTrainStationsOpsProcessor    | TrainEngine   | PrePhysics - ExecuteAfter: RogueTrainStationDetectProcessor               | Train station state handing, passenger assignment / unassignment |
| RogueDebugDataProcessor           | All           | FrameEnd - ExecuteInGroup: Tasks                                          | Debug data gathering                                             |


---

# MASS Basics

This section gives a practical primer on Unreal Engine’s **MASS** Entity system. It focuses on the pieces you will touch most: data (Fragments), structure (Archetypes), behavior (Processors), and flow (Queries and Phases). It also includes patterns, pitfalls, and debugging tips.

---

## Core Building Blocks

### Entities
Lightweight IDs that reference a record in an archetype. Entities have no virtual methods or UObject overhead.

### Fragments
POD-style components that store per-entity data (structs).
- Kept small and cohesive; split unrelated concerns.
- Example: `FVelocityFragment`, `FHealthFragment`.

### Tags
Zero-sized markers used for fast include/exclude filtering or state flags.
- Example: `FDyingTag`, `FSelectedTag`.

### Shared Fragments
Data shared by all entities in a chunk (or group) to:
- Avoid duplication for constants or environmental parameters.
- Improve cache locality by grouping similar entities.
- Example: `FRogueTrackSharedFragment`, `FFormationShared`, `FPathSegmentShared`.

### Archetypes
A combination of fragments and tags. Entities with the same composition live together.
- Creation cost is higher than adding/removing tags, so avoid churn.
- Design archetypes around long-lived roles.

### Chunks
Contiguous blocks of entities within an archetype.
- Processors operate chunk-by-chunk for CPU cache efficiency.

### Entity Views
Typed views that provide structured access to fragments during processing.

---

## Behavior & Flow

### Queries
Declarative filters that specify:
- **All** fragments/tags required to read or write.
- **Any/None** constraints for conditional filtering.
- Example: “Entities with `FTransformFragment` + `FVelocityFragment` and not `FStunnedTag`.”

**Tip:** Mark read vs write access precisely to resolve dependencies and enable safe parallelism.

### Processors
Systems that execute queries over entity chunks.
- Derive from `UMassProcessor`.
- Declare **phase**, **execution flags**, and build a **FMassEntityQuery**.
- Keep them stateless when possible. Push persistent data into shared fragments or subsystems.

### Processing Phases
Standard **MASS** phases in UE 5.6 and when to use them.

- **PrePhysics**  
  Prepare simulation inputs. Read player/AI intent, advance purely kinematic sims, accumulate forces, set target states.  
  *Use for:* movement integration that does not need this frame’s physics result, AI scoring, headway prep, timers.  
  *Avoid:* reading physics results from this frame.

- **StartPhysics**  
  Sync from MASS to the physics scene before stepping. Write transforms that must affect the step, create/destroy constraints, apply impulses.  
  *Use for:* teleporting bodies, toggling sim state, pushing wakes/sleeps.  
  *Avoid:* heavy gameplay logic or queries unrelated to physics.

- **DuringPhysics**  
  Runs inside the physics step window. Rare. Only for processors that must couple tightly with the solver.  
  *Use for:* niche callbacks or custom sub-stepping hooks.  
  *Avoid:* anything that can run before or after; keep work minimal to prevent stalls.

- **EndPhysics**  
  Pull results from physics. Read contacts, sweeps, body poses; resolve collisions; convert results back to fragments/tags.  
  *Use for:* landing detection, hit processing, post-contact state changes.  
  *Avoid:* spawning/despawning or large structural changes.

- **PostPhysics**  
  Gameplay that depends on physics outcomes and broader sim updates. Good place to transition states, schedule representation, path updates, and LOD.  
  *Use for:* boarding/alighting decisions after stop, nav updates, visibility gating, cooldown ticks.  
  *Avoid:* writing things StartPhysics expects to be final.

- **FrameEnd**  
  Final housekeeping. Low-priority work that should not affect this frame’s sim. Safe for representation, replication packaging, metrics, and cleanup.  
  *Use for:* actor spawn/despawn mirroring, debug overlays, counters, buffer swaps.  
  *Avoid:* feeding data needed earlier in the same frame.

> Rule of Thumb: push data *to* physics in **StartPhysics**, pull data *from* physics in **EndPhysics**, and do gameplay that depends on physics in **PostPhysics**. Keep **DuringPhysics** exceptional and small.

### Process Groups & Execution Flags
- **ExecuteInGroup**: Group related processors to run together (e.g., Movement, Tasks).
- **ExecuteBefore/After**: Fine-tune order between specific processors.

---

### Execution Order & Dependencies
MASS computes order from read/write sets:
- Writers run before readers of the same fragment in the same phase.
- Prefer **many readers, few writers**.
- Split large systems into smaller processors if they write different fragments to unlock parallelism.

### Delegates, Signals, and Events
MASS itself is pull-based via processors. For “events”:
- Write **state fragments/tags** that other processors consume next frame.
- Use **subsystems** or **UObject** event hubs sparingly and only at chunk boundaries.

---

## World/Representation Integration

### Mass Actors & Representation
- Keep simulation in MASS. Mirror visuals with lightweight **Representation** processors.
- Spawn actors only for visible entities or editor tools.
- Use tags like `FVisibleTag` to gate expensive work.

### Spawning & Despawning
- Prefer **archetype factories** or **spawn processors** that ingest config assets.
- Despawn by removing from the entity subsystem; clean up representation in a separate pass.

### Replication
- Use **MassReplication** for large simulations.
- Keep replicated data small. Derive visuals client-side when possible.

---

## Performance Patterns
- **Fragment design**: group fields that change together; avoid “god fragments” with unrelated data.
- **Write minimal**: fewer writers per frame lowers order constraints.
- **Shared fragments**: cluster entities with the same read-only constants.
- **Archetype stability**: avoid frequent composition changes; use tags for transient state.
- **Parallel by chunk**: prefer more, smaller processors with clear read/write sets.
- **Avoid pointer chasing**: keep indices and small structs in tightly pack arrays, not UObjects, use entity ids over raw pointers.
- **Bit smart**: prefer uint16/uint8 where ranges allow, uint8 for boolean values.
- **Initialise**: build tables/arrays in subsystems at startup, not per-frame.

---

## Common Pitfalls
- **Over-coupled fragments**: one change forces many processors to serialize.
- **Write conflicts**: multiple writers for the same fragment in the same phase throttle parallelism.
- **Archetype churn**: constructing archetypes mid-frame is expensive.
- **Representation in sim loops**: actor operations in hot code paths cause stalls.
- **Hidden dependencies**: implicit ordering via subsystems leads to non-determinism. Prefer explicit fragment signals.

---

# Glossary
- **Alpha**: normalized position (0,1) along a closed track.
- **Headway**: distance or time spacing between successive trains.
- **Dwell**: time at station.
- **Entity**: ID pointing to data in an archetype.
- **Fragment**: Struct of data owned by an entity in MASS.
- **Shared Fragment**: Data shared by a chunk/group of entities.
- **Tag**: Zero-sized type used for fast inclusion/exclusion.
- **Processor**: System that runs queries over entity chunks.
- **Archetype**: Set of fragments/tags defining entity layout.
- **Chunk**: Contiguous storage unit inside an archetype.
- **Phase**: Scheduling bucket that orders processors.
- **Representation**: Mapping simulated entities to visuals.