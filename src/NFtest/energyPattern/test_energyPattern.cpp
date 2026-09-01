#include "test_energyPattern.hh"
#include "../../NFcore/NFcore.hh"
#include "../../NFcore/energyPattern.hh"
#include <iostream>
#include <stdexcept>
#include <vector>
#include <string>
#include <cmath>

using namespace std;
using namespace NFcore;

void NFtest_energyPattern::run()
{
	cout << "Running EnergyFunction tests..." << endl;

    EnergyFunction ef(0.5, 1.0); // phi = 0.5, RT = 1.0

	cout << "  Testing EnergyFunction::getPhi and getRT..." << endl;
    if (ef.getPhi() != 0.5) throw runtime_error("EnergyFunction::getPhi returned wrong value.");
    if (ef.getRT() != 1.0) throw runtime_error("EnergyFunction::getRT returned wrong value.");

    cout << "  Testing EnergyFunction::getNumPatterns..." << endl;
    if (ef.getNumPatterns() != 0) throw runtime_error("EnergyFunction::getNumPatterns initially not 0.");

    cout << "  Testing EnergyFunction::addEnergyPattern..." << endl;

    EnergyPatternInfo ep;
    ep.id = "EP1";
    ep.energyValue = -5.0;

    EpMolecule m1;
    m1.typeName = "A";
    m1.xmlId = "m1";

    EpMolecule::CompInfo c1;
    c1.name = "b";
    c1.bondPartnerId = "m2";
    c1.isBound = true;
    c1.stateConstraint = "";
    m1.components.push_back(c1);
    ep.molecules.push_back(m1);

    EpMolecule m2;
    m2.typeName = "B";
    m2.xmlId = "m2";
    EpMolecule::CompInfo c2;
    c2.name = "a";
    c2.bondPartnerId = "m1";
    c2.isBound = true;
    c2.stateConstraint = "";
    m2.components.push_back(c2);
    ep.molecules.push_back(m2);

    EnergyPatternInfo::Bond bond;
    bond.mol1 = 0;
    bond.comp1 = 0;
    bond.mol2 = 1;
    bond.comp2 = 0;
    ep.bonds.push_back(bond);

    ef.addEnergyPattern(ep);

    if (ef.getNumPatterns() != 1) throw runtime_error("EnergyFunction::getNumPatterns not 1 after addition.");
    if (ef.getPattern(0).id != "EP1") throw runtime_error("EnergyFunction::getPattern returned wrong pattern.");

    // Testing findRelevantPatternsForBinding (indirectly via expandBindingRule)
    cout << "  Testing EnergyFunction::expandBindingRule..." << endl;

    // For A(b) + B(a) -> A(b!1).B(a!1), EP1 should be relevant as an "always" pattern
    vector<ExpandedRuleInfo> rules = ef.expandBindingRule("Rxn1", 10.0, 0.5, "A", "b", "B", "a");

    // We expect 1 forward and 1 reverse rule
    if (rules.size() != 2) throw runtime_error("expandBindingRule did not return 2 rules.");

    ExpandedRuleInfo fwd = rules[0];
    ExpandedRuleInfo rev = rules[1];

    // EP1 is always matching the product, baseG = -5.0
    // deltaG = -5.0.
    // Forward rate: exp(-(10.0 + 0.5 * -5.0)/1.0) = exp(-7.5)
    // Reverse rate: exp(-(10.0 + -0.5 * -5.0)/1.0) = exp(-12.5)

    if (fwd.name != "Rxn1_fwd") throw runtime_error("Forward rule name mismatch.");
    if (!fwd.isForward) throw runtime_error("Forward rule isForward flag false.");
    if (abs(fwd.deltaG - (-5.0)) > 1e-6) throw runtime_error("Forward rule deltaG mismatch.");
    if (abs(fwd.rate - exp(-7.5)) > 1e-6) throw runtime_error("Forward rule rate mismatch.");

    if (rev.name != "Rxn1_rev") throw runtime_error("Reverse rule name mismatch.");
    if (rev.isForward) throw runtime_error("Reverse rule isForward flag true.");
    if (abs(rev.deltaG - (-5.0)) > 1e-6) throw runtime_error("Reverse rule deltaG mismatch.");
    if (abs(rev.rate - exp(-12.5)) > 1e-6) throw runtime_error("Reverse rule rate mismatch.");

    cout << "  Testing compact conjunction context extraction..." << endl;

    EnergyPatternInfo epConjunction;
    epConjunction.id = "EP_CONJUNCTION";
    epConjunction.energyValue = 2.0;

    EpMolecule promoter;
    promoter.typeName = "P";
    promoter.xmlId = "promoter";
    for (int i = 0; i < 3; ++i) {
        EpMolecule::CompInfo site;
        site.name = string("s") + char('0' + i);
        site.bondPartnerId = string("ligand") + char('0' + i);
        site.isBound = true;
        site.stateConstraint = "";
        promoter.components.push_back(site);
    }
    epConjunction.molecules.push_back(promoter);

    EpMolecule ligand0;
    ligand0.typeName = "L";
    ligand0.xmlId = "ligand0";
    EpMolecule::CompInfo ligand0Site;
    ligand0Site.name = "p";
    ligand0Site.bondPartnerId = "promoter";
    ligand0Site.isBound = true;
    ligand0Site.stateConstraint = "";
    ligand0.components.push_back(ligand0Site);
    epConjunction.molecules.push_back(ligand0);

    EnergyPatternInfo::Bond centerBond;
    centerBond.mol1 = 0;
    centerBond.comp1 = 0;
    centerBond.mol2 = 1;
    centerBond.comp2 = 0;
    epConjunction.bonds.push_back(centerBond);

    for (int i = 1; i < 3; ++i) {
        EpMolecule contextLigand;
        contextLigand.typeName = "L";
        contextLigand.xmlId = string("ligand") + char('0' + i);
        EpMolecule::CompInfo contextSite;
        contextSite.name = "p";
        contextSite.bondPartnerId = "promoter";
        contextSite.isBound = true;
        contextSite.stateConstraint = "";
        contextLigand.components.push_back(contextSite);
        epConjunction.molecules.push_back(contextLigand);

        EnergyPatternInfo::Bond contextBond;
        contextBond.mol1 = 0;
        contextBond.comp1 = i;
        contextBond.mol2 = i + 1;
        contextBond.comp2 = 0;
        epConjunction.bonds.push_back(contextBond);
    }

    ef.addEnergyPattern(epConjunction);

    EnergyBindingContext conjunctionContext;
    if (!ef.getBindingContext("P", "s0", "L", "p", conjunctionContext))
        throw runtime_error("getBindingContext rejected a compact conjunction.");
    if (conjunctionContext.conditions.size() != 2)
        throw runtime_error("compact conjunction did not retain both conditions.");
    if (conjunctionContext.conditionalTerms.size() != 1 ||
        conjunctionContext.conditionalTerms[0].conditionMask != 3u)
        throw runtime_error("compact conjunction mask was not encoded correctly.");


    // Test state change rule
    cout << "  Testing EnergyFunction::expandStateChangeRule..." << endl;

    EnergyPatternInfo epState;
    epState.id = "EP2";
    epState.energyValue = -3.0;

    EpMolecule ms;
    ms.typeName = "C";
    ms.xmlId = "ms1";

    EpMolecule::CompInfo cs;
    cs.name = "p";
    cs.bondPartnerId = "";
    cs.isBound = false;
    cs.stateConstraint = "phos";
    ms.components.push_back(cs);
    epState.molecules.push_back(ms);

    ef.addEnergyPattern(epState);

    // For C(p~unphos) -> C(p~phos), EP2 matches "to" state, baseG = -3.0
    // deltaG = -3.0
    // Forward rate: exp(-(10.0 + 0.5 * -3.0)/1.0) = exp(-8.5)
    // Reverse rate: exp(-(10.0 + -0.5 * -3.0)/1.0) = exp(-11.5)

    vector<ExpandedRuleInfo> rulesState = ef.expandStateChangeRule("RxnState", 10.0, 0.5, "C", "p", "unphos", "phos");

    if (rulesState.size() != 2) throw runtime_error("expandStateChangeRule did not return 2 rules.");

    ExpandedRuleInfo fwdS = rulesState[0];
    ExpandedRuleInfo revS = rulesState[1];

    if (fwdS.name != "RxnState_fwd") throw runtime_error("State forward rule name mismatch.");
    if (abs(fwdS.deltaG - (-3.0)) > 1e-6) throw runtime_error("State forward rule deltaG mismatch.");
    if (abs(fwdS.rate - exp(-8.5)) > 1e-6) throw runtime_error("State forward rule rate mismatch.");

    cout << "  Testing compact partner pool swap removal..." << endl;
    System *poolSystem = new System("CompactPartnerPoolTest");
    vector<string> poolComponents;
    poolComponents.push_back("site");
    MoleculeType *poolMoleculeType =
        new MoleculeType("PoolMolecule", poolComponents, poolSystem);
    Molecule *poolMolecule0 = poolMoleculeType->genDefaultMolecule();
    Molecule *poolMolecule1 = poolMoleculeType->genDefaultMolecule();
    Molecule *poolMolecule2 = poolMoleculeType->genDefaultMolecule();
    CompactPartnerPool pool;
    pool.add(poolMolecule0,
             static_cast<unsigned int>(poolMolecule0->getMolListId()));
    pool.add(poolMolecule1,
             static_cast<unsigned int>(poolMolecule1->getMolListId()));
    pool.add(poolMolecule2,
             static_cast<unsigned int>(poolMolecule2->getMolListId()));
    if (!pool.remove(poolMolecule0,
                     static_cast<unsigned int>(poolMolecule0->getMolListId())))
        throw runtime_error("compact partner pool did not remove its first entry.");
    if (!pool.contains(poolMolecule1,
                       static_cast<unsigned int>(poolMolecule1->getMolListId())) ||
        !pool.contains(poolMolecule2,
                       static_cast<unsigned int>(poolMolecule2->getMolListId())) ||
        pool.getByIndex(0) != poolMolecule2)
        throw runtime_error("compact partner pool reverse index was not updated after swap removal.");
    delete poolSystem;

	cout << "EnergyFunction tests completed successfully." << endl;
}
