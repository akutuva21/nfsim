#include "profile.hh"

#include <fstream>
#include <iomanip>
#include <iostream>

using namespace std;

namespace NFcore {

ProfileReactionStats::ProfileReactionStats()
    : rxnId(-1), name(), fireCalls(0), nullEvents(0), candidateChecks(0),
      membershipUpdates(0), fireCpuSeconds(0.0) {}

NFsimProfile::NFsimProfile()
    : enabled(false), outputPath(), phaseCpuSeconds(), reactionStats(),
      activeReaction(false), activeStats(0) {}

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
}

double NFsimProfile::seconds(clock_t elapsed)
{
    return static_cast<double>(elapsed) / static_cast<double>(CLOCKS_PER_SEC);
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
    phaseCpuSeconds["reaction_fire"] += seconds(elapsed);
    activeReaction = false;
    activeStats = 0;
}

void NFsimProfile::recordMatchCandidate()
{
    if (!enabled || !activeReaction || activeStats == 0) return;
    ++activeStats->candidateChecks;
}

void NFsimProfile::recordMembershipUpdate()
{
    if (!enabled || !activeReaction || activeStats == 0) return;
    ++activeStats->membershipUpdates;
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
    output << "# NFsim opt-in profile v1\n";
    output << "# Times use process CPU clock() seconds.\n";
    output << "# candidate_checks counts tryToAdd gateway evaluations caused by the fired rule.\n";
    output << "# membership_updates counts product-molecule membership refreshes attributed to the fired rule.\n";
    output << "phase\tname\tcpu_seconds\n";
    output << setprecision(12);
    for (map<string, double>::const_iterator it = phaseCpuSeconds.begin();
         it != phaseCpuSeconds.end(); ++it) {
        output << "phase\t" << cleanField(it->first) << "\t" << it->second << "\n";
    }

    output << "reaction\trx_id\tname\tfire_calls\tnull_events\tfire_cpu_seconds"
           << "\tcandidate_checks\tmembership_updates\tcandidate_checks_per_fire"
           << "\tmembership_updates_per_fire\n";
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
        output << "reaction\t" << stats.rxnId << "\t" << cleanField(stats.name)
               << "\t" << stats.fireCalls << "\t" << stats.nullEvents
               << "\t" << stats.fireCpuSeconds << "\t" << stats.candidateChecks
               << "\t" << stats.membershipUpdates << "\t" << candidatesPerFire
               << "\t" << membershipsPerFire << "\n";
    }
}

}
