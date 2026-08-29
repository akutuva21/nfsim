#include "profile.hh"

#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>

using namespace std;

namespace NFcore {

namespace {

void addPhaseTotal(map<string, double> &totals, const string &name, double value)
{
    if (value != 0.0) totals[name] += value;
}

unsigned int profileHistogramBucket(unsigned long long value)
{
    if (value == 0) return 0;

    unsigned int bucket = 0;
    while (value > 1 && bucket < PROFILE_HISTOGRAM_BUCKETS - 2) {
        value >>= 1;
        ++bucket;
    }
    return bucket + 1;
}

unsigned long long profileHistogramQuantile(
    const unsigned long long *hist,
    unsigned long long samples,
    unsigned long long numerator,
    unsigned long long denominator)
{
    if (hist == 0 || samples == 0 || denominator == 0 || numerator == 0)
        return 0;

    // Compute ceil(samples * numerator / denominator) without multiplying
    // the potentially large sample count by the numerator.
    unsigned long long whole = samples / denominator;
    unsigned long long remainder = samples % denominator;
    unsigned long long rank = whole >
        std::numeric_limits<unsigned long long>::max() / numerator
        ? std::numeric_limits<unsigned long long>::max()
        : whole * numerator;
    unsigned long long remainderProduct = remainder * numerator;
    rank += remainderProduct / denominator;
    if (remainderProduct % denominator != 0 &&
            rank != std::numeric_limits<unsigned long long>::max()) {
        ++rank;
    }
    if (rank == 0) rank = 1;

    unsigned long long cumulative = 0;
    for (unsigned int bucket = 0; bucket < PROFILE_HISTOGRAM_BUCKETS;
            ++bucket) {
        cumulative += hist[bucket];
        if (cumulative >= rank) {
            if (bucket == 0) return 0;
            if (bucket >= PROFILE_HISTOGRAM_BUCKETS - 1)
                return std::numeric_limits<unsigned long long>::max();
            return (1ULL << bucket) - 1ULL;
        }
    }
    return std::numeric_limits<unsigned long long>::max();
}

}

const char *profileConnectivityContextName(ProfileConnectivityContext context)
{
    switch (context) {
    case PROFILE_CONNECTIVITY_MATCHING:
        return "matching";
    case PROFILE_CONNECTIVITY_PRODUCT_PREPARATION:
        return "product_preparation";
    case PROFILE_CONNECTIVITY_TRANSFORMATION:
        return "transformation";
    case PROFILE_CONNECTIVITY_COMPLEX_MAINTENANCE:
        return "complex_maintenance";
    case PROFILE_CONNECTIVITY_LOCAL_FUNCTION:
        return "local_function";
    case PROFILE_CONNECTIVITY_OTHER:
    default:
        return "other";
    }
}

ProfileConnectivityStats::ProfileConnectivityStats()
    : traversalCalls(0), moleculeVisits(0), edgeVisits(0),
      elapsedSeconds(0.0), fireSamples(0), moleculeMaximum(0),
      edgeMaximum(0)
{
    for (unsigned int i = 0; i < PROFILE_HISTOGRAM_BUCKETS; ++i) {
        moleculeHistogram[i] = 0;
        edgeHistogram[i] = 0;
    }
}

ProfileReactionStats::ProfileReactionStats()
    : rxnId(-1), name(), fireCalls(0), nullEvents(0), candidateChecks(0),
      membershipUpdates(0), templateCompareCalls(0),
      connectivityMoleculeVisits(0), connectivityEdgeVisits(0), bindCalls(0),
      unbindCalls(0), complexMaintenanceCalls(0),
      complexMaintenanceMolecules(0), affectedComplexes(0),
      affectedComplexMolecules(0), canonicalLabelCalls(0),
      canonicalLabelNodes(0), canonicalLabelEdges(0), nautyCalls(0),
      mappingPushes(0), mappingPops(0), mappingRemoves(0),
      mappingConfirms(0), reactantListExpansions(0),
      reactantTreeExpansions(0), reactantListExpandedSlots(0),
      reactantTreeExpandedSlots(0), transformationCalls(0),
      productPreparationCalls(0), productPreparationMolecules(0),
      productCollectionCalls(0), productCollectionMolecules(0),
      observableRemovalMolecules(0), observableAdditionMolecules(0),
      fireCpuSeconds(0.0),
      membershipUpdateCpuSeconds(0.0),
      templateCompareCpuSeconds(0.0), connectivityCpuSeconds(0.0),
      bindCpuSeconds(0.0), unbindCpuSeconds(0.0),
      complexMaintenanceCpuSeconds(0.0), canonicalLabelCpuSeconds(0.0),
      reactantListExpansionCpuSeconds(0.0),
      reactantTreeExpansionCpuSeconds(0.0), transformationCpuSeconds(0.0),
      productPreparationCpuSeconds(0.0),
      productCollectionCpuSeconds(0.0), observableRemovalCpuSeconds(0.0),
      observableAdditionCpuSeconds(0.0) {}

NFsimProfile::NFsimProfile()
    : enabled(false), outputPath(), phaseCpuSeconds(), reactionStats(),
      activeReaction(false), activeStats(0), templateCompareDepth(0),
      templateCompareStart(),
      connectivityContext(PROFILE_CONNECTIVITY_OTHER)
{
    for (unsigned int i = 0; i < PROFILE_CONNECTIVITY_CONTEXT_COUNT; ++i) {
        activeConnectivityMolecules[i] = 0;
        activeConnectivityEdges[i] = 0;
    }
}

void NFsimProfile::enable(const string &path)
{
    outputPath = path;
    enabled = true;
    reset();
}

void NFsimProfile::reset()
{
    phaseCpuSeconds.clear();
    reactionStats.clear();
    activeReaction = false;
    activeStats = 0;
    templateCompareDepth = 0;
    templateCompareStart = ProfileTime();
    connectivityContext = PROFILE_CONNECTIVITY_OTHER;
    for (unsigned int i = 0; i < PROFILE_CONNECTIVITY_CONTEXT_COUNT; ++i) {
        activeConnectivityMolecules[i] = 0;
        activeConnectivityEdges[i] = 0;
    }
}

string NFsimProfile::cleanField(const string &value)
{
    string result = value;
    for (string::iterator it = result.begin(); it != result.end(); ++it) {
        if (*it == '\t' || *it == '\n' || *it == '\r') *it = ' ';
    }
    return result;
}

void NFsimProfile::setReactionIdentity(ProfileReactionStats &stats,
                                       int rxnId, const string &name)
{
    stats.rxnId = rxnId;
    if (stats.name.empty()) stats.name = name;
}

void NFsimProfile::recordPhase(const string &phase, clock_t elapsed)
{
    if (!enabled) return;
    phaseCpuSeconds[phase] += seconds(elapsed);
}

void NFsimProfile::beginReactionFire(int rxnId, const string &name)
{
    if (!enabled) return;
    ProfileReactionStats &stats = reactionStats[rxnId];
    setReactionIdentity(stats, rxnId, name);
    activeReaction = true;
    activeStats = &stats;
    connectivityContext = PROFILE_CONNECTIVITY_OTHER;
    for (unsigned int i = 0; i < PROFILE_CONNECTIVITY_CONTEXT_COUNT; ++i) {
        activeConnectivityMolecules[i] = 0;
        activeConnectivityEdges[i] = 0;
    }
}

void NFsimProfile::recordReactionFire(int rxnId, const string &name,
                                       clock_t elapsed, bool nullEvent)
{
    if (!enabled) return;
    ProfileReactionStats *activeStatsForFire = activeReaction && activeStats != 0
        ? activeStats
        : &reactionStats[rxnId];
    setReactionIdentity(*activeStatsForFire, rxnId, name);
    ProfileReactionStats &stats = *activeStatsForFire;
    ++stats.fireCalls;
    if (nullEvent) ++stats.nullEvents;
    stats.fireCpuSeconds += seconds(elapsed);
    if (activeReaction && activeStats == activeStatsForFire)
        recordConnectivityFireSamples(stats);
    activeReaction = false;
    activeStats = 0;
    connectivityContext = PROFILE_CONNECTIVITY_OTHER;
}

void NFsimProfile::recordMembershipPhase(double elapsed)
{
    if (!isReactionActive()) return;
    activeStats->membershipUpdateCpuSeconds += elapsed;
}

void NFsimProfile::recordConnectivity(double elapsed,
                                      unsigned long long moleculesVisited,
                                      unsigned long long edgeVisits,
                                      ProfileConnectivityContext context)
{
    if (!isReactionActive()) return;
    if (context < PROFILE_CONNECTIVITY_OTHER ||
            context >= PROFILE_CONNECTIVITY_CONTEXT_COUNT) {
        context = PROFILE_CONNECTIVITY_OTHER;
    }
    activeStats->connectivityMoleculeVisits += moleculesVisited;
    activeStats->connectivityEdgeVisits += edgeVisits;
    activeStats->connectivityCpuSeconds += elapsed;

    ProfileConnectivityStats &contextStats =
        activeStats->connectivityByContext[context];
    ++contextStats.traversalCalls;
    contextStats.moleculeVisits += moleculesVisited;
    contextStats.edgeVisits += edgeVisits;
    contextStats.elapsedSeconds += elapsed;
    activeConnectivityMolecules[context] += moleculesVisited;
    activeConnectivityEdges[context] += edgeVisits;
}

void NFsimProfile::recordConnectivityFireSamples(ProfileReactionStats &stats)
{
    for (unsigned int i = 0; i < PROFILE_CONNECTIVITY_CONTEXT_COUNT; ++i) {
        ProfileConnectivityStats &contextStats = stats.connectivityByContext[i];
        ++contextStats.fireSamples;
        const unsigned long long moleculeVisits = activeConnectivityMolecules[i];
        const unsigned long long edgeVisits = activeConnectivityEdges[i];
        if (moleculeVisits > contextStats.moleculeMaximum)
            contextStats.moleculeMaximum = moleculeVisits;
        if (edgeVisits > contextStats.edgeMaximum)
            contextStats.edgeMaximum = edgeVisits;
        ++contextStats.moleculeHistogram[
            profileHistogramBucket(moleculeVisits)];
        ++contextStats.edgeHistogram[profileHistogramBucket(edgeVisits)];
    }
}

void NFsimProfile::recordBind(double elapsed)
{
    if (!isReactionActive()) return;
    ++activeStats->bindCalls;
    activeStats->bindCpuSeconds += elapsed;
}

void NFsimProfile::recordUnbind(double elapsed)
{
    if (!isReactionActive()) return;
    ++activeStats->unbindCalls;
    activeStats->unbindCpuSeconds += elapsed;
}

void NFsimProfile::recordComplexMaintenance(double elapsed,
                                             unsigned long long moleculesTouched)
{
    if (!isReactionActive()) return;
    ++activeStats->complexMaintenanceCalls;
    activeStats->complexMaintenanceMolecules += moleculesTouched;
    activeStats->complexMaintenanceCpuSeconds += elapsed;
}

void NFsimProfile::recordAffectedComplexes(unsigned long long complexes,
                                           unsigned long long molecules)
{
    if (!isReactionActive()) return;
    activeStats->affectedComplexes += complexes;
    activeStats->affectedComplexMolecules += molecules;
}

void NFsimProfile::recordCanonicalLabel(double elapsed,
                                        unsigned long long nodes,
                                        unsigned long long edges,
                                        bool nautyCalled)
{
    if (!isReactionActive()) return;
    ++activeStats->canonicalLabelCalls;
    activeStats->canonicalLabelNodes += nodes;
    activeStats->canonicalLabelEdges += edges;
    if (nautyCalled) ++activeStats->nautyCalls;
    activeStats->canonicalLabelCpuSeconds += elapsed;
}

void NFsimProfile::recordReactantListExpansion(unsigned long long expandedSlots,
                                               double elapsed)
{
    if (!isReactionActive()) return;
    ++activeStats->reactantListExpansions;
    activeStats->reactantListExpandedSlots += expandedSlots;
    activeStats->reactantListExpansionCpuSeconds += elapsed;
}

void NFsimProfile::recordReactantTreeExpansion(unsigned long long expandedSlots,
                                               double elapsed)
{
    if (!isReactionActive()) return;
    ++activeStats->reactantTreeExpansions;
    activeStats->reactantTreeExpandedSlots += expandedSlots;
    activeStats->reactantTreeExpansionCpuSeconds += elapsed;
}

void NFsimProfile::recordTransformation(double elapsed)
{
    if (!isReactionActive()) return;
    ++activeStats->transformationCalls;
    activeStats->transformationCpuSeconds += elapsed;
}

void NFsimProfile::recordProductPreparation(double elapsed,
                                             unsigned long long moleculesPrepared)
{
    if (!isReactionActive()) return;
    ++activeStats->productPreparationCalls;
    activeStats->productPreparationMolecules += moleculesPrepared;
    activeStats->productPreparationCpuSeconds += elapsed;
}

void NFsimProfile::recordProductCollection(double elapsed,
                                           unsigned long long moleculesAdded)
{
    if (!isReactionActive()) return;
    ++activeStats->productCollectionCalls;
    activeStats->productCollectionMolecules += moleculesAdded;
    activeStats->productCollectionCpuSeconds += elapsed;
}

void NFsimProfile::recordObservableRemoval(double elapsed,
                                           unsigned long long molecules)
{
    if (!isReactionActive()) return;
    activeStats->observableRemovalMolecules += molecules;
    activeStats->observableRemovalCpuSeconds += elapsed;
}

void NFsimProfile::recordObservableAddition(double elapsed,
                                            unsigned long long molecules)
{
    if (!isReactionActive()) return;
    activeStats->observableAdditionMolecules += molecules;
    activeStats->observableAdditionCpuSeconds += elapsed;
}

bool NFsimProfile::write() const
{
    if (!enabled) return true;
    if (outputPath.empty() || outputPath == "-") {
        write(cout);
        return static_cast<bool>(cout);
    }

    ofstream output(outputPath.c_str());
    if (!output) return false;
    write(output);
    return static_cast<bool>(output);
}

void NFsimProfile::write(ostream &output) const
{
    output << "# NFsim opt-in profile v3\n";
    output << "# fire_cpu_seconds and broad phases use process CPU clock() seconds.\n";
    output << "# Component timing fields use low-overhead steady elapsed seconds.\n";
    output << "# Nested phase times can overlap; use them for attribution, not summation.\n";
    output << "# candidate_checks counts tryToAdd gateway evaluations caused by the fired rule.\n";
    output << "# membership_updates counts product-molecule membership refreshes attributed to the fired rule.\n";
    output << "# membership_update_seconds measures those refreshes, including dependent reaction-list work.\n";
    output << "# template_compare_calls counts recursive TemplateMolecule::compare invocations.\n";
    output << "# connectivity_edge_visits counts bonded-neighbor examinations (each bond can be seen twice).\n";
    output << "# connectivity_context rows split traversals by their active caller context.\n";
    output << "# Context histograms are bounded power-of-two bins; P50/P90/P99 are inclusive upper bounds.\n";
    output << "# Context distribution samples include fires with zero traversal work.\n";
    output << "# canonical_label_calls counts generated labels, not cache hits; nauty_calls counts actual Nauty calls.\n";
    output << "# mapping operation counts are container-operation calls and may include nested clone cleanup.\n";
    output << "# transformation_seconds covers TransformationSet::transform; product collection is reported separately.\n";
    output << "phase\tname\tseconds\n";
    output << setprecision(12);
    map<string, double> phaseTotals = phaseCpuSeconds;
    for (map<int, ProfileReactionStats>::const_iterator it = reactionStats.begin();
         it != reactionStats.end(); ++it) {
        const ProfileReactionStats &stats = it->second;
        addPhaseTotal(phaseTotals, "reaction_fire", stats.fireCpuSeconds);
        addPhaseTotal(phaseTotals, "membership_update",
                      stats.membershipUpdateCpuSeconds);
        addPhaseTotal(phaseTotals, "template_compare",
                      stats.templateCompareCpuSeconds);
        addPhaseTotal(phaseTotals, "connectivity_traversal",
                      stats.connectivityCpuSeconds);
        addPhaseTotal(phaseTotals, "bind", stats.bindCpuSeconds);
        addPhaseTotal(phaseTotals, "unbind", stats.unbindCpuSeconds);
        addPhaseTotal(phaseTotals, "complex_maintenance",
                      stats.complexMaintenanceCpuSeconds);
        addPhaseTotal(phaseTotals, "canonical_label",
                      stats.canonicalLabelCpuSeconds);
        addPhaseTotal(phaseTotals, "reactant_list_expansion",
                      stats.reactantListExpansionCpuSeconds);
        addPhaseTotal(phaseTotals, "reactant_tree_expansion",
                      stats.reactantTreeExpansionCpuSeconds);
        addPhaseTotal(phaseTotals, "transformation",
                      stats.transformationCpuSeconds);
        addPhaseTotal(phaseTotals, "product_preparation",
                      stats.productPreparationCpuSeconds);
        addPhaseTotal(phaseTotals, "product_collection",
                      stats.productCollectionCpuSeconds);
        addPhaseTotal(phaseTotals, "observable_removal",
                      stats.observableRemovalCpuSeconds);
        addPhaseTotal(phaseTotals, "observable_addition",
                      stats.observableAdditionCpuSeconds);
    }
    for (map<string, double>::const_iterator it = phaseTotals.begin();
         it != phaseTotals.end(); ++it) {
        output << "phase\t" << cleanField(it->first) << "\t" << it->second << "\n";
    }

    output << "reaction\trx_id\tname\tfire_calls\tnull_events\tfire_cpu_seconds"
           << "\tcandidate_checks\tmembership_updates\ttemplate_compare_calls"
           << "\tmembership_update_seconds"
           << "\ttemplate_compare_seconds\tconnectivity_molecule_visits"
           << "\tconnectivity_edge_visits\tconnectivity_seconds\tbind_calls"
           << "\tbind_seconds\tunbind_calls\tunbind_seconds"
           << "\tcomplex_maintenance_calls\tcomplex_maintenance_molecules"
           << "\tcomplex_maintenance_seconds\taffected_complexes"
           << "\taffected_complex_molecules\tcanonical_label_calls"
           << "\tcanonical_label_nodes\tcanonical_label_edges\tnauty_calls"
           << "\tcanonical_label_seconds\tmapping_pushes\tmapping_pops"
           << "\tmapping_removes\tmapping_confirms\treactant_list_expansions"
           << "\treactant_list_expanded_slots\treactant_list_expansion_seconds"
           << "\treactant_tree_expansions\treactant_tree_expanded_slots"
           << "\treactant_tree_expansion_seconds\tcandidate_checks_per_fire"
           << "\ttransformation_calls\ttransformation_seconds"
           << "\tproduct_preparation_calls\tproduct_preparation_molecules"
           << "\tproduct_preparation_seconds"
           << "\tproduct_collection_calls\tproduct_collection_molecules"
           << "\tproduct_collection_seconds"
           << "\tobservable_removal_molecules\tobservable_removal_seconds"
           << "\tobservable_addition_molecules\tobservable_addition_seconds"
           << "\tmembership_updates_per_fire\ttemplate_compare_calls_per_fire"
           << "\tconnectivity_molecule_visits_per_fire"
           << "\tconnectivity_edge_visits_per_fire\taffected_complexes_per_fire"
           << "\taffected_complex_molecules_per_fire\n";
    for (map<int, ProfileReactionStats>::const_iterator it = reactionStats.begin();
         it != reactionStats.end(); ++it) {
        const ProfileReactionStats &stats = it->second;
        double candidatesPerFire = stats.fireCalls == 0
            ? 0.0
            : static_cast<double>(stats.candidateChecks) /
              static_cast<double>(stats.fireCalls);
        double membershipsPerFire = stats.fireCalls == 0
            ? 0.0
            : static_cast<double>(stats.membershipUpdates) /
              static_cast<double>(stats.fireCalls);
        double templateComparesPerFire = stats.fireCalls == 0
            ? 0.0
            : static_cast<double>(stats.templateCompareCalls) /
              static_cast<double>(stats.fireCalls);
        double connectivityMoleculesPerFire = stats.fireCalls == 0
            ? 0.0
            : static_cast<double>(stats.connectivityMoleculeVisits) /
              static_cast<double>(stats.fireCalls);
        double connectivityEdgesPerFire = stats.fireCalls == 0
            ? 0.0
            : static_cast<double>(stats.connectivityEdgeVisits) /
              static_cast<double>(stats.fireCalls);
        double affectedComplexesPerFire = stats.fireCalls == 0
            ? 0.0
            : static_cast<double>(stats.affectedComplexes) /
              static_cast<double>(stats.fireCalls);
        double affectedComplexMoleculesPerFire = stats.fireCalls == 0
            ? 0.0
            : static_cast<double>(stats.affectedComplexMolecules) /
              static_cast<double>(stats.fireCalls);
        output << "reaction\t" << stats.rxnId << "\t" << cleanField(stats.name)
               << "\t" << stats.fireCalls << "\t" << stats.nullEvents
               << "\t" << stats.fireCpuSeconds << "\t" << stats.candidateChecks
               << "\t" << stats.membershipUpdates
               << "\t" << stats.templateCompareCalls
               << "\t" << stats.membershipUpdateCpuSeconds
               << "\t" << stats.templateCompareCpuSeconds
               << "\t" << stats.connectivityMoleculeVisits
               << "\t" << stats.connectivityEdgeVisits
               << "\t" << stats.connectivityCpuSeconds
               << "\t" << stats.bindCalls << "\t" << stats.bindCpuSeconds
               << "\t" << stats.unbindCalls << "\t" << stats.unbindCpuSeconds
               << "\t" << stats.complexMaintenanceCalls
               << "\t" << stats.complexMaintenanceMolecules
               << "\t" << stats.complexMaintenanceCpuSeconds
               << "\t" << stats.affectedComplexes
               << "\t" << stats.affectedComplexMolecules
               << "\t" << stats.canonicalLabelCalls
               << "\t" << stats.canonicalLabelNodes
               << "\t" << stats.canonicalLabelEdges
               << "\t" << stats.nautyCalls
               << "\t" << stats.canonicalLabelCpuSeconds
               << "\t" << stats.mappingPushes << "\t" << stats.mappingPops
               << "\t" << stats.mappingRemoves << "\t" << stats.mappingConfirms
               << "\t" << stats.reactantListExpansions
               << "\t" << stats.reactantListExpandedSlots
               << "\t" << stats.reactantListExpansionCpuSeconds
               << "\t" << stats.reactantTreeExpansions
               << "\t" << stats.reactantTreeExpandedSlots
               << "\t" << stats.reactantTreeExpansionCpuSeconds
               << "\t" << candidatesPerFire
               << "\t" << stats.transformationCalls
               << "\t" << stats.transformationCpuSeconds
               << "\t" << stats.productPreparationCalls
               << "\t" << stats.productPreparationMolecules
               << "\t" << stats.productPreparationCpuSeconds
               << "\t" << stats.productCollectionCalls
               << "\t" << stats.productCollectionMolecules
               << "\t" << stats.productCollectionCpuSeconds
               << "\t" << stats.observableRemovalMolecules
               << "\t" << stats.observableRemovalCpuSeconds
               << "\t" << stats.observableAdditionMolecules
               << "\t" << stats.observableAdditionCpuSeconds
               << "\t" << membershipsPerFire
               << "\t" << templateComparesPerFire
               << "\t" << connectivityMoleculesPerFire
               << "\t" << connectivityEdgesPerFire
               << "\t" << affectedComplexesPerFire
               << "\t" << affectedComplexMoleculesPerFire << "\n";
    }

    output << "connectivity_context\trx_id\tname\tcontext\ttraversal_calls"
           << "\tmolecule_visits\tedge_visits\tseconds\tfire_samples"
           << "\tmolecule_mean_per_fire\tmolecule_p50_upper_bound"
           << "\tmolecule_p90_upper_bound\tmolecule_p99_upper_bound"
           << "\tmolecule_max\tedge_mean_per_fire\tedge_p50_upper_bound"
           << "\tedge_p90_upper_bound\tedge_p99_upper_bound\tedge_max\n";
    for (map<int, ProfileReactionStats>::const_iterator it = reactionStats.begin();
         it != reactionStats.end(); ++it) {
        const ProfileReactionStats &stats = it->second;
        for (unsigned int context = 0;
                context < PROFILE_CONNECTIVITY_CONTEXT_COUNT; ++context) {
            const ProfileConnectivityStats &contextStats =
                stats.connectivityByContext[context];
            double moleculeMeanPerFire = contextStats.fireSamples == 0
                ? 0.0
                : static_cast<double>(contextStats.moleculeVisits) /
                  static_cast<double>(contextStats.fireSamples);
            double edgeMeanPerFire = contextStats.fireSamples == 0
                ? 0.0
                : static_cast<double>(contextStats.edgeVisits) /
                  static_cast<double>(contextStats.fireSamples);
            unsigned long long moleculeP50 = profileHistogramQuantile(
                contextStats.moleculeHistogram, contextStats.fireSamples, 50, 100);
            unsigned long long moleculeP90 = profileHistogramQuantile(
                contextStats.moleculeHistogram, contextStats.fireSamples, 90, 100);
            unsigned long long moleculeP99 = profileHistogramQuantile(
                contextStats.moleculeHistogram, contextStats.fireSamples, 99, 100);
            unsigned long long edgeP50 = profileHistogramQuantile(
                contextStats.edgeHistogram, contextStats.fireSamples, 50, 100);
            unsigned long long edgeP90 = profileHistogramQuantile(
                contextStats.edgeHistogram, contextStats.fireSamples, 90, 100);
            unsigned long long edgeP99 = profileHistogramQuantile(
                contextStats.edgeHistogram, contextStats.fireSamples, 99, 100);
            output << "connectivity_context\t" << stats.rxnId
                   << "\t" << cleanField(stats.name)
                   << "\t" << profileConnectivityContextName(
                       static_cast<ProfileConnectivityContext>(context))
                   << "\t" << contextStats.traversalCalls
                   << "\t" << contextStats.moleculeVisits
                   << "\t" << contextStats.edgeVisits
                   << "\t" << contextStats.elapsedSeconds
                   << "\t" << contextStats.fireSamples
                   << "\t" << moleculeMeanPerFire
                   << "\t" << moleculeP50
                   << "\t" << moleculeP90
                   << "\t" << moleculeP99
                   << "\t" << contextStats.moleculeMaximum
                   << "\t" << edgeMeanPerFire
                   << "\t" << edgeP50
                   << "\t" << edgeP90
                   << "\t" << edgeP99
                   << "\t" << contextStats.edgeMaximum << "\n";
        }
    }
}

}
