#ifndef NFSIM_PROFILE_HH
#define NFSIM_PROFILE_HH

#include <chrono>
#include <ctime>
#include <map>
#include <ostream>
#include <string>

namespace NFcore {

typedef std::chrono::steady_clock::time_point ProfileTime;

inline ProfileTime profileNow()
{
    return std::chrono::steady_clock::now();
}

inline double profileElapsedSeconds(const ProfileTime &start)
{
    return std::chrono::duration<double>(profileNow() - start).count();
}

struct ProfileReactionStats {
    ProfileReactionStats();

    int rxnId;
    std::string name;
    unsigned long long fireCalls;
    unsigned long long nullEvents;
    unsigned long long candidateChecks;
    unsigned long long membershipUpdates;
    unsigned long long templateCompareCalls;
    unsigned long long connectivityMoleculeVisits;
    unsigned long long connectivityEdgeVisits;
    unsigned long long bindCalls;
    unsigned long long unbindCalls;
    unsigned long long complexMaintenanceCalls;
    unsigned long long complexMaintenanceMolecules;
    unsigned long long affectedComplexes;
    unsigned long long affectedComplexMolecules;
    unsigned long long canonicalLabelCalls;
    unsigned long long canonicalLabelNodes;
    unsigned long long canonicalLabelEdges;
    unsigned long long nautyCalls;
    unsigned long long mappingPushes;
    unsigned long long mappingPops;
    unsigned long long mappingRemoves;
    unsigned long long mappingConfirms;
    unsigned long long reactantListExpansions;
    unsigned long long reactantTreeExpansions;
    unsigned long long reactantListExpandedSlots;
    unsigned long long reactantTreeExpandedSlots;
    unsigned long long transformationCalls;
    unsigned long long productPreparationCalls;
    unsigned long long productPreparationMolecules;
    unsigned long long productCollectionCalls;
    unsigned long long productCollectionMolecules;
    unsigned long long observableRemovalMolecules;
    unsigned long long observableAdditionMolecules;
    double fireCpuSeconds;
    double membershipUpdateCpuSeconds;
    double templateCompareCpuSeconds;
    double connectivityCpuSeconds;
    double bindCpuSeconds;
    double unbindCpuSeconds;
    double complexMaintenanceCpuSeconds;
    double canonicalLabelCpuSeconds;
    double reactantListExpansionCpuSeconds;
    double reactantTreeExpansionCpuSeconds;
    double transformationCpuSeconds;
    double productPreparationCpuSeconds;
    double productCollectionCpuSeconds;
    double observableRemovalCpuSeconds;
    double observableAdditionCpuSeconds;
};

class NFsimProfile {
public:
    NFsimProfile();

    void enable(const std::string &outputPath);
    bool isEnabled() const { return enabled; }
    void reset();

    void recordPhase(const std::string &phase, clock_t elapsed);
    void beginReactionFire(int rxnId, const std::string &name);
    void recordReactionFire(int rxnId, const std::string &name,
                            clock_t elapsed, bool nullEvent);
    void recordMatchCandidate() {
        if (!enabled || !activeReaction || activeStats == 0) return;
        ++activeStats->candidateChecks;
    }
    void recordMembershipUpdate() {
        if (!enabled || !activeReaction || activeStats == 0) return;
        ++activeStats->membershipUpdates;
    }
    void recordMembershipPhase(double elapsed);
    bool isReactionActive() const {
        return enabled && activeReaction && activeStats != 0;
    }
    void beginTemplateCompare() {
        if (!isReactionActive()) return;
        ++activeStats->templateCompareCalls;
        if (templateCompareDepth == 0) templateCompareStart = profileNow();
        ++templateCompareDepth;
    }
    void endTemplateCompare() {
        if (!isReactionActive() || templateCompareDepth <= 0) return;
        --templateCompareDepth;
        if (templateCompareDepth == 0) {
            double elapsed = profileElapsedSeconds(templateCompareStart);
            activeStats->templateCompareCpuSeconds += elapsed;
            templateCompareStart = ProfileTime();
        }
    }
    void recordConnectivity(double elapsed,
                            unsigned long long moleculesVisited,
                            unsigned long long edgeVisits);
    void recordBind(double elapsed);
    void recordUnbind(double elapsed);
    void recordComplexMaintenance(double elapsed,
                                  unsigned long long moleculesTouched);
    void recordAffectedComplexes(unsigned long long complexes,
                                 unsigned long long molecules);
    void recordCanonicalLabel(double elapsed,
                              unsigned long long nodes,
                              unsigned long long edges,
                              bool nautyCalled);
    void recordMappingPush() {
        if (!isReactionActive()) return;
        ++activeStats->mappingPushes;
    }
    void recordMappingPop() {
        if (!isReactionActive()) return;
        ++activeStats->mappingPops;
    }
    void recordMappingRemove() {
        if (!isReactionActive()) return;
        ++activeStats->mappingRemoves;
    }
    void recordMappingConfirm() {
        if (!isReactionActive()) return;
        ++activeStats->mappingConfirms;
    }
    void recordReactantListExpansion(unsigned long long expandedSlots,
                                     double elapsed);
    void recordReactantTreeExpansion(unsigned long long expandedSlots,
                                     double elapsed);
    void recordTransformation(double elapsed);
    void recordProductPreparation(double elapsed,
                                  unsigned long long moleculesPrepared);
    void recordProductCollection(double elapsed,
                                unsigned long long moleculesAdded);
    void recordObservableRemoval(double elapsed,
                                 unsigned long long molecules);
    void recordObservableAddition(double elapsed,
                                  unsigned long long molecules);

    bool write() const;
    void write(std::ostream &output) const;

private:
    static double seconds(clock_t elapsed) {
        return static_cast<double>(elapsed) / static_cast<double>(CLOCKS_PER_SEC);
    }
    static std::string cleanField(const std::string &value);
    static void setReactionIdentity(ProfileReactionStats &stats,
                                    int rxnId, const std::string &name);

    bool enabled;
    std::string outputPath;
    std::map<std::string, double> phaseCpuSeconds;
    std::map<int, ProfileReactionStats> reactionStats;
    bool activeReaction;
    ProfileReactionStats *activeStats;
    int templateCompareDepth;
    ProfileTime templateCompareStart;
};

}

#endif
