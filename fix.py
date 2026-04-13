with open("src/NFcore/templateMolecule.cpp", "r") as f:
    content = f.read()

import re
old_func_regex = r"bool TemplateMolecule::isMoleculeTypeAndComponentPresent\(MoleculeType \* mt, int cIndex\) \{.*?\n\}"
new_func = """bool TemplateMolecule::isMoleculeTypeAndComponentPresent(MoleculeType * mt, int cIndex) {
	if (this->getMoleculeType() != mt) return false;

	// Check if this component is uniquely mapped via any empty/occupied/bond/state constraints
	if (this->compIsAlwaysMapped[cIndex]) return true;

	// Check if this component is required by a symmetric component constraint
	for(int i=0; i<n_symComps; i++) {
		int *molEqComp;
		int n_molEqComp = 0;
		mt->getEquivalencyClass(molEqComp, n_molEqComp, this->symCompName[i]);
		for(int j=0; j<n_molEqComp; j++) {
			if (molEqComp[j] == cIndex) return true;
		}
	}

	return false;
}"""

content = re.sub(old_func_regex, new_func, content, flags=re.DOTALL)

with open("src/NFcore/templateMolecule.cpp", "w") as f:
    f.write(content)
