import re
with open('src/NFcore/templateMolecule.cpp', 'r') as f:
    data = f.read()

func = re.search(r'bool TemplateMolecule::isMoleculeTypeAndComponentPresent.*?return false;\n}', data, re.DOTALL)
if func:
    print(func.group(0))
