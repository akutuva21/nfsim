# Rasi / translation optimization feature audit

This document maps the retained NFsim optimization lineage to the implementation
that is already present in this source tree. It exists to prevent a future
handoff from mistaking the optimized snapshot for an older pre-iteration23 tree.

## Runtime membership engine

| Retained optimization | Implementation anchor |
| --- | --- |
| Dependency-aware membership filter | `src/NFcore/moleculeType.cpp`: `generalMembershipFilterEnabled`, `buildMembershipDependencyIndex`, `prepareMembershipCandidates` |
| Differential correctness oracle | `NFSIM_MEMFILTER_DIFF`, membership differential path in `MoleculeType::updateRxnMembership` |
| Adaptive small-model bypass | `generalMembershipFilterMinRegistrations()`; default 16; `NFSIM_MEMFILTER_FORCE` override |
| Event mutation capture | `MembershipEventMutation` in `src/NFcore/NFcore.hh`; capture methods in `src/NFcore/system.cpp` |
| Direction-aware state/bond/topology candidates | dependency maps and event-plan construction in `MoleculeType::buildMembershipDependencyIndex` / `prepareMembershipEventPlan` |
| Sparse active-membership loss intersection | `Molecule::activeReactionMembershipIndices` and `MoleculeType::appendMembershipCandidateVector` |
| Semantic propensity guard | `ReactionClass::propensityDependsOnlyOnMembership()` and overrides |
| Root-local necessary conditions | `membershipRootContexts`, `membershipRootContextMatches`, `membershipRootPredicateMatches` |
| Compiled root occupancy masks | `membershipRootRequiredBoundMasks`, `membershipRootRequiredFreeMasks` |
| Occupancy-specialized candidate views | `MembershipCandidateView::byBoundMask` |
| Common-root topology proof/skip | `MembershipCandidateView::hasCommonRootTopology`; `skipFirstPredicate` |
| Propagate proven occupancy | `skipOccupancyMasks` / `occupancyProven` in candidate application |
| Pre-resolved gain lookup | `MembershipGainLookup`, `membership*GainLookups` |
| Loss-side static bitmaps | `membershipLossCandidateBitmaps`; current validated cutoff 64 |
| 32-bit hot generation stamps | `vector<std::uint32_t> membershipCandidateSeen`, `membershipRootContextChecked` |
| Event-level candidate plan | `membershipEventCandidatePlan`, `membershipEventPlanGeneration` |
| Initial population root filtering/cache | `sparseInitialMembership`, `initialCandidateCache` in `MoleculeType::prepareForSimulation` |

## Mapping / memory representation

| Retained optimization | Implementation anchor |
| --- | --- |
| Sparse `MappingIdSet` representation | compact `MappingIdSet` in `src/NFcore/NFcore.hh` |
| Lazy paged mapping table | `PagedMappingIdTable`; default `PAGE_SHIFT=5` (32 mapping slots/page) |
| Two-level lazy mapping directory | `PagedMappingIdTable::PageGroup` |
| Empty-page reclamation | `PagedMappingIdTable::noteBecameEmpty` / page deletion |
| Sparse active reaction memberships | `activeReactionMembershipIndices` |
| Template symmetry allocation cleanup | conditional `compIsAlwaysMapped` / symmetric structures in `templateMolecule.cpp` |
| Fused molecule per-site allocation | `Molecule::siteBlock` in `molecule.cpp` |

## Graph / matcher optimizations

| Retained optimization | Implementation anchor |
| --- | --- |
| Sparse bonded-component traversal | `Molecule::bondedComponentIndices`; bind/unbind maintenance and traversal users |
| Indexed reaction matching | `BasicRxnClass::tryToAddWithIndex` |
| Conservative compiled simple matcher | `TemplateMolecule::matchesCompiledSimple` |
| Late `MappingSet` materialization | `BasicRxnClass::tryToAddWithIndex`: match before `pushNextAvailableMappingSet()` on compiled path |
| Reuse unchanged live mappings | `compiledSimpleMappingEquals`, `reusedMappingId` in `reaction.cpp` |
| Rootfast endpoint ordering | partner component checked before partner pointer/type in root topology matching |
| Single-mapping `ReactantTree` fast path | `singleMappingFastPath` in `reactantTree.cpp`/`.hh` |

## Observable / selector optimizations

| Retained optimization | Implementation anchor |
| --- | --- |
| Compiled molecule-observable matcher | `MoleculesObservable::isObservable(Molecule*)` calls `matchesCompiledSimple` |
| Generic observable fallback | same function falls back to `TemplateMolecule::compare` when compiled subset is unsafe |
| DirectSelector active-bit enumeration | `activeReactionBits` in `directSelector.cpp` |
| Tunable selector block size | `NFSIM_SELECTOR_BLOCK_SIZE`; retained default 128 |

## Startup / input groundwork

| Retained optimization | Implementation anchor |
| --- | --- |
| Streamed reaction-rule XML parsing | `NFinput::initReactionRulesStreamed` |
| Dependency-aware standard build | CMake generator dependency tracking (native CMake behavior); legacy generated makefiles also include dependency variables |
| Initial identical-state candidate reuse | `initialCandidateCache` |

## Validation status encoded by this snapshot

The source includes the mechanisms described in the recovered
`iteration23_obscompiled` status, including the late mapping and observable
changes that were previously feared lost:

- late `MappingSet` materialization;
- reuse of unchanged compiled-simple mappings;
- occupancy-proof propagation;
- compiled molecule-observable matching;
- compiled reaction matching;
- loss-side static bitmaps;
- common-root topology proof/skip.

The validated iteration23 loss policy remains unchanged:

```text
loss bitmap cutoff: 64 entries
active-side crossover: candidate_size > 8 * active_membership_size
```

The later experimental 1x crossover was not retained. The proposed lower-bitmap-
cutoff/crossover matrix was also not promoted without another Rasi/uORF gate.

## Quick automated audit

Run:

```bash
python3 tools/verify_rasi_optimization_features.py
```

A zero exit status means all retained implementation anchors above were found.
This is a structural audit, not a semantic replacement for fixed-seed/differential
model validation.
