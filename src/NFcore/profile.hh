#ifndef NFSIM_PROFILE_HH
#define NFSIM_PROFILE_HH

#include <ctime>
#include <map>
#include <ostream>
#include <string>

namespace NFcore {

struct ProfileReactionStats {
    ProfileReactionStats();

    int rxnId;
    std::string name;
    unsigned long long fireCalls;
    unsigned long long nullEvents;
    unsigned long long candidateChecks;
    unsigned long long membershipUpdates;
    double fireCpuSeconds;
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
    void recordMatchCandidate();
    void recordMembershipUpdate();

    bool write() const;
    void write(std::ostream &output) const;

private:
    static double seconds(clock_t elapsed);
    static std::string cleanField(const std::string &value);
    static void setReactionIdentity(ProfileReactionStats &stats,
                                    int rxnId, const std::string &name);

    bool enabled;
    std::string outputPath;
    std::map<std::string, double> phaseCpuSeconds;
    std::map<int, ProfileReactionStats> reactionStats;
    bool activeReaction;
    ProfileReactionStats *activeStats;
};

}

#endif
