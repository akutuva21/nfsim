#!/usr/bin/env python3
"""Generate controlled NFsim EnergyPattern promoter workloads.

The generated XML deliberately keeps the reaction topology fixed while
varying promoter width and the number of initially occupied sites.  ``pairwise``
patterns exercise event-time matching and membership work without making the
input compiler exponential.  ``global`` is a stress case for the legacy
context-expansion representation: one energy pattern observes all promoter
sites, so each binding rule has one context condition per other site.

The file is a generator rather than a checked-in collection of large XML
fixtures, which keeps benchmark inputs auditable and makes occupancy changes
explicit in the command line.
"""

from __future__ import print_function

import argparse
import os
import xml.etree.ElementTree as ET


def _component(parent, identifier, name, bonds=0):
    return ET.SubElement(
        parent,
        "Component",
        {"id": identifier, "name": name, "numberOfBonds": str(bonds)},
    )


def _molecule(parent, identifier, name, components):
    molecule = ET.SubElement(parent, "Molecule", {"id": identifier, "name": name})
    component_list = ET.SubElement(molecule, "ListOfComponents")
    for component in components:
        _component(component_list, *component)
    return molecule


def _pattern(parent, identifier, molecules, bonds=()):
    child_tag = {
        "ListOfReactantPatterns": "ReactantPattern",
        "ListOfProductPatterns": "ProductPattern",
        "EnergyPattern": "Pattern",
    }.get(parent.tag)
    if child_tag is None:
        raise ValueError("cannot add a pattern below %s" % parent.tag)
    pattern = ET.SubElement(parent, child_tag, {"id": identifier})
    molecule_list = ET.SubElement(pattern, "ListOfMolecules")
    for molecule in molecules:
        _molecule(molecule_list, *molecule)
    if bonds:
        bond_list = ET.SubElement(pattern, "ListOfBonds")
        for bond_id, site1, site2 in bonds:
            ET.SubElement(
                bond_list,
                "Bond",
                {"id": bond_id, "site1": site1, "site2": site2},
            )
    return pattern


def _reaction_rule(model, site_index, forward):
    prefix = "R%04d" % site_index
    if not forward:
        prefix += "_reverse"
    site_name = "s%d" % site_index
    rule = ET.SubElement(
        model.find("ListOfReactionRules"),
        "ReactionRule",
        {"id": prefix, "name": prefix, "symmetry_factor": "1"},
    )
    reactants = ET.SubElement(rule, "ListOfReactantPatterns")
    products = ET.SubElement(rule, "ListOfProductPatterns")

    if forward:
        p1 = "_%s_RP1" % prefix
        p2 = "_%s_RP2" % prefix
        q1 = "_%s_PP1" % prefix
        q2 = "_%s_PP2" % prefix
        _pattern(
            reactants,
            p1,
            [(p1 + "_M1", "P", [(p1 + "_M1_C1", site_name, 0)])],
        )
        _pattern(
            reactants,
            p2,
            [(p2 + "_M1", "L", [(p2 + "_M1_C1", "p", 0)])],
        )
        p_mol = q1 + "_M1"
        l_mol = q1 + "_M2"
        p_comp = p_mol + "_C1"
        l_comp = l_mol + "_C1"
        _pattern(
            products,
            q1,
            [
                (p_mol, "P", [(p_comp, site_name, 1)]),
                (l_mol, "L", [(l_comp, "p", 1)]),
            ],
            [(q1 + "_B1", p_comp, l_comp)],
        )
        ET.SubElement(
            rule,
            "Map",
        )
        mapping = rule.find("Map")
        ET.SubElement(mapping, "MapItem", {"sourceID": p1 + "_M1", "targetID": p_mol})
        ET.SubElement(mapping, "MapItem", {"sourceID": p1 + "_M1_C1", "targetID": p_comp})
        ET.SubElement(mapping, "MapItem", {"sourceID": p2 + "_M1", "targetID": l_mol})
        ET.SubElement(mapping, "MapItem", {"sourceID": p2 + "_M1_C1", "targetID": l_comp})
        operations = ET.SubElement(rule, "ListOfOperations")
        ET.SubElement(operations, "AddBond", {"site1": p1 + "_M1_C1", "site2": p2 + "_M1_C1"})
    else:
        p1 = "_%s_RP1" % prefix
        q1 = "_%s_PP1" % prefix
        q2 = "_%s_PP2" % prefix
        p_mol = p1 + "_M1"
        l_mol = p1 + "_M2"
        p_comp = p_mol + "_C1"
        l_comp = l_mol + "_C1"
        _pattern(
            reactants,
            p1,
            [
                (p_mol, "P", [(p_comp, site_name, 1)]),
                (l_mol, "L", [(l_comp, "p", 1)]),
            ],
            [(p1 + "_B1", p_comp, l_comp)],
        )
        p_product = q1 + "_M1"
        l_product = q2 + "_M1"
        p_product_comp = q1 + "_M1_C1"
        l_product_comp = q2 + "_M1_C1"
        _pattern(
            products,
            q1,
            [(p_product, "P", [(p_product_comp, site_name, 0)])],
        )
        _pattern(
            products,
            q2,
            [(l_product, "L", [(l_product_comp, "p", 0)])],
        )
        mapping = ET.SubElement(rule, "Map")
        ET.SubElement(mapping, "MapItem", {"sourceID": p_mol, "targetID": p_product})
        ET.SubElement(mapping, "MapItem", {"sourceID": p_comp, "targetID": p_product_comp})
        ET.SubElement(mapping, "MapItem", {"sourceID": l_mol, "targetID": l_product})
        ET.SubElement(mapping, "MapItem", {"sourceID": l_comp, "targetID": l_product_comp})
        operations = ET.SubElement(rule, "ListOfOperations")
        ET.SubElement(operations, "DeleteBond", {"site1": p_comp, "site2": l_comp})

    rate_law = ET.SubElement(
        rule,
        "RateLaw",
        {"id": prefix + "_RateLaw", "type": "Arrhenius", "totalrate": "0"},
    )
    constants = ET.SubElement(rate_law, "ListOfRateConstants")
    ET.SubElement(constants, "RateConstant", {"value": "phi"})
    ET.SubElement(constants, "RateConstant", {"value": "Ea0"})


def _seed_species(model, sites, occupied, ligand_count):
    species_list = model.find("ListOfSpecies")
    species_id = "S_P_%d" % occupied
    p_id = species_id + "_M1"
    p_components = []
    for index in range(sites):
        p_components.append((p_id + "_C%d" % index, "s%d" % index, int(index < occupied)))
    molecules = [(p_id, "P", p_components)]
    bonds = []
    for index in range(occupied):
        ligand_id = species_id + "_M%d" % (index + 2)
        ligand_comp = ligand_id + "_C0"
        molecules.append((ligand_id, "L", [(ligand_comp, "p", 1)]))
        bonds.append(
            (
                species_id + "_B%d" % index,
                p_id + "_C%d" % index,
                ligand_comp,
            )
        )
    species = ET.SubElement(
        species_list,
        "Species",
        {"id": species_id, "concentration": "1", "name": "P"},
    )
    molecule_list = ET.SubElement(species, "ListOfMolecules")
    for molecule in molecules:
        _molecule(molecule_list, *molecule)
    if bonds:
        bond_list = ET.SubElement(species, "ListOfBonds")
        for bond_id, site1, site2 in bonds:
            ET.SubElement(bond_list, "Bond", {"id": bond_id, "site1": site1, "site2": site2})

    free_ligands = ligand_count - occupied
    if free_ligands:
        species = ET.SubElement(
            species_list,
            "Species",
            {"id": "S_L_%d" % occupied, "concentration": str(free_ligands), "name": "L(p)"},
        )
        molecule_list = ET.SubElement(species, "ListOfMolecules")
        _molecule(molecule_list, "S_L_M1", "L", [("S_L_M1_C1", "p", 0)])


def _add_energy_patterns(model, sites, mode):
    patterns = ET.SubElement(model, "ListOfEnergyPatterns")
    if mode == "global":
        pattern_id = "EP_GLOBAL"
        molecules = []
        bonds = []
        p_mol = pattern_id + "_P1_M1"
        p_components = []
        for index in range(sites):
            p_components.append((p_mol + "_C%d" % index, "s%d" % index, 1))
        molecules.append((p_mol, "P", p_components))
        for index in range(sites):
            ligand_mol = pattern_id + "_P1_M%d" % (index + 2)
            ligand_comp = ligand_mol + "_C1"
            molecules.append((ligand_mol, "L", [(ligand_comp, "p", 1)]))
            bonds.append((pattern_id + "_B%d" % index, p_mol + "_C%d" % index, ligand_comp))
        energy_pattern = ET.SubElement(
            patterns,
            "EnergyPattern",
            {"id": pattern_id, "pattern": "global", "expression": "g"},
        )
        _pattern(energy_pattern, pattern_id + "_P1", molecules, bonds)
        return

    for index in range(sites):
        other = (index + 1) % sites
        pattern_id = "EP_%04d" % index
        p_mol = pattern_id + "_P1_M1"
        l1_mol = pattern_id + "_P1_M2"
        l2_mol = pattern_id + "_P1_M3"
        p_site = p_mol + "_C1"
        p_other = p_mol + "_C2"
        l1_site = l1_mol + "_C1"
        l2_site = l2_mol + "_C1"
        energy_pattern = ET.SubElement(
            patterns,
            "EnergyPattern",
            {"id": pattern_id, "pattern": "pairwise", "expression": "g"},
        )
        _pattern(
            energy_pattern,
            pattern_id + "_P1",
            [
                (p_mol, "P", [(p_site, "s%d" % index, 1), (p_other, "s%d" % other, 1)]),
                (l1_mol, "L", [(l1_site, "p", 1)]),
                (l2_mol, "L", [(l2_site, "p", 1)]),
            ],
            [
                (pattern_id + "_B1", p_site, l1_site),
                (pattern_id + "_B2", p_other, l2_site),
            ],
        )


def make_model(sites, occupied, mode, ligand_count=0, ea0="2.0", energy="0.05"):
    if sites <= 0:
        raise ValueError("sites must be positive")
    if occupied < 0 or occupied > sites:
        raise ValueError("occupied must be between zero and sites")
    if ligand_count == 0:
        ligand_count = sites
    if ligand_count < occupied:
        raise ValueError("ligand_count must be at least occupied")
    if mode not in ("pairwise", "global"):
        raise ValueError("mode must be pairwise or global")

    model = ET.Element("model", {"id": "energy_promoter_%s_%d_%d" % (mode, sites, occupied)})
    parameters = ET.SubElement(model, "ListOfParameters")
    for identifier, value in (("phi", "0.5"), ("RT", "1.0"), ("Ea0", str(ea0)), ("g", str(energy))):
        ET.SubElement(parameters, "Parameter", {"id": identifier, "type": "Constant", "value": value, "expr": value})

    molecule_types = ET.SubElement(model, "ListOfMoleculeTypes")
    p_type = ET.SubElement(molecule_types, "MoleculeType", {"id": "P"})
    p_components = ET.SubElement(p_type, "ListOfComponentTypes")
    for index in range(sites):
        ET.SubElement(p_components, "ComponentType", {"id": "s%d" % index})
    l_type = ET.SubElement(molecule_types, "MoleculeType", {"id": "L"})
    l_components = ET.SubElement(l_type, "ListOfComponentTypes")
    ET.SubElement(l_components, "ComponentType", {"id": "p"})
    ET.SubElement(model, "ListOfCompartments")
    ET.SubElement(model, "ListOfSpecies")
    ET.SubElement(model, "ListOfReactionRules")
    ET.SubElement(model, "ListOfObservables")
    ET.SubElement(model, "ListOfFunctions")

    _seed_species(model, sites, occupied, ligand_count)
    _add_energy_patterns(model, sites, mode)
    for index in range(sites):
        _reaction_rule(model, index, True)
        _reaction_rule(model, index, False)

    return ET.Element("sbml", {"level": "3", "version": "1"}), model


def write_model(path, sites, occupied, mode, ligand_count=0, ea0="2.0", energy="0.05"):
    root, model = make_model(sites, occupied, mode, ligand_count, ea0, energy)
    root.append(model)
    tree = ET.ElementTree(root)
    try:
        ET.indent(tree, space="  ")
    except AttributeError:
        pass
    tree.write(path, encoding="utf-8", xml_declaration=True)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", help="destination XML path")
    parser.add_argument("--sites", type=int, required=True)
    parser.add_argument("--occupied", type=int, default=0)
    parser.add_argument("--mode", choices=("pairwise", "global"), default="pairwise")
    parser.add_argument("--ligands", type=int, default=0, help="total ligand molecules, including initially bound ones")
    parser.add_argument("--ea0", default="2.0", help="Arrhenius activation-energy parameter")
    parser.add_argument("--energy", default="0.05", help="energy assigned to each pattern")
    arguments = parser.parse_args(argv)
    output = os.path.abspath(arguments.output)
    parent = os.path.dirname(output)
    if parent and not os.path.isdir(parent):
        os.makedirs(parent, exist_ok=True)
    write_model(
        output,
        arguments.sites,
        arguments.occupied,
        arguments.mode,
        arguments.ligands,
        arguments.ea0,
        arguments.energy,
    )
    print(output)


if __name__ == "__main__":
    main()
