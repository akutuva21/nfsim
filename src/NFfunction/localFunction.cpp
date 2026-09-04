/*
 * localFunction.cpp
 *
 *  Created on: Dec 4, 2008
 *      Author: msneddon
 */


#include "NFfunction.hh"






using namespace std;
using namespace NFcore;
#ifndef NFSIM_USE_EXPRTK
using namespace mu;
#endif



list <Molecule *> LocalFunction::molList;
list <Molecule *>::iterator LocalFunction::molIter;


string LocalFunction::getName() const {
	return this->name;
}
string LocalFunction::getNiceName() const {
	return nicename;
}
string LocalFunction::getExpression() const {
	return originalExpression;
}
string LocalFunction::getParsedExpression() const {
	return parsedExpression;
}



LocalFunction::LocalFunction(System *s,
					string name,
					string originalExpression,
					string parsedExpression,
					vector <string> &args,
					vector <string> &varRefNames,
					vector <string> &varObservableNames,
					vector <Observable *> & varObservables,
					vector <int> &varRefScope,
					vector <string> paramNames)
{
//	cout<<"Attempting to create local function: "<<name<<endl;

	//Some checks to make sure we are doing things ok
	if(args.size()>1) {
		cerr<<"For efficiency, local functions currently support a maximum of 1 argument."<<endl;
		cerr<<"Quitting now."<<endl;
		exit(1);
	}
	if(args.size()<1) {
		cerr<<"When creating a local Function, ERROR!! there were no args, so it is a global function."<<endl;
		cerr<<"Quitting now."<<endl;
		exit(1);
	}

	//Do the basics first...
	this->name = name;
	this->originalExpression=originalExpression;
	this->parsedExpression=parsedExpression;
	// default to false
	this->isEverEvaluatedOnSpeciesScope=false;
	this->hasSimpleStateDependency=false;
	this->directStateLookup=false;
	this->simpleStateMoleculeType=0;
	this->simpleStateComponent=-1;
	this->simpleStateCacheEnabled=false;
	this->simpleStateCacheSize=0;
	this->simpleStateCacheValues=0;
	this->simpleStateCacheValid=0;
	this->n_typeImolecules=0;
	this->typeI_mol=new MoleculeType *[s->getNumOfMoleculeTypes()];
	this->typeI_localFunctionIndex=new int[s->getNumOfMoleculeTypes()];
	// remember the system
	this->system = s;

	//Move the vectors into our neat little arrays
	this->n_args=args.size();
	this->argNames = new string[n_args];
	for(unsigned int i=0; i<n_args; i++) {
		this->argNames[i]=args.at(i);
	}

	this->n_varRefs=varRefNames.size();
	this->varRefNames = new string[n_varRefs];
	this->varObservableNames = new string[n_varRefs];
	this->varLocalObservables = new Observable *[n_varRefs];
	this->varRefScope = new int[n_varRefs];
	this->simpleStateValues = n_varRefs > 0 ? new int[n_varRefs] : 0;
	for (unsigned int i=0; i<n_varRefs; ++i)
		this->simpleStateValues[i] = -1;
	for(unsigned int i=0; i<n_varRefs; i++) {
		this->varRefNames[i] = varRefNames.at(i);
		this->varObservableNames[i] = varObservableNames.at(i);
		this->varLocalObservables[i] = varObservables.at(i);
		this->varRefScope[i] = varRefScope.at(i);
	}

	this->n_params=paramNames.size();
	this->paramNames = new string[n_params];
	for(unsigned int i=0; i<n_params; i++) {
		this->paramNames[i] = paramNames.at(i);
	}

	//now assemble the nicename
	nicename = this->name + "(";
	for(unsigned int i=0;i<n_args; i++) {
		if(i==0) nicename+=argNames[i];
		else nicename+=","+argNames[i];
	}
	nicename+=")";

	p=0;


	//Identify the type II molecules - those molecules that when changed
	//cause this function to change
	vector <TemplateMolecule *> tmList;
	vector <MoleculeType *> addedMoleculeTypes;

//	cout<<"Now remembering type II molecules..."<<endl;
	bool hasAdded = false;
	for(unsigned int i=0; i<n_varRefs; i++) {

		//Go through each template molecule from each observable, and remember the moleculetypes
		//that we will need to keep track of
		TemplateMolecule **tmHeads=0; int n_tmHeads=0;
		if(varRefScope[i]==-1) {//handle global scopes
			s->getObservableByName(this->varObservableNames[i])->getTemplateMoleculeList(n_tmHeads,tmHeads);
			for(int k=0; k<n_tmHeads; k++) {
				TemplateMolecule::traverse(tmHeads[k],tmList,TemplateMolecule::FIND_ALL);
			}
		} else { //handle local scopes
			this->varLocalObservables[i]->getTemplateMoleculeList(n_tmHeads,tmHeads);
			for(int k=0; k<n_tmHeads; k++) {
				TemplateMolecule::traverse(tmHeads[k],tmList,TemplateMolecule::FIND_ALL);
			}
		}
		//cout<<"traversed obs "<<i<<" and found: "<<tmList.size()<<" templates\n";

		for(unsigned int t=0; t<tmList.size(); t++) {
			MoleculeType *mt = tmList.at(t)->getMoleculeType();

			//Make sure we haven't added this molecule type before
			hasAdded = false;
			for(unsigned int m=0; m<addedMoleculeTypes.size(); m++) {
				if(addedMoleculeTypes.at(m)==mt) {
					hasAdded=true; break;
				}
			}
			if(!hasAdded) {
				addedMoleculeTypes.push_back(mt);
			//	cout<<"remembering: "<<mt->getName()<<endl;
			}
			//else {
			//	cout<<"ignoring: "<<mt->getName()<<endl;
			//}
		}
		tmList.clear();
	}

	//Now actually remember them (and make sure they remember us...)
	n_typeIImolecules = addedMoleculeTypes.size();
	typeII_mol = new MoleculeType * [n_typeIImolecules];
	typeII_localFunctionIndex = new int[n_typeIImolecules];
	for(int m=0; m<n_typeIImolecules; m++) {
		int index = addedMoleculeTypes.at(m)->addLocalFunc_TypeII(this);
		typeII_mol[m]=addedMoleculeTypes.at(m);
		this->typeII_localFunctionIndex[m]=index;
	}

	/* Detect a narrowly defined state-only dependency.  It is safe to skip
	 * reevaluation when the argument molecule's relevant state is unchanged;
	 * any global, bond, stoichiometric, multi-molecule, or complex-scoped
	 * dependency deliberately falls back to the generic path. */
	if (n_varRefs > 0) {
		bool simple = true;
		MoleculeType *dependencyType = 0;
		int dependencyComponent = -1;
		for (unsigned int i=0; i<n_varRefs; ++i) {
			if (varRefScope[i] < 0 || varLocalObservables[i] == 0) {
				simple = false;
				break;
			}
			MoleculeType *candidateType = 0;
			int candidateComponent = -1;
			int candidateState = -1;
			if (!varLocalObservables[i]->getSimpleStatePredicate(
					candidateType, candidateComponent, candidateState)) {
				simple = false;
				break;
			}
			if (dependencyType == 0) {
				dependencyType = candidateType;
				dependencyComponent = candidateComponent;
			} else if (dependencyType != candidateType ||
					dependencyComponent != candidateComponent) {
				simple = false;
				break;
			}
			if (simpleStateValues != 0)
				simpleStateValues[i] = candidateState;
		}
		if (simple && dependencyType != 0) {
			hasSimpleStateDependency = true;
			simpleStateMoleculeType = dependencyType;
			simpleStateComponent = dependencyComponent;
			/* The value of a state-only function is molecule-independent
			 * once constants are fixed.  Do not use this cache for time()
			 * expressions; parameter updates invalidate it below. */
			simpleStateCacheEnabled = !containsTimeExpression(originalExpression);
			vector<vector<string> > possibleStates =
					dependencyType->getPossibleCompStates();
			if (dependencyComponent >= 0 &&
					dependencyComponent < (int)possibleStates.size())
				simpleStateCacheSize = possibleStates[dependencyComponent].size();
			for (unsigned int i = 0; i < n_varRefs; ++i)
				if (simpleStateValues[i] >= simpleStateCacheSize)
					simpleStateCacheSize = simpleStateValues[i] + 1;
			if (simpleStateCacheSize > 0) {
				simpleStateCacheValues = new double[simpleStateCacheSize];
				simpleStateCacheValid = new bool[simpleStateCacheSize];
				for (int i = 0; i < simpleStateCacheSize; ++i) {
					simpleStateCacheValues[i] = 0.0;
					simpleStateCacheValid[i] = false;
				}
			}
			dependencyType->addSimpleStateLocalFunc_TypeII(this,
					dependencyComponent);
		}
	}
}

bool LocalFunction::configureSimpleStateLookup(
		MoleculeType *moleculeType,
		int componentIndex,
		const vector<double> &stateValues)
{
	if (moleculeType == 0 || componentIndex < 0 ||
			componentIndex >= moleculeType->getNumOfComponents() ||
			stateValues.empty() || n_args != 1 || n_varRefs != 0 ||
			n_typeIImolecules != 0 || directStateLookup)
		return false;

	/* The parser-level lookup is still a normal local function from the
	 * simulator's point of view.  It has one Type-II dependency, one
	 * state-indexed invalidation slot, and can later acquire Type-I
	 * dependencies when a DOR rate law references it. */
	directStateLookup = true;
	hasSimpleStateDependency = true;
	simpleStateMoleculeType = moleculeType;
	simpleStateComponent = componentIndex;
	simpleStateCacheEnabled = true;
	simpleStateCacheSize = static_cast<int>(stateValues.size());
	simpleStateCacheValues = new double[simpleStateCacheSize];
	simpleStateCacheValid = new bool[simpleStateCacheSize];
	for (int state = 0; state < simpleStateCacheSize; ++state) {
		simpleStateCacheValues[state] = stateValues[state];
		simpleStateCacheValid[state] = true;
	}

	delete [] typeII_mol;
	delete [] typeII_localFunctionIndex;
	typeII_mol = new MoleculeType *[1];
	typeII_localFunctionIndex = new int[1];
	typeII_localFunctionIndex[0] = moleculeType->addLocalFunc_TypeII(this);
	typeII_mol[0] = moleculeType;
	n_typeIImolecules = 1;
	moleculeType->addSimpleStateLocalFunc_TypeII(this, componentIndex);
	return true;
}

double LocalFunction::evaluateSimpleStateValue(int stateValue)
{
	if (directStateLookup) {
		if (stateValue >= 0 && stateValue < simpleStateCacheSize)
			return simpleStateCacheValues[stateValue];
		return 0.0;
	}
	if (simpleStateCacheEnabled && stateValue >= 0 &&
			stateValue < simpleStateCacheSize &&
			simpleStateCacheValid[stateValue])
		return simpleStateCacheValues[stateValue];

	for (unsigned int i=0; i<n_varRefs; ++i) {
		/* The constructor only enables this path for molecule observables that
		 * are one state predicate on the same molecule type/component.  Update
		 * their parser-backed counters directly and avoid template matching. */
		varLocalObservables[i]->clear();
		if (stateValue == simpleStateValues[i])
			varLocalObservables[i]->straightAdd();
	}
	double value = FuncFactory::Eval(p);
	if (simpleStateCacheEnabled && stateValue >= 0 &&
			stateValue < simpleStateCacheSize) {
		simpleStateCacheValues[stateValue] = value;
		simpleStateCacheValid[stateValue] = true;
	}
	return value;
}

double LocalFunction::evaluateSimpleState(Molecule *m)
{
	return evaluateSimpleStateValue(m->getComponentState(simpleStateComponent));
}

double LocalFunction::getSimpleStateValueForState(int stateValue)
{
	return evaluateSimpleStateValue(stateValue);
}

void LocalFunction::cacheSimpleStateValue(Molecule *m)
{
	if (!directStateLookup || m == 0 ||
			m->getMoleculeType() != simpleStateMoleculeType)
		return;

	int stateValue = m->getComponentState(simpleStateComponent);
	double value = evaluateSimpleStateValue(stateValue);
	for (int ti = 0; ti < n_typeImolecules; ++ti) {
		if (typeI_mol[ti] == m->getMoleculeType()) {
			int localIndex = typeI_localFunctionIndex[ti];
			if (!m->isLocalFunctionStateCurrent(localIndex, stateValue)) {
				m->setLocalFunctionValue(value, localIndex);
				m->setLocalFunctionState(localIndex, stateValue);
			}
		}
	}
}

// set/get whether this evaluates on complex complex
bool LocalFunction::getEvaluateComplexScope() const {
	return isEverEvaluatedOnSpeciesScope;
};
void LocalFunction::setEvaluateComplexScope( bool val ) {
	isEverEvaluatedOnSpeciesScope=val;
	if ( !system->getEvaluateComplexScopedLocalFunctions() && val ) {
		// warn user that complex-scoped evaluations are disabled.
		cout<<"Warning! LocalFunction argument has complex scope, but complex-scoped local functions"<<endl;
		cout<<"  are disabled. Remove '-nocslf' argument to enable complex-scoped evaluation!"<<endl;
	}
};


void LocalFunction::prepareForSimulation(System *s) {
	if (directStateLookup)
		return;

	//Finally, we can create the local function
	try {
		const string expression = normalizeTimeExpression(this->parsedExpression);
		p=FuncFactory::create();
		if (expression != this->parsedExpression) {
			p->DefineVar("time", s->getCurrentTimePointer());
			s->setHasTimeDependentFunctions(true);
		}

		//Give the local observable to the function so it can be used
		for(unsigned int i=0; i<n_varRefs; i++) {
			if(this->varRefScope[i]==-1) { //for global variables, use the global observable
				s->getObservableByName(this->varObservableNames[i])->addReferenceToMyself(this->varRefNames[i],p);
			} else { //for local observables, use this function's observable
				this->varLocalObservables[i]->addReferenceToMyself(this->varRefNames[i],p);
			}
		}

		//Set the constant values
		for(unsigned int i=0; i<this->n_params; i++) {
			p->DefineConst(this->paramNames[i],s->getParameter(paramNames[i]));
		}

		//Finally, we can set the expression
		p->SetExpr(expression);

	//Catch anything that goes astray
	} catch (mu::Parser::exception_type &e) {
		cout<<"Error creating local function "<<name<<" in class LocalFunction!!  This is what happened:"<<endl;
		cout<< "  "<<e.GetMsg() << endl;
		cout<<"Quitting."<<endl;
		exit(1);
	}
}


double LocalFunction::getValue(Molecule *m, int scope)
{
	//cout<<"getting local function value: "<<this->nicename<<endl;
	//cout<<"using molecule: "<<m->getUniqueID()<<" with scope: "<<scope<<endl;

	/* A parser-level state lookup has no muParser object by design.  Legacy
	 * callers can still ask for it on a molecule of the wrong type; the old
	 * observable-backed implementation returned zero for that case. */
	if (directStateLookup) {
		if (m == 0 || m->getMoleculeType() != simpleStateMoleculeType)
			return 0.0;
		return evaluateSimpleState(m);
	}

	if(scope==LocalFunction::SPECIES) {
		//cout<<"Species scope"<<endl;
		for(int ti=0; ti<n_typeImolecules; ti++) {
			//cout << "this molecule has type: " << m->getMoleculeTypeName() << endl;
			//cout << "current typeI_mol is: " << typeI_mol[ti]->getName() << endl;
			if(m->getMoleculeType()==typeI_mol[ti]) {
				return m->getLocalFunctionValue(typeI_localFunctionIndex[ti]);
			}
		}
		LocalFunctionException lfe;
		lfe.setType1_Mol(typeI_mol, n_typeImolecules);
		throw lfe;

	} else if(scope==LocalFunction::MOLECULE) {
		//cout<<"Molecule scope."<<endl;
		if (hasSimpleStateDependency && m != 0 &&
				m->getMoleculeType() == simpleStateMoleculeType) {
			return evaluateSimpleState(m);
		}

		//For each of our variables
		int matches = 0;
		for(unsigned int i=0; i<n_varRefs; i++) {
			if(varLocalObservables[i]!=0) {   //If it is local
				if(varLocalObservables[i]->getType()==Observable::MOLECULES) {
					varLocalObservables[i]->clear();  //clear it first
					matches = varLocalObservables[i]->isObservable(m);
					for(int k=0; k<matches; k++) {
						varLocalObservables[i]->straightAdd();
					}
				} else {
					cerr<<"Error in LocalFunction::evaluateOn()! cannot handle Species observable when"<<endl;
					cerr<<"evaluating on a single molecule."<<endl;
					exit(1);
				}
			}
		}


		//Recalculate the function
		double newValue = FuncFactory::Eval(p);
		//cout<<"*"<<this->name<<" "<<newValue<<"\n";
		return newValue;


	} else {
		cout<<"Internal error in LocalFunction::evaluateOn()! trying to evaluate a function with unknown scope."<<endl;
		exit(1);

	}

	cout<<"Internal error in LocalFunction::evaluateOn()! Trying to evaluate a function with unknown scope."<<endl;
	return -1;

}



//This only accepts one molecule, because there can only be one argument
//if we can have multiple arguments, this must be extended to have an
//array of molecules (as in a composite function evaluation)
double LocalFunction::evaluateOn(Molecule *m, int scope) {

	//cout<<"evaluating local function: "<<this->nicename<<endl;
	//this->printDetails(m->getMoleculeType()->getSystem());
	//cout<<"using molecule: "<<m->getUniqueID()<<" with scope: "<<scope<<endl;

	if (directStateLookup) {
		if (m == 0 || m->getMoleculeType() != simpleStateMoleculeType)
			return 0.0;
	}

	if(scope==LocalFunction::SPECIES) {

		if(!isEverEvaluatedOnSpeciesScope) {
			return this->evaluateOn(m, LocalFunction::MOLECULE);
		}

		if(!system->getEvaluateComplexScopedLocalFunctions()) {
			return 0;
		}

		molList.clear();
		m->traverseBondedNeighborhood(molList,ReactionClass::NO_LIMIT);
		return this->evaluateOn(m, molList);

	} else if(scope==LocalFunction::MOLECULE) {
		//cout<<"evaluating on Molecule scope."<<endl;

		if (hasSimpleStateDependency && m != 0 &&
				m->getMoleculeType() == simpleStateMoleculeType) {
			double newValue = evaluateSimpleState(m);
			for (int ti=0; ti<n_typeImolecules; ++ti) {
				if (m->getMoleculeType() == typeI_mol[ti]) {
					m->setLocalFunctionValue(newValue,
							this->typeI_localFunctionIndex[ti]);
					m->setLocalFunctionState(
							this->typeI_localFunctionIndex[ti],
							m->getComponentState(simpleStateComponent));
					m->updateDORRxnValues(
							this->typeI_localFunctionIndex[ti]);
				}
			}
			return newValue;
		}


		int matches = 0;
		for(unsigned int i=0; i<n_varRefs; i++) {
			if(varLocalObservables[i]!=0) {   //If it is local
				if(varLocalObservables[i]->getType()==Observable::MOLECULES) {
					varLocalObservables[i]->clear();  //clear it first
					matches = varLocalObservables[i]->isObservable(m);
					for(int k=0; k<matches; k++) {
						varLocalObservables[i]->straightAdd();
					}
				} else {
					cerr<<"Error in LocalFunction::evaluateOn()! cannot handle Species observable when"<<endl;
					cerr<<"evaluating on a single molecule."<<endl;
					exit(1);
				}
			}
		}

		//Recalculate the function

		double newValue = FuncFactory::Eval(p);
		//cout<<this->name<<" "<<newValue<<"\n";

		//Update the function values
		for(int ti=0; ti<n_typeImolecules; ti++) {
			if(m->getMoleculeType()==typeI_mol[ti]) {
				m->setLocalFunctionValue(newValue,this->typeI_localFunctionIndex[ti]);
				if (hasSimpleStateDependency &&
						m->getMoleculeType() == simpleStateMoleculeType) {
					m->setLocalFunctionState(this->typeI_localFunctionIndex[ti],
							m->getComponentState(simpleStateComponent));
				}
				m->updateDORRxnValues(
						this->typeI_localFunctionIndex[ti]);
			}
		}

		//cout<<name<<" "<<newValue<<"\n";
	//	this->printDetails(m->getMoleculeType()->getSystem());
		return newValue;


	} else {
		cout<<"Internal error in LocalFunction::evaluateOn()! trying to evaluate a function with unknown scope."<<endl;
		exit(1);

	}

	return -1;
}

double LocalFunction::evaluateOn(Molecule *m, const list <Molecule *> &members) {
	if(!isEverEvaluatedOnSpeciesScope || directStateLookup) {
		if (hasSimpleStateDependency) {
			Molecule *stateMolecule = 0;
			if (m != 0 && m->getMoleculeType() == simpleStateMoleculeType)
				stateMolecule = m;
			if (stateMolecule == 0) {
				for (list<Molecule *>::const_iterator memberIter = members.begin();
						memberIter != members.end(); ++memberIter) {
					if (*memberIter != 0 &&
							(*memberIter)->getMoleculeType() == simpleStateMoleculeType) {
						stateMolecule = *memberIter;
						break;
					}
				}
			}
			if (stateMolecule != 0) {
				int stateValue =
						stateMolecule->getComponentState(simpleStateComponent);
				Molecule *cachedMolecule = 0;
				bool cacheValid = true;
				for (list<Molecule *>::const_iterator memberIter = members.begin();
						memberIter != members.end(); ++memberIter) {
					for (int ti=0; ti<n_typeImolecules; ++ti) {
						if (*memberIter != 0 &&
								(*memberIter)->getMoleculeType() == typeI_mol[ti]) {
							if (cachedMolecule == 0)
								cachedMolecule = *memberIter;
							if (!(*memberIter)->isLocalFunctionStateCurrent(
									this->typeI_localFunctionIndex[ti], stateValue))
								cacheValid = false;
						}
					}
				}
				if (cachedMolecule != 0 && cacheValid) {
					for (int ti=0; ti<n_typeImolecules; ++ti) {
						if (typeI_mol[ti] == cachedMolecule->getMoleculeType())
							return cachedMolecule->getLocalFunctionValue(
									this->typeI_localFunctionIndex[ti]);
					}
				}
				double newValue = evaluateSimpleState(stateMolecule);
				for (list<Molecule *>::const_iterator memberIter = members.begin();
						memberIter != members.end(); ++memberIter) {
					for (int ti=0; ti<n_typeImolecules; ++ti) {
						if (*memberIter != 0 &&
								(*memberIter)->getMoleculeType() == typeI_mol[ti]) {
							int localIndex = typeI_localFunctionIndex[ti];
							if (!(*memberIter)->isLocalFunctionStateCurrent(
									localIndex, stateValue)) {
								(*memberIter)->setLocalFunctionValue(newValue,
										localIndex);
								(*memberIter)->setLocalFunctionState(localIndex,
										stateValue);
								(*memberIter)->updateDORRxnValues(localIndex);
							}
						}
					}
				}
				return newValue;
			}
		}
		return this->evaluateOn(m, LocalFunction::MOLECULE);
	}

	if(!system->getEvaluateComplexScopedLocalFunctions()) {
		return 0;
	}

	//First, clear out all the observables
	for(unsigned int i=0; i<n_varRefs; i++) {
		if(varLocalObservables[i]!=0) {
			varLocalObservables[i]->clear();
		}
	}

	//recompute the observables
	int matches = 0;
	for(list<Molecule *>::const_iterator memberIter=members.begin(); memberIter!=members.end(); memberIter++) {

		//Loop over each observable
		for(unsigned int i=0; i<n_varRefs; i++) {
			if(varLocalObservables[i]!=0) {   //If it is local

				//If the observable is of type MOLECULES
				if(varLocalObservables[i]->getType()==Observable::MOLECULES) {
					matches = varLocalObservables[i]->isObservable((*memberIter));
					varLocalObservables[i]->straightAdd(matches);
				}
				//If the observables is of a different type
				else {
					cerr<<"Error in LocalFunction::evaluateOn()! cannot handle this observable type when"<<endl;
					cerr<<"evaluating on a connected component."<<endl;
					exit(1);
				}
			}
		}

	}

	//evaluate the function
	double newValue = FuncFactory::Eval(p);


	//Here we have to notify the type I molecules that this function has changed
	//Update the molecules (Type I) that needed this function evaluated...
	for(list<Molecule *>::const_iterator memberIter=members.begin(); memberIter!=members.end(); memberIter++) {
		for(int ti=0; ti<n_typeImolecules; ti++) {
			if((*memberIter)->getMoleculeType()==typeI_mol[ti]) {
				(*memberIter)->setLocalFunctionValue(newValue,this->typeI_localFunctionIndex[ti]);
				(*memberIter)->updateDORRxnValues(
						this->typeI_localFunctionIndex[ti]);
			}
		}
	}

	//cout<<"*"<<this->name<<" "<<newValue<<"\n";
	return newValue;
}

//This version accepts a complex and evaluates the LocalFunction with SPECIES scope.
double LocalFunction::evaluateOn(Complex *c) {

	if (!isEverEvaluatedOnSpeciesScope) {
		// If this is a molecule-scoped function, we still might want to update
		// all molecules in the complex if this was called.
		if (hasSimpleStateDependency) {
			for (molIter = (c->complexMembers).begin();
					molIter != (c->complexMembers).end(); ++molIter) {
				if ((*molIter)->getMoleculeType() == simpleStateMoleculeType)
					return this->evaluateOn(*molIter, c->complexMembers);
			}
		}
		double val = 0;
		for ( molIter = (c->complexMembers).begin(); molIter!=(c->complexMembers).end(); ++molIter) {
			val = this->evaluateOn((*molIter), LocalFunction::MOLECULE);
		}
		return val;
	}

	if(!system->getEvaluateComplexScopedLocalFunctions()) {
		return 0;
	}

	//First, clear out all the observables
	for(unsigned int i=0; i<n_varRefs; i++) {
		if (varLocalObservables[i]!=0) {
			varLocalObservables[i]->clear();
		}
	}

	//recompute the observables
	int matches = 0;
	for ( molIter = (c->complexMembers).begin(); molIter!=(c->complexMembers).end(); ++molIter) {
		//Loop over each observable
		for(unsigned int i=0; i<n_varRefs; i++) {
			if(varLocalObservables[i]!=0) {
				//If the observable is of type MOLECULES
				if(varLocalObservables[i]->getType()==Observable::MOLECULES) {
					matches = varLocalObservables[i]->isObservable((*molIter));
					varLocalObservables[i]->straightAdd(matches);
				}
				//If the observables is of a different type
				else {
					cerr<<"Error in LocalFunction::evaluateOn()! cannot handle Species observable when"<<endl;
					cerr<<"evaluating on a single molecule."<<endl;
					exit(1);
				}
			}
		}

	}

	//evaluate the function
	double newValue = FuncFactory::Eval(p);

	//Here we have to notify the type I molecules that this function has changed
	//Update the molecules (Type I) that needed this function evaluated...
	for (molIter=(c->complexMembers).begin(); molIter!=(c->complexMembers).end(); ++molIter) {
		for(int ti=0; ti<n_typeImolecules; ti++) {
			if ((*molIter)->getMoleculeType()==typeI_mol[ti]) {
				(*molIter)->setLocalFunctionValue(newValue,this->typeI_localFunctionIndex[ti]);
				(*molIter)->updateDORRxnValues(
						this->typeI_localFunctionIndex[ti]);
			}
		}
	}
	return newValue;
}


LocalFunction::~LocalFunction() {

	delete [] argNames;
	delete [] paramNames;


	for(unsigned int i=0; i<n_varRefs; i++) {
		delete varLocalObservables[i];
	}

	delete [] varRefNames;
	delete [] varObservableNames;
	delete [] varRefScope;
	delete [] varLocalObservables;
	delete [] simpleStateValues;
	delete [] simpleStateCacheValues;
	delete [] simpleStateCacheValid;
	delete [] typeII_mol;
	delete [] typeII_localFunctionIndex;
	delete [] typeI_mol;
	delete [] typeI_localFunctionIndex;


	if(p!=NULL) delete p;
}




/*void LocalFunction::setEvaluationLevel(int eLevel) {

	if(eLevel<0 || eLevel>1) {
		cout<<"Error when setting evaluation level of function: "<<getNiceName();
		cout<<"\nEvaluation level given was:"<<eLevel<<" but currently only supports levels of 0 or 1."<<endl;
		exit(1);
	}

	this->evaluationLevel = eLevel;
}*/


//This function is generally called by a DOR reaction class once the
//DOR reaction class has established that the value of this function
//is required for some moleculetype...
void LocalFunction::addTypeIMoleculeDependency(MoleculeType *mt,
		ReactionClass *rxn, int reactionPosition) {

	//First, make sure we haven't added this bad boy yet
	for(int i=0; i<this->n_typeImolecules; i++) {
		if(typeI_mol[i]==mt) {
			if (rxn != 0 && reactionPosition >= 0)
				mt->addTypeILocalFunctionReaction(
						typeI_localFunctionIndex[i], rxn, reactionPosition);
			return;
		}
	}

	//First, add myself to the moleculeType
	int index = mt->addLocalFunc_TypeI(this);
	this->typeI_mol[this->n_typeImolecules]=mt;
	this->typeI_localFunctionIndex[this->n_typeImolecules]=index;
	if (rxn != 0 && reactionPosition >= 0)
		mt->addTypeILocalFunctionReaction(index, rxn, reactionPosition);
	this->n_typeImolecules++;
}

/*
int LocalFunction::getIndexOfTypeIFunctionValue(Molecule *m) {

	for(int i=0; i<this->n_typeImolecules; i++) {
		if(typeI_mol[i]==m->getMoleculeType()) return this->typeI_localFunctionIndex[i];
	}
	cout<<"Error when getting the index of a Type I function value in LocalFunction:"<<endl;
	cout<<"Could not find the molecule type: '"<<m->getMoleculeType()->getName()<<"' as a type I molecule of this function: "<<this->getNiceName()<<endl;
	exit(1);
}
*/


void LocalFunction::updateParameters(System *s)
{
	if (directStateLookup)
		return;
	if (simpleStateCacheValid != 0) {
		for (int i = 0; i < simpleStateCacheSize; ++i)
			simpleStateCacheValid[i] = false;
	}
	for(unsigned int i=0; i<n_params; i++) {
		p->DefineConst(paramNames[i],s->getParameter(paramNames[i]));
	}
}

void LocalFunction::printDetails(System *s)
{
	cout<<"Local Function: "+this->nicename+"\n";
	cout<<" = "<<this->originalExpression<<endl;
	cout<<" parsed expression = "<<this->parsedExpression<<endl;
	cout<<" ever evaluated on complex scope? "<<isEverEvaluatedOnSpeciesScope <<endl;

	cout<<"   -Variable References:"<<endl;
	for(unsigned int i=0; i<n_varRefs; i++) {
		if(varRefScope[i]==-1) {
			cout<<"         "<<varObservableNames[i]<<" (scope=global): ";
			cout<<s->getObservableByName(varRefNames[i])->getCount()<<endl;
		} else {
			cout<<"         "<<varObservableNames[i]<<" (scope=";
			cout<<argNames[varRefScope[i]]<<") last evaluated to: ";
			cout<<varLocalObservables[i]->getCount()<<endl;
		}
	}

	if(n_params>0) {
		cout<<"   -Constant Parameters:"<<endl;
		for(unsigned int i=0; i<n_params; i++) {
			cout<<"         "<<paramNames[i]<<" = " << s->getParameter(paramNames[i])<<endl;
		}
	}

	cout<<"   -Type II Molecules (this function depends on these molecules):"<<endl;
	for(unsigned int i=0; i<this->n_typeIImolecules; i++) {
		cout<<"         "<<typeII_mol[i]->getName()<<endl;
	}

	cout<<"   -Type I Molecules (molecules in a dor rxn that depend on this function):"<<endl;
	for(int i=0; i<n_typeImolecules; i++) {
		cout<<"         "<<typeI_mol[i]->getName()<<endl;
	}


	if(p!=0)
		cout<<"   Function last evaluated to: "<<FuncFactory::Eval(p)<<endl;
}
