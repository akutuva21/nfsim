#!/usr/bin/env python3
"""Structural audit for the retained Rasi/NFsim optimization lineage.

This intentionally checks implementation anchors, not benchmark claims.  It is
useful when moving patches between branches: if a merge silently drops one of
the optimized representations/paths, this script fails loudly.
"""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]

CHECKS = [
    ("membership filter", "src/NFcore/moleculeType.cpp", "NFSIM_MEMFILTER"),
    ("differential oracle", "src/NFcore/moleculeType.cpp", "NFSIM_MEMFILTER_DIFF"),
    ("adaptive threshold", "src/NFcore/moleculeType.cpp", "NFSIM_MEMFILTER_MIN_REGISTRATIONS"),
    ("propensity predicate", "src/NFreactions/reactions/reaction.hh", "propensityDependsOnlyOnMembership"),
    ("event mutation capture", "src/NFcore/NFcore.hh", "MembershipEventMutation"),
    ("event candidate plan", "src/NFcore/NFcore.hh", "membershipEventCandidatePlan"),
    ("pre-resolved gain lookup", "src/NFcore/NFcore.hh", "MembershipGainLookup"),
    ("root occupancy masks", "src/NFcore/NFcore.hh", "membershipRootRequiredBoundMasks"),
    ("occupancy views", "src/NFcore/NFcore.hh", "byBoundMask"),
    ("common-root proof", "src/NFcore/NFcore.hh", "hasCommonRootTopology"),
    ("occupancy proof propagation", "src/NFcore/moleculeType.cpp", "skipOccupancyMasks"),
    ("loss bitmap", "src/NFcore/NFcore.hh", "membershipLossCandidateBitmaps"),
    ("32-bit candidate stamps", "src/NFcore/NFcore.hh", "vector<std::uint32_t> membershipCandidateSeen"),
    ("lazy paged mappings", "src/NFcore/NFcore.hh", "class PagedMappingIdTable"),
    ("sparse bonded components", "src/NFcore/NFcore.hh", "bondedComponentIndices"),
    ("indexed reaction matcher", "src/NFreactions/reactions/reaction.cpp", "tryToAddWithIndex"),
    ("compiled reaction matcher", "src/NFcore/templateMolecule.cpp", "matchesCompiledSimple"),
    ("late MappingSet materialization", "src/NFreactions/reactions/reaction.cpp", "Match before activating a MappingSet"),
    ("unchanged mapping reuse", "src/NFreactions/reactions/reaction.cpp", "reusedMappingId"),
    ("compiled observable matcher", "src/NFcore/observable.cpp", "matchesCompiledSimple"),
    ("sparse selector bits", "src/NFcore/reactionSelector/directSelector.cpp", "activeReactionBits"),
    ("selector block override", "src/NFcore/reactionSelector/directSelector.cpp", "NFSIM_SELECTOR_BLOCK_SIZE"),
    ("streamed reaction XML", "src/NFinput/NFinput.cpp", "initReactionRulesStreamed"),
    ("initial candidate cache", "src/NFcore/moleculeType.cpp", "initialCandidateCache"),
    ("single-mapping reactant-tree path", "src/NFreactions/reactantLists/reactantTree.cpp", "singleMappingFastPath"),
    ("shared product-node pool", "src/NFcore/reactionClass.cpp", "NFSIM_PRODUCT_NODE_REUSE"),
]

failed = []
for label, rel, token in CHECKS:
    path = ROOT / rel
    try:
        text = path.read_text(errors="replace")
    except OSError as exc:
        failed.append((label, rel, f"cannot read: {exc}"))
        continue
    if token not in text:
        failed.append((label, rel, f"missing token {token!r}"))

# Also verify the retained defaults that distinguish the validated endpoint from
# rejected late experiments.
mtype = (ROOT / "src/NFcore/moleculeType.cpp").read_text(errors="replace")
if not re.search(
        r'generalMembershipFilterTuning\(\s*'
        r'"NFSIM_MEMFILTER_LOSS_BITMAP_MIN"\s*,\s*64\s*\)', mtype):
    failed.append(("validated loss bitmap cutoff", "src/NFcore/moleculeType.cpp", "expected cutoff 64"))
if not re.search(
        r'generalMembershipFilterTuning\(\s*'
        r'"NFSIM_MEMFILTER_LOSS_ACTIVE_MULTIPLIER"\s*,\s*8\s*\)', mtype):
    failed.append(("validated active-side crossover", "src/NFcore/moleculeType.cpp", "expected factor 8"))

selector = (ROOT / "src/NFcore/reactionSelector/directSelector.cpp").read_text(errors="replace")
if "selectionBlockSize = 128" not in selector:
    failed.append(("validated selector default", "src/NFcore/reactionSelector/directSelector.cpp", "expected 128"))

if failed:
    print("Rasi optimization structural audit: FAIL")
    for label, rel, reason in failed:
        print(f"  - {label}: {rel}: {reason}")
    sys.exit(1)

print(f"Rasi optimization structural audit: PASS ({len(CHECKS) + 3} checks)")
