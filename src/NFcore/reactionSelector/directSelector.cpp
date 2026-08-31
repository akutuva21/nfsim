/*
 * DirectSelector.cpp
 *
 *  Created on: Jul 23, 2009
 *      Author: msneddon
 */



#include "reactionSelector.hh"

#if defined(_MSC_VER)
#include <intrin.h>
#endif

using namespace std;
using namespace NFcore;

namespace {

unsigned int directSelectorTrailingZeroCount(std::uint64_t value)
{
#if defined(_MSC_VER)
	unsigned long bit = 0;
	_BitScanForward64(&bit, value);
	return static_cast<unsigned int>(bit);
#else
	return static_cast<unsigned int>(__builtin_ctzll(value));
#endif
}

}


double ReactionSelector::updateBatch(vector<ReactionClass *> &rxns)
{
	for (vector<ReactionClass *>::const_iterator it = rxns.begin();
			it != rxns.end(); ++it) {
		ReactionClass *r = *it;
		double oldA = r->get_a();
		double newA = r->update_a();
		update(r, oldA, newA);
	}
	return getAtot();
}




DirectSelector::DirectSelector(vector <ReactionClass *> &rxns, System *sys) :
	ReactionSelector(sys)
{
	this->Atot = 0;
	this->n_reactions = rxns.size();
	this->reactionClassList = new ReactionClass *[n_reactions];
	this->selectionBlockSize = 8;
	this->selectionBlockPropensities.assign(
			(static_cast<std::size_t>(n_reactions) + selectionBlockSize - 1) /
					selectionBlockSize, 0.0);
	this->sparseSelectionSafe = n_reactions > 0;
	for(int r=0; r<n_reactions; r++) {
		reactionClassList[r] = rxns.at(r);
		double propensity = reactionClassList[r]->get_a();
		Atot += propensity;
		selectionBlockPropensities[static_cast<std::size_t>(r) /
				selectionBlockSize] += propensity;
		if (!reactionClassList[r]->supportsSparseSelection())
			sparseSelectionSafe = false;
	}
	if (sparseSelectionSafe) {
		activeReactionBits.assign(
				(static_cast<std::size_t>(n_reactions) + 63) / 64, 0);
		for (int r=0; r<n_reactions; r++) {
			if (reactionClassList[r]->get_a() != 0.0)
				activeReactionBits[static_cast<std::size_t>(r) >> 6] |=
					(std::uint64_t(1) << (r & 63));
		}
	}
}



DirectSelector::~DirectSelector()
{
	Atot = 0;
	n_reactions = 0;
	delete [] reactionClassList;
	sparseSelectionSafe = false;
	activeReactionBits.clear();
}

double DirectSelector::refactorPropensities()
{
	Atot = 0;
	std::fill(selectionBlockPropensities.begin(),
			selectionBlockPropensities.end(), 0.0);
	if (sparseSelectionSafe)
		std::fill(activeReactionBits.begin(), activeReactionBits.end(), 0);
	for(int r=0; r<n_reactions; r++) {
		double propensity = reactionClassList[r]->update_a();
		Atot += propensity;
		selectionBlockPropensities[static_cast<std::size_t>(r) /
				selectionBlockSize] += propensity;
		if (sparseSelectionSafe && propensity != 0.0)
			activeReactionBits[static_cast<std::size_t>(r) >> 6] |=
				(std::uint64_t(1) << (r & 63));
	}
	return Atot;
}


double DirectSelector::update(ReactionClass *r,double oldA, double newA)
{
	Atot-=oldA;
	Atot+=newA;
	int reaction = r->getRxnId();
	if (reaction < 0 || reaction >= n_reactions ||
			reactionClassList[reaction] != r) {
		/* System::prepareForSimulation() assigns the global reaction id
		 * before any runtime update.  Retain a cold fallback for callers
		 * that construct a selector directly in tests. */
		for (reaction = 0; reaction < n_reactions; ++reaction) {
			if (reactionClassList[reaction] == r) break;
		}
	}
	if (reaction >= 0 && reaction < n_reactions) {
		std::size_t block = static_cast<std::size_t>(reaction) /
				selectionBlockSize;
		selectionBlockPropensities[block] -= oldA;
		selectionBlockPropensities[block] += newA;
	}
	if (sparseSelectionSafe) {
		if (reaction < n_reactions) {
			std::uint64_t &word =
				activeReactionBits[static_cast<std::size_t>(reaction) >> 6];
			std::uint64_t bit = std::uint64_t(1) << (reaction & 63);
			if (newA != 0.0)
				word |= bit;
			else
				word &= ~bit;
		}
	}
	return Atot;
}

double DirectSelector::updateBatch(vector<ReactionClass *> &rxns)
{
	for (vector<ReactionClass *>::const_iterator it = rxns.begin();
			it != rxns.end(); ++it) {
		ReactionClass *r = *it;
		double oldA = r->get_a();
		double newA = r->update_a();
		Atot -= oldA;
		Atot += newA;

		int reaction = r->getRxnId();
		if (reaction < 0 || reaction >= n_reactions ||
				reactionClassList[reaction] != r) {
			/* System::prepareForSimulation() assigns the global reaction id
			 * before any runtime update.  Retain a cold fallback for callers
			 * that construct a selector directly in tests. */
			for (reaction = 0; reaction < n_reactions; ++reaction) {
				if (reactionClassList[reaction] == r) break;
			}
		}
		if (reaction < 0 || reaction >= n_reactions) continue;
		std::size_t block = static_cast<std::size_t>(reaction) /
				selectionBlockSize;
		selectionBlockPropensities[block] -= oldA;
		selectionBlockPropensities[block] += newA;
		if (sparseSelectionSafe) {
			std::uint64_t &word =
					activeReactionBits[static_cast<std::size_t>(reaction) >> 6];
			std::uint64_t bit =
					std::uint64_t(1) << (reaction & 63);
			if (newA != 0.0)
				word |= bit;
			else
				word &= ~bit;
		}
	}
	return Atot;
}



double DirectSelector::getNextReactionClass(ReactionClass *&rc)
{
	double randNum = sys_->getRNG().random(Atot);

	double a_sum=0, last_a_sum=0;
	/* Compact EnergyPattern reactions are safe for an order-preserving sparse
	 * scan.  Use bounded prefix blocks for that path; retain the legacy dense
	 * scan for reaction classes whose zero-rate membership cannot be indexed. */
	if (sparseSelectionSafe && selectionBlockPropensities.size() > 1) {
		for (std::size_t block = 0;
				block < selectionBlockPropensities.size(); ++block) {
			double blockPropensity = selectionBlockPropensities[block];
			if (randNum <= a_sum + blockPropensity) {
				std::size_t first = block * selectionBlockSize;
				std::size_t last = std::min<std::size_t>(
						first + selectionBlockSize,
						static_cast<std::size_t>(n_reactions));
				for (std::size_t r = first; r < last; ++r) {
					if (sparseSelectionSafe &&
							(activeReactionBits[r >> 6] &
									(std::uint64_t(1) << (r & 63))) == 0)
						continue;
					a_sum += reactionClassList[r]->get_a();
					if (randNum <= a_sum) {
						rc = reactionClassList[r];
						return (randNum-last_a_sum);
					}
					last_a_sum = a_sum;
				}
				/* A block sum can differ from the scalar prefix by one ulp
				 * after many incremental updates.  Continue from the exact
				 * scanned prefix if that rare boundary case occurs. */
				last_a_sum = a_sum;
				continue;
			}
			a_sum += blockPropensity;
			last_a_sum = a_sum;
		}
		this->refactorPropensities();
		return getNextReactionClass(rc);
	}

	//WARNING - DO NOT USE THE DEFAULT C++ RANDOM NUMBER GENERATOR FOR THIS STEP
	// - IT INTRODUCES SMALL NUMERICAL ERRORS CAUSING THE ORDER OF RXNS TO
	//   AFFECT SIMULATION RESULTS
	if (sparseSelectionSafe) {
		for(std::size_t wordIndex=0; wordIndex<activeReactionBits.size();
				++wordIndex) {
			std::uint64_t active = activeReactionBits[wordIndex];
			while (active != 0) {
				unsigned int bit = directSelectorTrailingZeroCount(active);
				unsigned int r = static_cast<unsigned int>((wordIndex << 6) + bit);
				active &= active - 1;
				a_sum += reactionClassList[r]->get_a();
				if(randNum <= a_sum)
				{
					rc = reactionClassList[r];
					return (randNum-last_a_sum);
				}
				last_a_sum = a_sum;
			}
		}
	} else {
		for(int r=0; r<n_reactions; r++) {
			a_sum += reactionClassList[r]->get_a();
			if(randNum <= a_sum)
			{
				rc = reactionClassList[r];
				return (randNum-last_a_sum);
			}
			last_a_sum = a_sum;
		}
	}

	this->refactorPropensities();
	return getNextReactionClass(rc);

}


double DirectSelector::getAtot()
{
	return Atot;
}
