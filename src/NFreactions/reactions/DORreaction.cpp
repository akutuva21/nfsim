


#include "reaction.hh"



using namespace std;
using namespace NFcore;


//should also accept list of local functions and list of PointerNames for each of the functions...
DORRxnClass::DORRxnClass(
		string name,
		double baseRate,
		string baseRateName,
		TransformationSet *transformationSet,
		CompositeFunction *function,
		vector <string> &lfArgumentPointerNameList, System *s) :
	ReactionClass(name,baseRate,baseRateName,transformationSet,s)
{
	vector <TemplateMolecule *> dorMolecules;

	//////////////////////////////////////////////////////////////////////////////////////////
	//Step 1: Find the DOR reactant, and make sure there is only one.  DOR reactants
	//can be found because they have a LocalFunctionPointer Transformation that keeps
	//information about the pointer onto either a reactant species or a particular molecule
	//in the pattern.
	this->DORreactantIndex = -1;
	for(int r=0; (unsigned)r<n_reactants; r++) {
		for(int i=0; i<transformationSet->getNumOfTransformations(r); i++) {
			Transformation *transform = transformationSet->getTransformation(r,i);
			if((unsigned)transform->getType()==TransformationFactory::LOCAL_FUNCTION_REFERENCE) {

				if(DORreactantIndex==-1)
				{
					if ( transformationSet->getTemplateMolecule(r)->getMoleculeType()->isPopulationType() )
					{   // DOR reactant is a population!
						cout<<"Error when creating DORRxnClass: "<<name<<endl;
						cout<<"DOR reactant cannot be a population type."<<endl;
						exit(1);
					}

					DORreactantIndex=r;
				}
				else if(DORreactantIndex!=r) {
					cout<<"Error when creating DORRxnClass: "<<name<<endl;
					cout<<"DOR reactions currently only support one DOR reactant.  This means that you can"<<endl;
					cout<<"only have a pointer to one or the other of the two reactants, but not both."<<endl;
					exit(1);
				}
			}
		}
	}
	if(DORreactantIndex==-1) {
		cout<<"Error when creating DORRxnClass: "<<name<<endl;
		cout<<"You don't have any pointers onto the Molecules or Species, so you can't have a local function!"<<endl;
		cout<<"That means that this is not a DOR reaction at all!"<<endl;
		exit(1);
	}


	//////////////////////////////////////////////////////////////////////////////////////////
	//Step 2: Some bookkeeping so that we can quickly get the function values from a mapping set
	// Now that we have found the DOR reactant, which can potentially have multiple functions, lets
	// figure out which functions apply to which
	//Array to double check that we have used all pointer references we have created
	bool *hasMatched = new bool [transformationSet->getNumOfTransformations(DORreactantIndex)];
	for(int i=0; i<transformationSet->getNumOfTransformations(DORreactantIndex); i++) hasMatched[i]=false;

	//make sure that we have the right number of functions and argument names
	if((unsigned)function->getNumOfArgs()!=lfArgumentPointerNameList.size()) {
		cout<<"Error when creating DORRxnClass: "<<name<<endl;
		cout<<"Number of arguments and LocalFunctionArgumentPointerNameList size do not match!"<<endl;
		exit(1);
	}


	//
	this->n_argMolecules=lfArgumentPointerNameList.size();
	argIndexIntoMappingSet =  new int [n_argMolecules];
	argMappedMolecule = new Molecule *[n_argMolecules];
	argScope = new int [n_argMolecules];


	for(int i=0; i<(int)lfArgumentPointerNameList.size(); i++) {

		//Now search for the function argument...
		bool match = false;
		for(int k=0; k<transformationSet->getNumOfTransformations(DORreactantIndex); k++) {
			Transformation *transform = transformationSet->getTransformation(DORreactantIndex,k);
			if((unsigned)transform->getType()==TransformationFactory::LOCAL_FUNCTION_REFERENCE) {
				LocalFunctionReference *lfr = static_cast<LocalFunctionReference*>(transform);
				if(lfr->getPointerName()==lfArgumentPointerNameList.at(i)) {
					//If we got here, we found a match, so remember the index of the transformation
					//so we can quickly get the value of the function for any mapping object we try
					//to push on the reactant Tree.

					argIndexIntoMappingSet[i] =  k;
					argMappedMolecule[i] = 0;
					argScope[i] = lfr->getFunctionScope();

					hasMatched[k]=true;
					match=true;
				}
			}
		}
		if(!match){  //If there was no match found, then we've got issues...
			cout<<"Error when creating DOR reaction: "<<name<<endl;
			cout<<"Could not find a match in the templateMolecules for the pointer reference to species/molecule: ";
			cout<<lfArgumentPointerNameList.at(i)<<endl;
			exit(1);
		}
	}

	//Just send out a warning if we didn't use one of the pointer references we were given
	for(int k=0; k<transformationSet->getNumOfTransformations(DORreactantIndex); k++) {
		Transformation *transform = transformationSet->getTransformation(DORreactantIndex,k);
		if((unsigned)transform->getType()==TransformationFactory::LOCAL_FUNCTION_REFERENCE) {
			if(!hasMatched[k]) {
				cout<<endl<<"Warning!  when creating DORrxnClass: "<<name<<endl;
				cout<<"Pointer reference: "<<  static_cast<LocalFunctionReference*>(transform)->getPointerName();
				cout<<" that was provided is not used in the local function definition."<<endl;
	}	}	}


	delete [] hasMatched;

	//////////////////////////////////////////////////////////////////////////////////////////
	///  Step 3: Wheh! now we can finally get on the business of creating the reactant lists
	///  and the reactant tree and setting the usual reactionClass parameters

	//Remember that we are a DOR ReactionClass
	this->reactionType = ReactionClass::DOR_RXN;

	//Set up the reactant tree
	//reactantTree = new ReactantTree(this->DORreactantIndex,transformationSet,4);
	reactantTree = new ReactantTree(this->DORreactantIndex,transformationSet,32,this->system);
	msPairBuffer = new MappingSet*[2];

	//Set up the reactantLists
	reactantLists = new ReactantList *[n_reactants];
	for(unsigned int r=0; r<n_reactants; r++) {
		if((signed)r!=this->DORreactantIndex)
			reactantLists[r]=(new ReactantList(r,transformationSet,25,this->system));
	}

	//Initialize a to zero
	this->a=0;


	//Set the actual function
	this->cf = function;

	//Add type I molecule dependencies, so that when this function
	//is reevaluated on a molecule, the molecule knows to update this reaction.
	//This is only necessary for the DOR reactant.
	cf->addTypeIMoleculeDependency( reactantTemplates[DORreactantIndex]->getMoleculeType() );

}

DORRxnClass::DORRxnClass(
		string name,
		double baseRate,
		string baseRateName,
		TransformationSet *transformationSet,
		int dorReactantIndex,
		System *s,
		unsigned int reactantListInitialCapacity) :
	ReactionClass(name,baseRate,baseRateName,transformationSet,s),
	cf(0),
	DORreactantIndex(dorReactantIndex),
	n_argMolecules(0),
	argIndexIntoMappingSet(0),
	argMappedMolecule(0),
	argScope(0)
{
	/* This constructor is deliberately limited to the same single weighted
	 * reactant model used by ordinary DOR reactions.  EnergyRxnClass uses it
	 * only for two-reactant binding rules (and one-reactant reverse rules). */
	if (dorReactantIndex < 0 || dorReactantIndex >= (int)n_reactants) {
		cerr << "Invalid weighted reactant index when creating DOR reaction: "
		     << name << endl;
		exit(1);
	}
	if (transformationSet->getTemplateMolecule((unsigned)dorReactantIndex)
			->getMoleculeType()->isPopulationType()) {
		cerr << "A weighted DOR reactant cannot be a population type: "
		     << name << endl;
		exit(1);
	}

	this->reactionType = ReactionClass::DOR_RXN;
	this->reactantTree = new ReactantTree(this->DORreactantIndex,
			transformationSet,32);
	this->msPairBuffer = new MappingSet *[n_reactants > 2 ? n_reactants : 2];
	this->reactantLists = new ReactantList *[n_reactants];
	for (unsigned int r=0; r<n_reactants; r++) {
		if ((int)r != this->DORreactantIndex)
			this->reactantLists[r] = new ReactantList(
					r, transformationSet, reactantListInitialCapacity);
	}
	this->a = 0;
}

DORRxnClass::~DORRxnClass() {

	for(unsigned int r=0; r<n_reactants; r++) {
		if(this->DORreactantIndex!=r)
			delete reactantLists[r];
	}

	delete [] reactantLists;
	delete reactantTree;

	delete [] argIndexIntoMappingSet;
	delete [] argMappedMolecule;
	delete [] argScope;
	delete [] msPairBuffer;

}

void DORRxnClass::init() {

	//Here we have to tell the molecules that they are part of this function
	//and for single molecule functions, we have to tell them also that they are in
	//this function, so they need to update thier value should they be transformed
	for(unsigned int r=0; r<n_reactants; r++)
	{
		reactantTemplates[r]->getMoleculeType()->addReactionClass(this,r);
	}
}



void DORRxnClass::remove(Molecule *m, unsigned int reactantPos)
{
	if(reactantPos==(unsigned)this->DORreactantIndex) {

		// handle the DOR reactant
		int rxnIndex = m->getMoleculeType()->getRxnIndex(this,reactantPos);
		if(m->getRxnListMappingId(rxnIndex)>=0) {
			reactantTree->removeMappingSet(m->getRxnListMappingId(rxnIndex));
			m->setRxnListMappingId(rxnIndex,Molecule::NOT_IN_RXN);
		}
	} else {

		// handle it normally...
		ReactantList *rl = reactantLists[reactantPos];
		int rxnIndex = m->getMoleculeType()->getRxnIndex(this,reactantPos);
		if(m->getRxnListMappingId(rxnIndex)>=0) {
			rl->removeMappingSet(m->getRxnListMappingId(rxnIndex));
			m->setRxnListMappingId(rxnIndex,Molecule::NOT_IN_RXN);
		}
	}
}

int DORRxnClass::checkForCollision(Molecule *m, MappingSet* ms, int rxnIndex){
	
	const MappingIdSet& tempSet = m->getRxnListMappingSet(rxnIndex);
	for(MappingIdSet::const_iterator it= tempSet.begin();it!= tempSet.end(); ++it){
		MappingSet* ms2 = reactantTree->getMappingSet(*it);
		if(MappingSet::checkForEquality(ms,ms2)){
			return *it;
		}
	}
	return -1;


}

bool DORRxnClass::tryToAdd(Molecule *m, unsigned int reactantPos) {
	if (system != 0 && system->isProfilingEnabled())
		system->recordProfileMatchCandidate();

	// see BasicRxnClass::tryToAdd()
	if (contextCountsPerComplex[reactantPos] && reactantPos == (unsigned)DORreactantIndex) {
		reactantTree->noteMappedComplexSize(m->getComplex()->getComplexSize());
	}
	if(reactantPos==(unsigned)this->DORreactantIndex) {

		// handle the DOR reactant
		int rxnIndex = m->getMoleculeType()->getRxnIndex(this,reactantPos);


		if(reactantTree->getHasClonedMappings()) {
			while(m->getRxnListMappingId(rxnIndex)>=0) {
				reactantTree->removeMappingSet(m->getRxnListMappingId(rxnIndex));
				m->deleteRxnListMappingId(rxnIndex,m->getRxnListMappingId(rxnIndex));
			}
		}
		//JJT: keep a list containing those mapping sets that will be deleted
		MappingIdSet deleteMs = m->getRxnListMappingSet(rxnIndex);
		symmetricMappingSet.clear();
		if(m->getRxnListMappingId(rxnIndex)>=0) {
			/* JJT: this branch contains those reactions for which a reaction and a molecule had been mapped together before
			*  in a previous cycle. However it is not sufficient to just check if they still mapped, it is necessary to see if 
			*  they still map the same way
			*  or whether some of the mappings are still valid (or there are new mappings to this reation from molecule <m>)
			*/
			
			MappingSet *ms = reactantTree->pushNextAvailableMappingSet();

			comparisonResult = reactantTemplates[reactantPos]->compare(m,reactantTree,ms,false,&symmetricMappingSet);

			if(!comparisonResult) {
				reactantTree->removeMappingSet(ms->getId());
				//JJT: removes any symmetric mapping sets that might have been added since we are not using them
				for(vector<MappingSet *>::iterator it=symmetricMappingSet.begin();it!=symmetricMappingSet.end();++it){
					reactantTree->removeMappingSet((*it)->getId());
				}
			} else {
				//JJT: checking if the mapping set we found is new 
				if (symmetricMappingSet.size() >0){
					//JJT: delete ms since symmetricMappingSet contains all the mapping information we need
					reactantTree->removeMappingSet(ms->getId());
					for(vector<MappingSet *>::iterator it=symmetricMappingSet.begin();it!=symmetricMappingSet.end();++it){
						int mapIndex = checkForCollision(m,*it,rxnIndex);
						if(mapIndex >= 0){
							//JJT: the agent already contains this mapping, so keep  the old one
							deleteMs.erase(mapIndex);
							reactantTree->removeMappingSet((*it)->getId());
						}
						else{
							//JJT: new mapping and we are keeping it, so evaluate the function and confirm the push
							double localFunctionValue = this->evaluateLocalFunctions(*it);

							reactantTree->confirmPush((*it)->getId(),localFunctionValue);
							m->setRxnListMappingId(rxnIndex,(*it)->getId());
						}

					}


				}
				else{
					/*int mapIndex = checkForCollision(m,ms,rxnIndex);
					if(mapIndex >= 0){
						deleteMs.erase(mapIndex);
						reactantTree->removeMappingSet(ms->getId());
						if (deleteMs.size() == 0)
							break;
					}
					else{*/
						//m->setRxnListMappingId(rxnIndex,-1);
						//JJT: If instead the mapping information is a single mapping contained in <ms>...
						double localFunctionValue = this->evaluateLocalFunctions(ms);
						reactantTree->confirmPush(ms->getId(),localFunctionValue);
						m->setRxnListMappingId(rxnIndex,ms->getId());
						//deleteMs.clear()
					}
				
			}

			for(MappingIdSet::iterator it=deleteMs.begin();it!=deleteMs.end(); ++it){
				m->deleteRxnListMappingId(rxnIndex,*it);
				reactantTree->removeMappingSet(*it);
			}

				//if(!comparisonResult.second)
				//	break;

			
			//delete all mappings that were no longer found to match between a molecule and a species

		} else {
			MappingSet *ms = reactantTree->pushNextAvailableMappingSet();

			comparisonResult = reactantTemplates[reactantPos]->compare(m,reactantTree,ms,false,&symmetricMappingSet);
			if(!comparisonResult) {
				reactantTree->removeMappingSet(ms->getId());
			} else {
				if (symmetricMappingSet.size() >0){
					reactantTree->removeMappingSet(ms->getId());
					for(vector<MappingSet *>::iterator it=symmetricMappingSet.begin();it!=symmetricMappingSet.end();++it){
						int mapIndex = checkForCollision(m,*it,rxnIndex);
						if(mapIndex >= 0){
							//the agent already contains this mapping
							reactantTree->removeMappingSet((*it)->getId());		
						}
						else{
							//we are keeping it, so evaluate the function and confirm the push
							double localFunctionValue = this->evaluateLocalFunctions(*it);
							reactantTree->confirmPush((*it)->getId(),localFunctionValue);
							m->setRxnListMappingId(rxnIndex,(*it)->getId());
						}

					}
				}
				else{
					double localFunctionValue = this->evaluateLocalFunctions(ms);
					reactantTree->confirmPush(ms->getId(),localFunctionValue);
					m->setRxnListMappingId(rxnIndex,ms->getId());
					
				}

				//if(!comparisonResult.second)
				//	break;

				//m->printDetails();
				//we are keeping it, so evaluate the function and confirm the push
				//double localFunctionValue = this->evaluateLocalFunctions(ms);
				//reactantTree->confirmPush(ms->getId(),localFunctionValue);
				//m->setRxnListMappingId(rxnIndex,ms->getId());
			}

		}
	} else {

		//Get the specified reactantList
		ReactantList *rl = reactantLists[reactantPos];
		int rxnIndex = m->getMoleculeType()->getRxnIndex(this,reactantPos);

		if(rl->getHasClonedMappings()) {
			/*if(m->getRxnListMappingId(rxnIndex)>=0) {
				rl->removeMappingSet(m->getRxnListMappingId(rxnIndex));
				m->setRxnListMappingId(rxnIndex,Molecule::NOT_IN_RXN);
			}*/
			//JJT: accounting for the fact that we can now have multiple mappings
			while(m->getRxnListMappingId(rxnIndex)>=0) {
				rl->removeMappingSet(m->getRxnListMappingId(rxnIndex));
				m->deleteRxnListMappingId(rxnIndex,m->getRxnListMappingId(rxnIndex));
			}
		}

		//Here we get the standard update...
		if(m->getRxnListMappingId(rxnIndex)>=0) //If we are in this reaction...
		{
			if(!reactantTemplates[reactantPos]->compare(m)) {
				rl->removeMappingSet(m->getRxnListMappingId(rxnIndex));
				m->setRxnListMappingId(rxnIndex,Molecule::NOT_IN_RXN);
			}

		} else {
			//Try to map it!
			MappingSet *ms = rl->pushNextAvailableMappingSet();
			comparisonResult = reactantTemplates[reactantPos]->compare(m,rl,ms);
			//if(!reactantTemplates[reactantPos]->compare(m,rl,ms)) {
			if(!comparisonResult){
				//we must remove, if we did not match.  This will also remove
				//everything that was cloned off of the mapping set
				rl->removeMappingSet(ms->getId());
			} else {
				m->setRxnListMappingId(rxnIndex,ms->getId());
			}
		}




	}
	return true;
}


int DORRxnClass::getReactantCount(unsigned int reactantIndex) const
{
	if(reactantIndex==(unsigned)this->DORreactantIndex) {
		return reactantTree->size();
	}
	return isPopulationType[reactantIndex] ?
		       reactantLists[reactantIndex]->getPopulation()
	         : reactantLists[reactantIndex]->size();
}


int DORRxnClass::getCorrectedReactantCount(unsigned int reactantIndex) const
{
	if(reactantIndex==(unsigned)this->DORreactantIndex) {
		return contextCountsPerComplex[reactantIndex]
		         ? countDistinctComplexes(reactantTree)
		         : reactantTree->size();
	}

	// MatchOnce is the user's explicit request; contextCountsPerComplex is the
	// same counting applied automatically to a reactant the rule never transforms,
	// which is how BNG counts one.
	if ((matchOncePerReactant[reactantIndex] || contextCountsPerComplex[reactantIndex])
	    && !isPopulationType[reactantIndex]) {
		return countDistinctComplexes(reactantLists[reactantIndex]);
	}

	return isPopulationType[reactantIndex] ?
			   std::max( reactantLists[reactantIndex]->getPopulation()
			             - identicalPopCountCorrection[reactantIndex], 0 )
			 : reactantLists[reactantIndex]->size();
}

/*
JJT: this function is called if the default mappingset information is sending the wrong parameter to the local function when using a species
scope label. for now the solution is to try every molecule referenced by the mapping set. This may be inefficient but it will only be as long
as the length of a pattern defined by the user (usually 5-6 mt long) multiplied by the lenght of the observable referenced by the local function,
so O(nm) with n, m <~ 6.
*/
double DORRxnClass::pickLocalFunctionParameter(MappingSet* ms, int index, MoleculeType ** type1_Mol, int n_type1_Mol, int* reactantCounts)
{
		for(unsigned int r=0; r<ms->getNumOfMappings(); r++)
		{
			for(int type1_i=0; type1_i<n_type1_Mol; type1_i++){
				auto it = type1_Mol[type1_i];
				if(it == ms->get(r)->getMolecule()->getMoleculeType()){
					try{
						this->argMappedMolecule[index] = ms->get(r)->getMolecule();
						return this->cf->evaluateOn(this->argMappedMolecule,this->argScope, reactantCounts, this->n_reactants);
						
					}
					catch(LocalFunctionException &lfe){
						if(lfe.getIndex() == index){
							continue;
						}
						else{
							return this->pickLocalFunctionParameter(ms, lfe.getIndex(), lfe.getType1_Mol(), lfe.get_n_type1_Mol(), reactantCounts);
						}
						
					}
				}
			}
		}
		// there is truly no possible mapping. User mistake prob, the error message could use somem improvement
		cout<<"Internal error in LocalFunction::evaluateOn()! Trying to evaluate a function with unknown scope."<<endl;
		exit(1);

}
//This function takes a given mappingset and looks up the value of its local
//functions based on the local functions that were defined
double DORRxnClass::evaluateLocalFunctions(MappingSet *ms)
{
	//Go through each function, and set the value of the function
	//this->argMappedMolecule

	//Grab the molecules needed for the local function to evaluate
	//default initialization


	for(int i=0; i<this->n_argMolecules; i++) {
		this->argMappedMolecule[i] = ms->get(this->argIndexIntoMappingSet[i])->getMolecule();
	}

	int * reactantCounts = new int[this->n_reactants];
	for(unsigned int r=0; r<n_reactants; r++) {
		if(r==this->DORreactantIndex) {
			reactantCounts[r]= reactantTree->size();
		}
		else {
			reactantCounts[r]=reactantLists[r]->size();
		}
	}
	double value;
	try{
		value = this->cf->evaluateOn(argMappedMolecule,argScope, reactantCounts, n_reactants);
	}
	catch(LocalFunctionException &lfe){
		//the parameter sent in argMappedMolecule cannot be mapped to an observable in the local function
		//solution here: just try everything taht we reference in ms for the parameter in question
		value = this->pickLocalFunctionParameter(ms, lfe.getIndex(), lfe.getType1_Mol(), lfe.get_n_type1_Mol(), reactantCounts);
	}
	endloop:
	delete [] reactantCounts;

	return value;


}


double DORRxnClass::update_a() {
	if (useRuleMonkey) {
		a = exactRuleMonkey_a();
		return a;
	}
	a = baseRate;
	for(unsigned int i=0; i<n_reactants; i++) {
		if(i!=DORreactantIndex) {
			a*=(double)getCorrectedReactantCount(i);
		} else {
			// This reactant's propensity comes from the tree's rate factor sum, not
			// from a count, so per-complex counting has to be applied to the sum.
			a *= contextCountsPerComplex[i]
			       ? perComplexRateFactorSum(reactantTree)
			       : reactantTree->getRateFactorSum();
		}
	}
	return a;
}

double DORRxnClass::exactRuleMonkey_a()
{
	if(this->totalRateFlag) {
		double exact_a = baseRate;
		for(unsigned int i=0; i<n_reactants; i++) {
			if(getCorrectedReactantCount(i)==0) exact_a = 0.0;
		}
		return exact_a;
	}

	double validCombinations = 0.0;
	if (n_reactants == 0) {
		validCombinations = 1.0;
	} else if (n_reactants == 1) {
		if (0 == DORreactantIndex) {
			// A DOR reactant's propensity comes from its tree's rate factor sum
			// rather than from a count, so per-complex counting is applied there;
			// see DORRxnClass::update_a().
			validCombinations = contextCountsPerComplex[0]
			                      ? perComplexRateFactorSum(reactantTree)
			                      : reactantTree->getRateFactorSum();
		} else {
			validCombinations = getCorrectedReactantCount(0);
		}
	} else if (n_reactants == 2) {
		// One representative per complex for a pure context reactant, as in
		// BasicRxnClass::exactRuleMonkey_a(), so the enumerated pairs match the
		// counts and rate factor sums the total is built from.
		static thread_local std::vector<MappingSet*> reps0, reps1;
		static thread_local std::vector<int> idx0, idx1;
		collectReactantRepresentatives(reactantLists[0], contextCountsPerComplex[0], reps0, &idx0);
		collectReactantRepresentatives(reactantLists[1], contextCountsPerComplex[1], reps1, &idx1);

		double totalCombinations = 1.0;
		for(unsigned int i=0; i<n_reactants; i++) {
			if(i!=DORreactantIndex) {
				totalCombinations*=(double)getCorrectedReactantCount(i);
			} else {
				totalCombinations *= contextCountsPerComplex[i]
				                       ? perComplexRateFactorSum(reactantTree)
				                       : reactantTree->getRateFactorSum();
			}
		}

		double invalidCombinations = 0;

		for (size_t i = 0; i < reps0.size(); ++i) {
			msPairBuffer[0] = reps0[i];
			for (size_t j = 0; j < reps1.size(); ++j) {
				msPairBuffer[1] = reps1[j];

				if (!transformationSet->checkMolecularity(msPairBuffer)) {
					double weight = 1.0;
					if (0 == DORreactantIndex) {
						weight = reactantTree->getRateFactor(idx0[i]);
					} else if (1 == DORreactantIndex) {
						weight = reactantTree->getRateFactor(idx1[j]);
					}
					invalidCombinations += weight;
				}
			}
		}
		validCombinations = totalCombinations - invalidCombinations;
		if (validCombinations < 0) validCombinations = 0;
	} else {
		validCombinations = 1.0;
		for(unsigned int i=0; i<n_reactants; i++) {
			if(i!=DORreactantIndex) {
				validCombinations*=(double)getCorrectedReactantCount(i);
			} else {
				validCombinations *= contextCountsPerComplex[i]
				                       ? perComplexRateFactorSum(reactantTree)
				                       : reactantTree->getRateFactorSum();
			}
		}
	}

	return validCombinations * baseRate;
}

void DORRxnClass::pickRuleMonkeyMappingSets(double random_A_number) const
{
	if (n_reactants != 2 || totalRateFlag) {
		double rateFactorMultiplier = baseRate;
		for(unsigned int i=0; i<n_reactants; i++) {
			if(i!=(unsigned)DORreactantIndex) {
				if ( isPopulationType[i] ) {
					reactantLists[i]->pickRandomFromPopulation(mappingSet[i]);
				} else {
					reactantLists[i]->pickRandom(mappingSet[i]);
				}
				rateFactorMultiplier*=getReactantCount(i);
			}
		}

		if(random_A_number<0) random_A_number = system->getRNG().random(this->a);
		reactantTree->pickReactantFromValue(mappingSet[DORreactantIndex],random_A_number,rateFactorMultiplier);
		return;
	}

	// For molecularity=2, we have to find a valid pair (no null events)
	int size0 = getReactantCount(0);
	int size1 = getReactantCount(1);
	
	validPairsBuffer.clear();
	validWeightsBuffer.clear();
	double totalWeight = 0.0;
	
	for (int i = 0; i < size0; ++i) {
		msPairBuffer[0] = reactantLists[0]->getMappingSet(i);
		for (int j = 0; j < size1; ++j) {
			msPairBuffer[1] = reactantLists[1]->getMappingSet(j);
			
			if (transformationSet->checkMolecularity(msPairBuffer)) {
				validPairsBuffer.push_back(make_pair(i, j));
				double weight = 1.0;
				if (0 == DORreactantIndex) {
					weight = reactantTree->getRateFactor(i);
				} else if (1 == DORreactantIndex) {
					weight = reactantTree->getRateFactor(j);
				}
				validWeightsBuffer.push_back(weight);
				totalWeight += weight;
			}
		}
	}
	
	if (validPairsBuffer.empty() || totalWeight <= 0) {
		// Safety fallback: this should be unreachable when exactRuleMonkey_a() > 0.
		// If reached, preserve legacy behavior by falling back to the standard selector.
		double rateFactorMultiplier = baseRate;
		for(unsigned int i=0; i<n_reactants; i++) {
			if(i!=(unsigned)DORreactantIndex) {
				if ( isPopulationType[i] ) {
					reactantLists[i]->pickRandomFromPopulation(mappingSet[i]);
				} else {
					reactantLists[i]->pickRandom(mappingSet[i]);
				}
				rateFactorMultiplier*=getReactantCount(i);
			}
		}

		if(random_A_number<0) random_A_number = system->getRNG().random(this->a);
		reactantTree->pickReactantFromValue(mappingSet[DORreactantIndex],random_A_number,rateFactorMultiplier);
		return;
	}
	
	// Select a valid pair weighted by the DOR tree factors
	double randNum = system->getRNG().random(totalWeight);
	double cumulative = 0;
	int selectedIndex = validPairsBuffer.size() - 1;
	for (size_t k = 0; k < validPairsBuffer.size(); ++k) {
		cumulative += validWeightsBuffer[k];
		if (randNum <= cumulative) {
			selectedIndex = k;
			break;
		}
	}
	
	int i = validPairsBuffer[selectedIndex].first;
	int j = validPairsBuffer[selectedIndex].second;
	
	mappingSet[0] = reactantLists[0]->getMappingSet(i);
	mappingSet[1] = reactantLists[1]->getMappingSet(j);
}


void DORRxnClass::pickMappingSets(double randNumber) const
{
	if (useRuleMonkey) {
		pickRuleMonkeyMappingSets(randNumber);
		return;
	}
	//here we cannot just select a random molecule.  This is where all of our hard
	//(as well as all the other reactants.  So here we go...
	double rateFactorMultiplier = baseRate;
	for(unsigned int i=0; i<n_reactants; i++) {
		if(i!=(unsigned)DORreactantIndex) {
			if ( isPopulationType[i] ) {
				reactantLists[i]->pickRandomFromPopulation(mappingSet[i]);
			} else {
				reactantLists[i]->pickRandom(mappingSet[i]);
			}
			rateFactorMultiplier*=getReactantCount(i);
		}
	}

	if(randNumber<0) randNumber = system->getRNG().random(this->a);
	reactantTree->pickReactantFromValue(mappingSet[DORreactantIndex],randNumber,rateFactorMultiplier);

	//mappingSet[DORreactantIndex]->printDetails();
	//reactantTree->printDetails();
}

void DORRxnClass::notifyRateFactorChange(Molecule * m, int reactantIndex, int rxnListIndex) {
	if(reactantIndex==DORreactantIndex) {
		double newValue = evaluateLocalFunctions(reactantTree->getMappingSet(rxnListIndex));
		reactantTree->updateValue(rxnListIndex,newValue);
	} else {
		cout<<"Internal Error in DORRxnClass::notifyRateFactorChange!!  : trying to change a rate\n";
		cout<<"factor of a non-DOR reactant.  That means this function was called in error!\n";
		exit(1);
	}
}


void DORRxnClass::printDetails() const
{
	cout<<"DORRxnClass: " << name <<"  ( baseRate="<<baseRate<<",  a="<<a<<", fired="<<fireCounter<<" times )"<<endl;
	for(unsigned int r=0; r<n_reactants; r++)
	{
		if(r!=(unsigned)DORreactantIndex) {
			cout<<"      -|"<< this->getReactantCount(r)<<" mappings|\t";
			cout<<this->reactantTemplates[r]->getPatternString()<<"\n";
		} else {

			cout<<"      -(DOR) |"<< this->getReactantCount(r)<<" mappings|\t";
			cout<<this->reactantTemplates[r]->getPatternString()<<"\n";
			cout<<"             (rateFactorSum="<<reactantTree->getRateFactorSum();
			cout<<")."<<endl;
		    //reactantTree->printDetails();
		}
	}

	//this->printFullDetails();

	if(n_reactants==0)
		cout<<"      >No Reactants: so this rule either creates new species or does nothing."<<endl;
}







EnergyRxnClass::EnergyRxnClass(
		string name,
		double baseRate,
		string baseRateName,
		TransformationSet *transformationSet,
		int dorReactantIndex,
		const EnergyBindingContext &context,
		double phi,
		double RT,
		bool isForward,
		System *s) :
	/* A compact partner list typically tracks many simple molecules; use a
	 * smaller starting capacity so doubling stops at 1024 rather than 1600 for
	 * a 1,000-molecule workload. */
	DORRxnClass(name,baseRate,baseRateName,transformationSet,dorReactantIndex,s,16),
	conditionalTerms(context.conditionalTerms),
	componentMaskFastPath(true),
	baseEnergy(context.baseEnergy),
	phi(phi),
	RT(RT),
	isForward(isForward),
	simpleMembership(false),
	reactionCenterComponentIndex(-1),
	partnerComponentIndex(-1),
	partnerMoleculeType(0),
	weightedDependencyMask(0),
	dependencyMaskValid(true),
	singleConditionalTermFastPath(false),
	baseEnergyRateFactor(0.0),
	conditionedEnergyRateFactor(0.0),
	directProductListDecisionKnown(false),
	directProductListSafe(false)
{
	/* The compact input path currently supplies contexts on the first
	 * reaction-center molecule.  Its mapping is the first mapping in both the
	 * forward two-reactant rule and the reverse connected rule. */
	if (dorReactantIndex != 0 || context.conditions.empty()) {
		cerr << "Invalid compact energy reaction context for " << name << endl;
		exit(1);
	}

	MoleculeType *weightedType = reactantTemplates[dorReactantIndex]->getMoleculeType();
	if (!context.conditions.empty() &&
			weightedType->getName() != context.conditions.front().molType) {
		cerr << "Compact energy reaction weighted template does not match its "
			 << "context molecule type for " << name << endl;
		exit(1);
	}
	for (const auto &condition : context.conditions) {
		if (condition.reactantIdx != 0) {
			cerr << "Compact energy reaction context is not on reactant 0 for "
			     << name << endl;
			exit(1);
		}
		conditionComponentIndices.push_back(
			weightedType->getCompIndexFromName(condition.compName));
	}
	for (unsigned int ti=0; ti<conditionalTerms.size(); ti++) {
		std::uint64_t componentMask = 0;
		for (unsigned int ci=0; ci<conditionComponentIndices.size(); ci++) {
			if ((conditionalTerms[ti].conditionMask &
					(std::uint64_t(1) << ci)) == 0)
				continue;
			int componentIndex = conditionComponentIndices[ci];
			if (componentIndex < 0 || componentIndex >= 64) {
				componentMaskFastPath = false;
				break;
			}
			componentMask |=
					(std::uint64_t(1) << componentIndex);
		}
		conditionalComponentMasks.push_back(componentMask);
	}

	/* Compact input creates only a reaction-center constraint on the weighted
	 * molecule.  Cache its two binding endpoints so membership refreshes can
	 * update the DOR tree directly instead of recursively comparing the
	 * template on every energy reaction.  Leave unusual/symmetric templates on
	 * the general DOR path. */
	if (transformationSet->getNumOfTransformations(0) > 0) {
		Transformation *center = transformationSet->getTransformation(0, 0);
		reactionCenterComponentIndex = center->getComponentIndex();
		if (isForward && n_reactants == 2 &&
				transformationSet->getNumOfTransformations(1) == 1 &&
				center->getType() == TransformationFactory::BINDING) {
			Transformation *partner = transformationSet->getTransformation(1, 0);
			partnerComponentIndex = partner->getComponentIndex();
			partnerMoleculeType = reactantTemplates[1]->getMoleculeType();
			simpleMembership =
				partner->getType() == TransformationFactory::EMPTY &&
				reactantTemplates[0]->getN_symComps() == 0 &&
				reactantTemplates[0]->getN_connectedTo() == 0 &&
				reactantTemplates[1]->getN_symComps() == 0 &&
				reactantTemplates[1]->getN_connectedTo() == 0;
		} else if (!isForward && n_reactants == 1 &&
				transformationSet->getNumOfTransformations(0) == 2 &&
				center->getType() == TransformationFactory::UNBINDING) {
			Transformation *partner = transformationSet->getTransformation(0, 1);
			partnerComponentIndex = partner->getComponentIndex();
			TemplateMolecule *partnerTemplate = partner->getTemplateMolecule();
			partnerMoleculeType = partnerTemplate->getMoleculeType();
			simpleMembership =
				partner->getType() == TransformationFactory::EMPTY &&
				reactantTemplates[0]->getN_symComps() == 0 &&
				reactantTemplates[0]->getN_connectedTo() == 0 &&
				partnerTemplate->getN_symComps() == 0 &&
				partnerTemplate->getN_connectedTo() == 0;
		}
	}

	if (simpleMembership) {
		if (reactionCenterComponentIndex < 0 ||
				reactionCenterComponentIndex >= 64) {
			dependencyMaskValid = false;
		} else {
			weightedDependencyMask |=
					(std::uint64_t(1) << reactionCenterComponentIndex);
		}
		for (unsigned int ci=0; ci<conditionComponentIndices.size(); ci++) {
			int componentIndex = conditionComponentIndices[ci];
			if (componentIndex < 0 || componentIndex >= 64) {
				dependencyMaskValid = false;
			} else {
				weightedDependencyMask |=
						(std::uint64_t(1) << componentIndex);
			}
		}
	}

	/* Most generated promoter rules have one conditional energy term.  Its
	 * occupancy test still changes per refresh, but the two Arrhenius factors
	 * do not.  Cache those factors so the hot membership path only performs the
	 * mask test; multi-term contexts retain the general evaluator below. */
	if (componentMaskFastPath && conditionalTerms.size() == 1) {
		double energyCoefficient = isForward ? phi : (phi - 1.0);
		baseEnergyRateFactor =
			exp(-(energyCoefficient * baseEnergy) / RT);
		conditionedEnergyRateFactor =
			exp(-(energyCoefficient *
				(baseEnergy + conditionalTerms[0].energyValue)) / RT);
		singleConditionalTermFastPath = true;
	}
}

bool EnergyRxnClass::tryToAdd(Molecule *m, unsigned int reactantPos)
{
	if (!simpleMembership)
		return DORRxnClass::tryToAdd(m, reactantPos);

	ReactantList *partnerList = 0;
	if (isForward && reactantPos == 1) {
		partnerList = reactantLists[reactantPos];
		if (partnerList->getHasClonedMappings())
			return DORRxnClass::tryToAdd(m, reactantPos);
	}

	if (system != 0 && system->isProfilingEnabled())
		system->recordProfileMatchCandidate();

	/* The unweighted ligand side of a compact forward rule has exactly one
	 * empty-site constraint.  Keep it in the ordinary ReactantList, but avoid
	 * recursively comparing its one-component template on every refresh. */
	if (isForward && reactantPos == 1) {
		ReactantList *rl = partnerList;

		int rxnIndex = m->getMoleculeType()->getRxnIndex(this, reactantPos);
		bool matches = m->isBindingSiteOpen(partnerComponentIndex);
		if (!matches) {
			while (m->getRxnListMappingId(rxnIndex) >= 0) {
				int mappingId = m->getRxnListMappingId(rxnIndex);
				m->deleteRxnListMappingId(rxnIndex, mappingId);
				rl->removeMappingSet(mappingId);
			}
		} else if (m->getRxnListMappingId(rxnIndex) < 0) {
			MappingSet *mappingSet = rl->pushNextAvailableMappingSet();
			mappingSet->set(0, m);
			m->setRxnListMappingId(rxnIndex, mappingSet->getId());
		}
		return true;
	}

	if (reactantPos != (unsigned int)DORreactantIndex)
		return DORRxnClass::tryToAdd(m, reactantPos);

	int rxnIndex = m->getMoleculeType()->getRxnIndex(this, reactantPos);
	Molecule *partnerMolecule = 0;
	bool matches = false;
	if (isForward) {
		matches = m->isBindingSiteOpen(reactionCenterComponentIndex);
	} else if (m->isBindingSiteBonded(reactionCenterComponentIndex)) {
		partnerMolecule = m->getBondedMolecule(reactionCenterComponentIndex);
		matches = partnerMolecule != 0 &&
				partnerMolecule->getMoleculeType() == partnerMoleculeType &&
				m->getBondedMoleculeBindingSiteIndex(reactionCenterComponentIndex) ==
					partnerComponentIndex;
	}

	if (!matches) {
		while (m->getRxnListMappingId(rxnIndex) >= 0) {
			int mappingId = m->getRxnListMappingId(rxnIndex);
			m->deleteRxnListMappingId(rxnIndex, mappingId);
			reactantTree->removeMappingSet(mappingId);
		}
		return true;
	}

	const MappingIdSet& existingMappings = m->getRxnListMappingSet(rxnIndex);
	if (!existingMappings.empty()) {
		for (MappingIdSet::const_iterator it = existingMappings.begin();
				it != existingMappings.end(); ++it) {
			MappingSet *mappingSet = reactantTree->getMappingSet(*it);
			mappingSet->set(0, m);
			if (!isForward) mappingSet->set(1, partnerMolecule);
			reactantTree->updateValue(
					*it, evaluateLocalFunctions(mappingSet));
		}
		return true;
	}

	MappingSet *mappingSet = reactantTree->pushNextAvailableMappingSet();
	mappingSet->set(0, m);
	if (!isForward) mappingSet->set(1, partnerMolecule);
	reactantTree->confirmPush(
				mappingSet->getId(), evaluateLocalFunctions(mappingSet));
	m->setRxnListMappingId(rxnIndex, mappingSet->getId());
	return true;
}

bool EnergyRxnClass::dependsOnEndpoint(
		MoleculeType *targetMoleculeType,
		MoleculeType *changedMoleculeType,
		int changedComponentIndex) const
{
	MoleculeType *weightedType = reactantTemplates[0]->getMoleculeType();
	if (targetMoleculeType == weightedType &&
			changedMoleculeType == weightedType) {
		if (dependencyMaskValid && changedComponentIndex >= 0 &&
				changedComponentIndex < 64) {
			return (weightedDependencyMask &
					(std::uint64_t(1) << changedComponentIndex)) != 0;
		}
		if (changedComponentIndex == reactionCenterComponentIndex)
			return true;
		for (unsigned int ci=0; ci<conditionComponentIndices.size(); ci++) {
			if (changedComponentIndex == conditionComponentIndices[ci])
				return true;
		}
	}

	return targetMoleculeType == partnerMoleculeType &&
			changedMoleculeType == partnerMoleculeType &&
			changedComponentIndex == partnerComponentIndex;
}

bool EnergyRxnClass::shouldUpdateMembership(
		Molecule *m, ReactionClass *firedReaction, bool directProduct) const
{
	if (!simpleMembership || firedReaction == 0 ||
			!firedReaction->usesIncrementalMembership())
		return true;

	const EnergyRxnClass *firedEnergy =
		dynamic_cast<const EnergyRxnClass *>(firedReaction);
	if (firedEnergy == 0 || !firedEnergy->simpleMembership)
		return true;

	/* A compact binding/unbinding rule changes only the molecules explicitly
	 * mapped by that rule.  The product list may also contain the rest of the
	 * connected complex, but those indirect molecules have no changed local
	 * endpoint that can affect another compact energy membership list. */
	if (!directProduct)
		return false;

	MoleculeType *targetMoleculeType = m->getMoleculeType();
	if (dependsOnEndpoint(targetMoleculeType,
			firedEnergy->reactantTemplates[0]->getMoleculeType(),
			firedEnergy->reactionCenterComponentIndex))
		return true;
	if (dependsOnEndpoint(targetMoleculeType,
			firedEnergy->partnerMoleculeType,
			firedEnergy->partnerComponentIndex))
		return true;
	return false;
}

bool EnergyRxnClass::canSkipIndirectMembership(
		ReactionClass *firedReaction) const
{
	if (!simpleMembership || firedReaction == 0 ||
			!firedReaction->usesIncrementalMembership())
		return false;

	const EnergyRxnClass *firedEnergy =
			dynamic_cast<const EnergyRxnClass *>(firedReaction);
	return firedEnergy != 0 && firedEnergy->simpleMembership;
}

bool EnergyRxnClass::canUseDirectProductList() const
{
	if (directProductListDecisionKnown)
		return directProductListSafe;

	bool safe = simpleMembership && system != 0 &&
			/* On-the-fly observables need the complete affected complex; with
			 * -notf they are rebuilt at output time instead. */
			(!system->getOnTheFlyObservables() ||
			 system->getNumOfObsForOutput() == 0) &&
			/* The compact constructor never creates added molecules. */
			transformationSet->getNumOfAddMoleculeTransforms() == 0 &&
			/* Product filters inspect the complete post-transform complexes. */
			!transformationSet->hasProductFilters();

	if (safe) {
		/* The direct list omits every indirect molecule.  Require both that no
		 * Type-II function needs those molecules and that every reaction
		 * registered on every molecule type agrees that an indirect refresh can
		 * be skipped.  This is intentionally conservative because the product
		 * complex is not traversed on this path. */
		for (int i = 0; i < system->getNumOfMoleculeTypes(); ++i) {
			MoleculeType *mt = system->getMoleculeType(i);
			if (mt->getNumOfTypeIIFunctions() > 0 ||
					!mt->canSkipIndirectMembership(
						const_cast<EnergyRxnClass *>(this))) {
				safe = false;
				break;
			}
		}
	}

	directProductListSafe = safe;
	directProductListDecisionKnown = true;
	return safe;
}

double EnergyRxnClass::evaluateLocalFunctions(MappingSet *ms)
{
	if (ms == 0 || ms->getNumOfMappings() == 0 || ms->get(0) == 0 ||
			ms->get(0)->getMolecule() == 0) {
		return 0.0;
	}

	Molecule *weightedMolecule = ms->get(0)->getMolecule();
	if (singleConditionalTermFastPath) {
		std::uint64_t boundMask = weightedMolecule->getBoundComponentMask();
		return (boundMask & conditionalComponentMasks[0]) ==
				conditionalComponentMasks[0]
			? conditionedEnergyRateFactor : baseEnergyRateFactor;
	}

	double deltaG = baseEnergy;
	if (componentMaskFastPath) {
		std::uint64_t boundMask = weightedMolecule->getBoundComponentMask();
		for (unsigned int ti=0; ti<conditionalTerms.size(); ti++) {
			std::uint64_t requiredMask = conditionalComponentMasks[ti];
			if ((boundMask & requiredMask) == requiredMask)
				deltaG += conditionalTerms[ti].energyValue;
		}
	} else {
		std::uint64_t conditionMask = 0;
		for (unsigned int ci=0; ci<conditionComponentIndices.size(); ci++) {
			if (weightedMolecule->isBindingSiteBonded(conditionComponentIndices[ci]))
				conditionMask |= (std::uint64_t(1) << ci);
		}
		for (const auto &term : conditionalTerms) {
			if ((conditionMask & term.conditionMask) == term.conditionMask)
				deltaG += term.energyValue;
		}
	}

	/* DOR's base rate carries exp(-Ea0/RT); this factor carries only the
	 * context-dependent Arrhenius contribution. */
	double energyCoefficient = isForward ? phi : (phi - 1.0);
	return exp(-(energyCoefficient * deltaG) / RT);
}

double EnergyRxnClass::exactRuleMonkey_a()
{
	/* Keep the same total-rate convention as DORRxnClass.  The compact path
	 * is otherwise a two-reactant microscopic rule with the first reactant
	 * weighted by its context-dependent energy factor. */
	if (totalRateFlag) {
		double exact_a = baseRate;
		for (unsigned int i = 0; i < n_reactants; i++) {
			if (getCorrectedReactantCount(i) == 0) exact_a = 0.0;
		}
		return exact_a;
	}

	if (n_reactants != 2 || DORreactantIndex != 0) {
		return DORRxnClass::exactRuleMonkey_a();
	}

	/* The inherited DOR implementation expects a ReactantList at index 0,
	 * but the compact reaction stores the weighted first reactant in its
	 * ReactantTree.  Enumerate the same valid pairs directly, retaining the
	 * tree's rate factor for each first-reactant mapping.  The context list is
	 * intentionally not collapsed here: this matches the counting semantics
	 * of the base branch on which this reaction class is compiled. */
	static thread_local vector<MappingSet *> contextRepresentatives;
	contextRepresentatives.clear();
	for (int i = 0; i < reactantLists[1]->size(); ++i) {
		MappingSet *ms = reactantLists[1]->getMappingSetByIndex(i);
		if (ms != 0) contextRepresentatives.push_back(ms);
	}

	double validPropensity = 0.0;
	for (int i = 0; i < reactantTree->size(); ++i) {
		msPairBuffer[0] = reactantTree->getMappingSetByIndex(i);
		for (size_t j = 0; j < contextRepresentatives.size(); ++j) {
			msPairBuffer[1] = contextRepresentatives[j];
			if (transformationSet->checkMolecularity(msPairBuffer)) {
				validPropensity += baseRate * reactantTree->getRateFactor(i);
			}
		}
	}

	return validPropensity;
}

void EnergyRxnClass::pickRuleMonkeyMappingSets(double random_A_number) const
{
	if (n_reactants != 2 || DORreactantIndex != 0) {
		DORRxnClass::pickRuleMonkeyMappingSets(random_A_number);
		return;
	}

	static thread_local vector<MappingSet *> contextRepresentatives;
	contextRepresentatives.clear();
	for (int i = 0; i < reactantLists[1]->size(); ++i) {
		MappingSet *ms = reactantLists[1]->getMappingSetByIndex(i);
		if (ms != 0) contextRepresentatives.push_back(ms);
	}
	if (contextRepresentatives.empty() || reactantTree->size() == 0)
		return;

	/* Enumerate valid pairs in RuleMonkey mode.  The normal compact selector
	 * can use the product of the tree sum and context count, but RuleMonkey
	 * promises to remove null molecularity events exactly.  Keeping this
	 * path explicit also handles models that disallow binding within one
	 * existing complex. */
	validPairsBuffer.clear();
	validWeightsBuffer.clear();
	double totalWeight = 0.0;
	for (int i = 0; i < reactantTree->size(); ++i) {
		msPairBuffer[0] = reactantTree->getMappingSetByIndex(i);
		for (size_t j = 0; j < contextRepresentatives.size(); ++j) {
			msPairBuffer[1] = contextRepresentatives[j];
			if (!transformationSet->checkMolecularity(msPairBuffer)) continue;

			validPairsBuffer.push_back(make_pair(i, (int)j));
			double weight = reactantTree->getRateFactor(i);
			validWeightsBuffer.push_back(weight);
			totalWeight += weight;
		}
	}

	if (validPairsBuffer.empty() || totalWeight <= 0.0) return;

	double randNum = system->getRNG().random(totalWeight);
	double cumulative = 0.0;
	size_t selectedIndex = validPairsBuffer.size() - 1;
	for (size_t k = 0; k < validPairsBuffer.size(); ++k) {
		cumulative += validWeightsBuffer[k];
		if (randNum <= cumulative) {
			selectedIndex = k;
			break;
		}
	}

	const pair<int, int> selected = validPairsBuffer[selectedIndex];
	mappingSet[0] = reactantTree->getMappingSetByIndex(selected.first);
	mappingSet[1] = contextRepresentatives[selected.second];
}


/*
 * DOR2RxnClass
 */

DOR2RxnClass::DOR2RxnClass(
		string name,
		double baseRate,
		string baseRateName,
		TransformationSet *transformationSet,
		CompositeFunction *function1,
		CompositeFunction *function2,
		vector <string> &lfArgumentPointerNameList1,
		vector <string> &lfArgumentPointerNameList2,
		System *s
	) : ReactionClass(name,baseRate,baseRateName,transformationSet,s)
{
	//////////////////////////////////////////////////////////////////////////////////////////
	//Step 1: Find the DOR reactants, and make sure there are exactly 2.  DOR reactants
	//can be found because they have a LocalFunctionPointer Transformation that keeps
	//information about the pointer onto either a reactant species or a particular molecule
	//in the pattern.
	DORreactantIndex1 = -1;
	DORreactantIndex2 = -1;
	for (int r=0; (unsigned)r<n_reactants; r++) {

		for (int i=0; i < transformationSet->getNumOfTransformations(r); i++) {

			Transformation *transform = transformationSet->getTransformation(r,i);
			if((unsigned)transform->getType()==TransformationFactory::LOCAL_FUNCTION_REFERENCE) {

				if (DORreactantIndex1 ==-1){
					if ( transformationSet->getTemplateMolecule(r)->getMoleculeType()->isPopulationType() )
					{   // DOR reactant is a population!
						cout<<"Error when creating DOR2RxnClass: "<<name<<endl;
						cout<<"DOR reactant1 cannot be a population type."<<endl;
						exit(1);
					}

					DORreactantIndex1 = r;
				}
				else if (DORreactantIndex1 == r) {
					// this is ok
				}
				else if (DORreactantIndex2 ==-1) {
					if ( transformationSet->getTemplateMolecule(r)->getMoleculeType()->isPopulationType() ) {
						// DOR reactant is a population!
						cout<<"Error when creating DOR2RxnClass: "<<name<<endl;
						cout<<"DOR reactant2 cannot be a population type."<<endl;
						exit(1);
					}

					DORreactantIndex2 = r;
				}
				else if (DORreactantIndex2 == r) {
					// this is ok
				}
				else {
					cout<<"Error when creating DOR2RxnClass: "<<name<<endl;
					cout<<"DOR2 reactions only support two DOR reactants."<<endl;
					exit(1);
				}
			}
		}
	}

	if (DORreactantIndex1==-1 || DORreactantIndex2==-1) {
		cout<<"Error when creating DOR2RxnClass: "<<name<<endl;
		cout<<"DOR2RxnClass requires pointers into two different reactant patterns, but fewer than 2 were found!"<<endl;
		exit(1);
	}



	//////////////////////////////////////////////////////////////////////////////////////////
	//Step 2: Some bookkeeping so that we can quickly get the function values from a mapping set
	// Now that we have found the DOR reactant, which can potentially have multiple functions, lets
	// figure out which functions apply to which

	// DOR reactant1
	//Array to double check that we have used all pointer references we have created
	bool *hasMatched1 = new bool [transformationSet->getNumOfTransformations(DORreactantIndex1)];
	for (int i=0; i<transformationSet->getNumOfTransformations(DORreactantIndex1); i++) hasMatched1[i]=false;

	//make sure that we have the right number of functions and argument names
	if((unsigned)function1->getNumOfArgs()!=lfArgumentPointerNameList1.size()) {
		cout<<"Error when creating DOR2RxnClass: "<<name<<endl;
		cout<<"Number of arguments in function1 and LocalFunctionArgumentPointerList1 size do not match!"<<endl;
		exit(1);
	}

	n_argMolecules1=lfArgumentPointerNameList1.size();
	argIndexIntoMappingSet1 =  new int [n_argMolecules1];
	argMappedMolecule1 = new Molecule *[n_argMolecules1];
	argScope1 = new int [n_argMolecules1];

	for(int i=0; i<(int)lfArgumentPointerNameList1.size(); i++) {
		//Now search for the function argument...
		bool match = false;
		for(int k=0; k<transformationSet->getNumOfTransformations(DORreactantIndex1); k++) {
			Transformation *transform = transformationSet->getTransformation(DORreactantIndex1,k);
			if((unsigned)transform->getType()==TransformationFactory::LOCAL_FUNCTION_REFERENCE) {
				LocalFunctionReference *lfr = static_cast<LocalFunctionReference*>(transform);
				if(lfr->getPointerName()==lfArgumentPointerNameList1.at(i)) {
					//If we got here, we found a match, so remember the index of the transformation
					//so we can quickly get the value of the function for any mapping object we try
					//to push on the reactant Tree.

					argIndexIntoMappingSet1[i] =  k;
					argMappedMolecule1[i] = 0;
					argScope1[i] = lfr->getFunctionScope();

					hasMatched1[k]=true;
					match=true;
				}
			}
		}
		if(!match){  //If there was no match found, then we've got issues...
			cout<<"Error when creating DOR2 reaction: "<<name<<endl;
			cout<<"Could not find a match in the templateMolecules for a pointer reference to species/molecule: ";
			cout<<lfArgumentPointerNameList1.at(i)<<endl;
			exit(1);
		}
	}

	//Just send out a warning if we didn't use one of the pointer references we were given
	for(int k=0; k<transformationSet->getNumOfTransformations(DORreactantIndex1); k++) {
		Transformation *transform = transformationSet->getTransformation(DORreactantIndex1,k);
		if((unsigned)transform->getType()==TransformationFactory::LOCAL_FUNCTION_REFERENCE) {
			if(!hasMatched1[k]) {
				cout<<endl<<"Warning!  when creating DOR2RxnClass: "<<name<<endl;
				cout<<"Pointer reference: "<<  static_cast<LocalFunctionReference*>(transform)->getPointerName();
				cout<<" that was provided is not used in the local function definition."<<endl;
			}
		}
	}
    // done with DOR reactant 1
	delete [] hasMatched1;


	// DOR reactant2
	//Array to double check that we have used all pointer references we have created
	bool *hasMatched2 = new bool [transformationSet->getNumOfTransformations(DORreactantIndex2)];
	for (int i=0; i<transformationSet->getNumOfTransformations(DORreactantIndex1); i++) hasMatched2[i]=false;

	//make sure that we have the right number of functions and argument names
	if((unsigned)function2->getNumOfArgs()!=lfArgumentPointerNameList2.size()) {
		cout<<"Error when creating DOR2RxnClass: "<<name<<endl;
		cout<<"Number of arguments in function2 and LocalFunctionArgumentPointerList2 size do not match!"<<endl;
		exit(1);
	}

	n_argMolecules2=lfArgumentPointerNameList2.size();
	argIndexIntoMappingSet2 =  new int [n_argMolecules2];
	argMappedMolecule2 = new Molecule *[n_argMolecules2];
	argScope2 = new int [n_argMolecules2];

	for(int i=0; i<(int)lfArgumentPointerNameList2.size(); i++) {
		//Now search for the function argument...
		bool match = false;
		for(int k=0; k<transformationSet->getNumOfTransformations(DORreactantIndex2); k++) {
			Transformation *transform = transformationSet->getTransformation(DORreactantIndex2,k);
			if((unsigned)transform->getType()==TransformationFactory::LOCAL_FUNCTION_REFERENCE) {
				LocalFunctionReference *lfr = static_cast<LocalFunctionReference*>(transform);
				if(lfr->getPointerName()==lfArgumentPointerNameList2.at(i)) {
					//If we got here, we found a match, so remember the index of the transformation
					//so we can quickly get the value of the function for any mapping object we try
					//to push on the reactant Tree.

					argIndexIntoMappingSet2[i] =  k;
					argMappedMolecule2[i] = 0;
					argScope2[i] = lfr->getFunctionScope();

					hasMatched2[k]=true;
					match=true;
				}
			}
		}
		if(!match){  //If there was no match found, then we've got issues...
			cout<<"Error when creating DOR2 reaction: "<<name<<endl;
			cout<<"Could not find a match in the templateMolecules for a pointer reference to species/molecule: ";
			cout<<lfArgumentPointerNameList2.at(i)<<endl;
			exit(1);
		}
	}



	//Just send out a warning if we didn't use one of the pointer references we were given
	for(int k=0; k<transformationSet->getNumOfTransformations(DORreactantIndex2); k++) {
		Transformation *transform = transformationSet->getTransformation(DORreactantIndex2,k);
		if((unsigned)transform->getType()==TransformationFactory::LOCAL_FUNCTION_REFERENCE) {
			if(!hasMatched2[k]) {
				cout<<endl<<"Warning!  when creating DOR2RxnClass: "<<name<<endl;
				cout<<"Pointer reference: "<<  static_cast<LocalFunctionReference*>(transform)->getPointerName();
				cout<<" that was provided is not used in the local function definition."<<endl;
			}
		}
	}
    // done with DOR reactant 2
	delete [] hasMatched2;


	//////////////////////////////////////////////////////////////////////////////////////////
	///  Step 3: Wheh! now we can finally get on the business of creating the reactant lists
	///  and the reactant tree and setting the usual reactionClass parameters

	//Remember that we are a DOR ReactionClass
	this->reactionType = ReactionClass::DOR2_RXN;

	//Set up the reactant trees
	reactantTree1 = new ReactantTree(this->DORreactantIndex1,transformationSet,32,this->system);
	reactantTree2 = new ReactantTree(this->DORreactantIndex2,transformationSet,32,this->system);
	msPairBuffer = new MappingSet*[2];

	//Set up the reactantLists
	reactantLists = new ReactantList *[n_reactants];
	for (unsigned int r=0; r<n_reactants; r++) {
		if( (signed)r!=this->DORreactantIndex1  &&  (signed)r!=this->DORreactantIndex2 )
			reactantLists[r]=(new ReactantList(r,transformationSet,25,this->system));
	}

	//Initialize a to zero
	this->a=0;

	//Set the actual functions
	this->cf1 = function1;
	this->cf2 = function2;

	//Add type I molecule dependencies, so that when this function
	//is reevaluated on a molecule, the molecule knows to update this reaction.
	//This is only necessary for the DOR reactants.
	cf1->addTypeIMoleculeDependency( reactantTemplates[DORreactantIndex1]->getMoleculeType() );
	cf2->addTypeIMoleculeDependency( reactantTemplates[DORreactantIndex2]->getMoleculeType() );

}


DOR2RxnClass::~DOR2RxnClass() {

	for(unsigned int r=0; r<n_reactants; r++) {
		if( r != DORreactantIndex1  && 	r != DORreactantIndex2 )
			delete reactantLists[r];
	}

	delete [] reactantLists;

	delete reactantTree1;
	delete reactantTree2;

	delete [] argIndexIntoMappingSet1;
	delete [] argIndexIntoMappingSet2;
	delete [] argMappedMolecule1;
	delete [] argMappedMolecule2;
	delete [] argScope1;
	delete [] argScope2;
	delete [] msPairBuffer;
}


void DOR2RxnClass::init() {

	//Here we have to tell the molecules that they are part of this function
	//and for single molecule functions, we have to tell them also that they are in
	//this function, so they need to update thier value should they be transformed
	for(unsigned int r=0; r<n_reactants; r++)
	{
		reactantTemplates[r]->getMoleculeType()->addReactionClass(this,r);
	}
}


void DOR2RxnClass::remove(Molecule *m, unsigned int reactantPos)
{
	// removing molecule from a DOR!!
	if(reactantPos==(unsigned)this->DORreactantIndex1){
		// handle the DOR reactant1
		int rxnIndex = m->getMoleculeType()->getRxnIndex(this,reactantPos);
		if(m->getRxnListMappingId(rxnIndex)>=0) {
			reactantTree1->removeMappingSet(m->getRxnListMappingId(rxnIndex));
			m->setRxnListMappingId(rxnIndex,Molecule::NOT_IN_RXN);
		}
	}
	else if (reactantPos==(unsigned)this->DORreactantIndex2){
		// handle the DOR reactant2
		int rxnIndex = m->getMoleculeType()->getRxnIndex(this,reactantPos);
		if(m->getRxnListMappingId(rxnIndex)>=0) {
			reactantTree2->removeMappingSet(m->getRxnListMappingId(rxnIndex));
			m->setRxnListMappingId(rxnIndex,Molecule::NOT_IN_RXN);
		}
	}
	else {
		// handle it normally...
		ReactantList *rl = reactantLists[reactantPos];
		int rxnIndex = m->getMoleculeType()->getRxnIndex(this,reactantPos);
		if(m->getRxnListMappingId(rxnIndex)>=0) {
			rl->removeMappingSet(m->getRxnListMappingId(rxnIndex));
			m->setRxnListMappingId(rxnIndex,Molecule::NOT_IN_RXN);
		}
	}
}


bool DOR2RxnClass::tryToAdd(Molecule *m, unsigned int reactantPos) {
	if (system != 0 && system->isProfilingEnabled())
		system->recordProfileMatchCandidate();

	// adding molecule to DOR2RxnClass
	if (reactantPos==(unsigned)this->DORreactantIndex1) {

		// handle the DOR reactant
		int rxnIndex = m->getMoleculeType()->getRxnIndex(this,reactantPos);

		if(reactantTree1->getHasClonedMappings()) {
			if(m->getRxnListMappingId(rxnIndex)>=0) {
				reactantTree1->removeMappingSet(m->getRxnListMappingId(rxnIndex));
				m->setRxnListMappingId(rxnIndex,Molecule::NOT_IN_RXN);
			}
		}

		if(m->getRxnListMappingId(rxnIndex)>=0) {
			// was in the tree, so checking if we should remove
			if(!reactantTemplates[reactantPos]->compare(m)) {
				// removing
				reactantTree1->removeMappingSet(m->getRxnListMappingId(rxnIndex));
				m->setRxnListMappingId(rxnIndex,Molecule::NOT_IN_RXN);
			} else {}
		} else {
			// wasn't in the tree, so trying to push and compare
			MappingSet *ms = reactantTree1->pushNextAvailableMappingSet();
			comparisonResult = reactantTemplates[reactantPos]->compare(m,reactantTree1,ms);
			if(!comparisonResult) {
			//if(!reactantTemplates[reactantPos]->compare(m,reactantTree1,ms)) {
				reactantTree1->removeMappingSet(ms->getId());
			} else {
				//we are keeping it, so evaluate the function and confirm the push
				double localFunctionValue = evaluateLocalFunctions1(ms);
				reactantTree1->confirmPush(ms->getId(),localFunctionValue);
				m->setRxnListMappingId(rxnIndex,ms->getId());
			}
		}
	}
	else if (reactantPos==(unsigned)this->DORreactantIndex2) {

		// handle the DOR reactant
		int rxnIndex = m->getMoleculeType()->getRxnIndex(this,reactantPos);

		if(reactantTree2->getHasClonedMappings()) {
			if(m->getRxnListMappingId(rxnIndex)>=0) {
				reactantTree2->removeMappingSet(m->getRxnListMappingId(rxnIndex));
				m->setRxnListMappingId(rxnIndex,Molecule::NOT_IN_RXN);
			}
		}

		if(m->getRxnListMappingId(rxnIndex)>=0) {
			// was in the tree, so checking if we should remove
			if(!reactantTemplates[reactantPos]->compare(m)) {
				// removing
				reactantTree2->removeMappingSet(m->getRxnListMappingId(rxnIndex));
				m->setRxnListMappingId(rxnIndex,Molecule::NOT_IN_RXN);
			} else {}
		} else {
			// wasn't in the tree, so trying to push and compare
			MappingSet *ms = reactantTree2->pushNextAvailableMappingSet();
			comparisonResult = reactantTemplates[reactantPos]->compare(m,reactantTree2,ms);
			if(!comparisonResult){
			//if(!reactantTemplates[reactantPos]->compare(m,reactantTree2,ms)) {
				reactantTree2->removeMappingSet(ms->getId());
			} else {
				//we are keeping it, so evaluate the function and confirm the push
				double localFunctionValue = this->evaluateLocalFunctions2(ms);
				reactantTree2->confirmPush(ms->getId(),localFunctionValue);
				m->setRxnListMappingId(rxnIndex,ms->getId());
			}
		}
	}
	else {
		//Get the specified reactantList
		ReactantList *rl = reactantLists[reactantPos];
		int rxnIndex = m->getMoleculeType()->getRxnIndex(this,reactantPos);

		if(rl->getHasClonedMappings()) {
			if(m->getRxnListMappingId(rxnIndex)>=0) {
				rl->removeMappingSet(m->getRxnListMappingId(rxnIndex));
				m->setRxnListMappingId(rxnIndex,Molecule::NOT_IN_RXN);
			}
		}

		//Here we get the standard update...
		if(m->getRxnListMappingId(rxnIndex)>=0) //If we are in this reaction...
		{
			if(!reactantTemplates[reactantPos]->compare(m)) {
				rl->removeMappingSet(m->getRxnListMappingId(rxnIndex));
				m->setRxnListMappingId(rxnIndex,Molecule::NOT_IN_RXN);
			}

		} else {
			//Try to map it!
			MappingSet *ms = rl->pushNextAvailableMappingSet();
			comparisonResult = reactantTemplates[reactantPos]->compare(m,rl,ms);
			if(!comparisonResult) {
				//we must remove, if we did not match.  This will also remove
				//everything that was cloned off of the mapping set
				rl->removeMappingSet(ms->getId());
			} else {
				m->setRxnListMappingId(rxnIndex,ms->getId());
			}
		}
	}
	return true;
}


int DOR2RxnClass::getReactantCount(unsigned int reactantIndex) const
{
	if (reactantIndex==(unsigned)this->DORreactantIndex1) {
		return reactantTree1->size();
	}
	if (reactantIndex==(unsigned)this->DORreactantIndex2) {
		return reactantTree2->size();
	}
	return isPopulationType[reactantIndex] ?
		       reactantLists[reactantIndex]->getPopulation()
	         : reactantLists[reactantIndex]->size();
}


int DOR2RxnClass::getCorrectedReactantCount(unsigned int reactantIndex) const
{
	if (reactantIndex==(unsigned)DORreactantIndex1) {
		return contextCountsPerComplex[reactantIndex]
		         ? countDistinctComplexes(reactantTree1) : reactantTree1->size();
	}
	else if (reactantIndex==(unsigned)DORreactantIndex2) {
		return contextCountsPerComplex[reactantIndex]
		         ? countDistinctComplexes(reactantTree2) : reactantTree2->size();
	}

	// MatchOnce is the user's explicit request; contextCountsPerComplex is the
	// same counting applied automatically to a reactant the rule never transforms,
	// which is how BNG counts one.
	if ((matchOncePerReactant[reactantIndex] || contextCountsPerComplex[reactantIndex])
	    && !isPopulationType[reactantIndex]) {
		return countDistinctComplexes(reactantLists[reactantIndex]);
	}

	return isPopulationType[reactantIndex] ?
			   std::max( reactantLists[reactantIndex]->getPopulation()
			             - identicalPopCountCorrection[reactantIndex], 0 )
			 : reactantLists[reactantIndex]->size();
}



//This function takes a given mappingset and looks up the value of its local
//functions based on the local functions that were defined
double DOR2RxnClass::evaluateLocalFunctions1(MappingSet *ms)
{
	//Go through each function, and set the value of the function

	//Grab the molecules needed for the local function to evaluate
	for(int i=0; i < n_argMolecules1; i++) {
		argMappedMolecule1[i] = ms->get(argIndexIntoMappingSet1[i])->getMolecule();
	}

	// done setting molecules, so now calling the composite function evaluate method
	int * reactantCounts = new int[n_reactants];
	for(unsigned int r=0; r<n_reactants; r++) {
		if(r==(unsigned int)DORreactantIndex1) {
			reactantCounts[r] = reactantTree1->size();
		}
		else if(r==(unsigned int)DORreactantIndex2) {
			reactantCounts[r] = reactantTree2->size();
		}
		else {
			reactantCounts[r] = reactantLists[r]->size();
		}
	}

	double value = cf1->evaluateOn(argMappedMolecule1, argScope1, reactantCounts, n_reactants);

	delete [] reactantCounts;
	return value;
}


//This function takes a given mappingset and looks up the value of its local
//functions based on the local functions that were defined
double DOR2RxnClass::evaluateLocalFunctions2(MappingSet *ms)
{
	//Go through each function, and set the value of the function

	//Grab the molecules needed for the local function to evaluate
	for (int i=0; i < n_argMolecules2; i++) {
		argMappedMolecule2[i] = ms->get(argIndexIntoMappingSet2[i])->getMolecule();
	}

	// done setting molecules, so now calling the composite function evaluate method
	int * reactantCounts = new int[n_reactants];
	for(unsigned int r=0; r<n_reactants; r++) {
		if(r==DORreactantIndex1) {
			reactantCounts[r] = reactantTree1->size();
		}
		else if(r==this->DORreactantIndex2) {
			reactantCounts[r] = reactantTree2->size();
		}
		else {
			reactantCounts[r] = reactantLists[r]->size();
		}
	}

	double value = cf2->evaluateOn(argMappedMolecule2, argScope2, reactantCounts, n_reactants);

	delete [] reactantCounts;
	return value;
}


double DOR2RxnClass::update_a() {
	if (useRuleMonkey) {
		a = exactRuleMonkey_a();
		return a;
	}
	a = baseRate;
	for (unsigned int i=0; i<n_reactants; i++) {
		// as in DORRxnClass::update_a(), a DOR reactant's propensity comes from its
		// rate factor sum, so that is where per-complex counting goes
		if (i==(unsigned int)DORreactantIndex1) {
			a *= contextCountsPerComplex[i]
			       ? perComplexRateFactorSum(reactantTree1)
			       : reactantTree1->getRateFactorSum();
		}
		else if (i==(unsigned int)DORreactantIndex2) {
			a *= contextCountsPerComplex[i]
			       ? perComplexRateFactorSum(reactantTree2)
			       : reactantTree2->getRateFactorSum();
		}
		else {
			a*=(double)getCorrectedReactantCount(i);
		}
	}
	return a;
}


double DOR2RxnClass::exactRuleMonkey_a()
{
	if(this->totalRateFlag) {
		double exact_a = baseRate;
		for(unsigned int i=0; i<n_reactants; i++) {
			if(getCorrectedReactantCount(i)==0) exact_a = 0.0;
		}
		return exact_a;
	}

	double validCombinations = 0.0;
	if (n_reactants == 0) {
		validCombinations = 1.0;
	} else if (n_reactants == 1) {
		// As in DORRxnClass, a DOR reactant's propensity comes from its tree's
		// rate factor sum, so that is where per-complex counting is applied.
		if (0 == DORreactantIndex1) {
			validCombinations = contextCountsPerComplex[0]
			                      ? perComplexRateFactorSum(reactantTree1)
			                      : reactantTree1->getRateFactorSum();
		} else if (0 == DORreactantIndex2) {
			validCombinations = contextCountsPerComplex[0]
			                      ? perComplexRateFactorSum(reactantTree2)
			                      : reactantTree2->getRateFactorSum();
		} else {
			validCombinations = getCorrectedReactantCount(0);
		}
	} else if (n_reactants == 2) {
		// One representative per complex for a pure context reactant, as in
		// BasicRxnClass::exactRuleMonkey_a().
		static thread_local std::vector<MappingSet*> reps0, reps1;
		static thread_local std::vector<int> idx0, idx1;
		collectReactantRepresentatives(reactantLists[0], contextCountsPerComplex[0], reps0, &idx0);
		collectReactantRepresentatives(reactantLists[1], contextCountsPerComplex[1], reps1, &idx1);

		double totalCombinations = 1.0;
		for (unsigned int i=0; i<n_reactants; i++) {
			if (i==(unsigned int)DORreactantIndex1) {
				totalCombinations *= contextCountsPerComplex[i]
				                       ? perComplexRateFactorSum(reactantTree1)
				                       : reactantTree1->getRateFactorSum();
			} else if (i==(unsigned int)DORreactantIndex2) {
				totalCombinations *= contextCountsPerComplex[i]
				                       ? perComplexRateFactorSum(reactantTree2)
				                       : reactantTree2->getRateFactorSum();
			} else {
				totalCombinations*=(double)getCorrectedReactantCount(i);
			}
		}

		double invalidCombinations = 0;

		for (size_t i = 0; i < reps0.size(); ++i) {
			msPairBuffer[0] = reps0[i];
			for (size_t j = 0; j < reps1.size(); ++j) {
				msPairBuffer[1] = reps1[j];

				if (!transformationSet->checkMolecularity(msPairBuffer)) {
					double weight0 = (0 == DORreactantIndex1) ? reactantTree1->getRateFactor(idx0[i]) :
					                 ((0 == DORreactantIndex2) ? reactantTree2->getRateFactor(idx0[i]) : 1.0);
					double weight1 = (1 == DORreactantIndex1) ? reactantTree1->getRateFactor(idx1[j]) :
					                 ((1 == DORreactantIndex2) ? reactantTree2->getRateFactor(idx1[j]) : 1.0);
					invalidCombinations += (weight0 * weight1);
				}
			}
		}
		validCombinations = totalCombinations - invalidCombinations;
		if (validCombinations < 0) validCombinations = 0;
	} else {
		validCombinations = 1.0;
		for (unsigned int i=0; i<n_reactants; i++) {
			if (i==(unsigned int)DORreactantIndex1) {
				validCombinations *= contextCountsPerComplex[i]
				                       ? perComplexRateFactorSum(reactantTree1)
				                       : reactantTree1->getRateFactorSum();
			} else if (i==(unsigned int)DORreactantIndex2) {
				validCombinations *= contextCountsPerComplex[i]
				                       ? perComplexRateFactorSum(reactantTree2)
				                       : reactantTree2->getRateFactorSum();
			} else {
				validCombinations*=(double)getCorrectedReactantCount(i);
			}
		}
	}

	return validCombinations * baseRate;
}


void DOR2RxnClass::pickRuleMonkeyMappingSets(double random_A_number) const
{
	if (n_reactants != 2 || totalRateFlag) {
		for(unsigned int i=0; i<n_reactants; i++) {
			if( i!=(unsigned)DORreactantIndex1 && i!=(unsigned)DORreactantIndex2) {
				if ( isPopulationType[i] ) {
					reactantLists[i]->pickRandomFromPopulation(mappingSet[i]);
				} else {
					reactantLists[i]->pickRandom(mappingSet[i]);
				}
			}
		}

		double randNumber1 = system->getRNG().random( reactantTree1->getRateFactorSum() );
		reactantTree1->pickReactantFromValue( mappingSet[DORreactantIndex1], randNumber1, 1.0);

		double randNumber2 = system->getRNG().random( reactantTree2->getRateFactorSum() );
		reactantTree2->pickReactantFromValue( mappingSet[DORreactantIndex2], randNumber2, 1.0);
		return;
	}

	int size0 = getReactantCount(0);
	int size1 = getReactantCount(1);
	
	validPairsBuffer.clear();
	validWeightsBuffer.clear();
	double totalWeight = 0.0;
	
	for (int i = 0; i < size0; ++i) {
		msPairBuffer[0] = reactantLists[0]->getMappingSet(i);
		for (int j = 0; j < size1; ++j) {
			msPairBuffer[1] = reactantLists[1]->getMappingSet(j);
			
			if (transformationSet->checkMolecularity(msPairBuffer)) {
				validPairsBuffer.push_back(make_pair(i, j));
				double weight0 = (0 == DORreactantIndex1) ? reactantTree1->getRateFactor(i) :
				                 ((0 == DORreactantIndex2) ? reactantTree2->getRateFactor(i) : 1.0);
				double weight1 = (1 == DORreactantIndex1) ? reactantTree1->getRateFactor(j) :
				                 ((1 == DORreactantIndex2) ? reactantTree2->getRateFactor(j) : 1.0);
				
				double weight = weight0 * weight1;
				validWeightsBuffer.push_back(weight);
				totalWeight += weight;
			}
		}
	}
	
	if (validPairsBuffer.empty() || totalWeight <= 0) {
		// Safety fallback: this should be unreachable when exactRuleMonkey_a() > 0.
		// If reached, preserve legacy behavior by falling back to the standard selector.
		for(unsigned int i=0; i<n_reactants; i++) {
			if( i!=(unsigned)DORreactantIndex1 && i!=(unsigned)DORreactantIndex2) {
				if ( isPopulationType[i] ) {
					reactantLists[i]->pickRandomFromPopulation(mappingSet[i]);
				} else {
					reactantLists[i]->pickRandom(mappingSet[i]);
				}
			}
		}

		double randNumber1 = system->getRNG().random( reactantTree1->getRateFactorSum() );
		reactantTree1->pickReactantFromValue( mappingSet[DORreactantIndex1], randNumber1, 1.0);

		double randNumber2 = system->getRNG().random( reactantTree2->getRateFactorSum() );
		reactantTree2->pickReactantFromValue( mappingSet[DORreactantIndex2], randNumber2, 1.0);
		return;
	}
	
	double randNum = system->getRNG().random(totalWeight);
	double cumulative = 0;
	int selectedIndex = validPairsBuffer.size() - 1;
	for (size_t k = 0; k < validPairsBuffer.size(); ++k) {
		cumulative += validWeightsBuffer[k];
		if (randNum <= cumulative) {
			selectedIndex = k;
			break;
		}
	}
	
	int i = validPairsBuffer[selectedIndex].first;
	int j = validPairsBuffer[selectedIndex].second;
	
	mappingSet[0] = reactantLists[0]->getMappingSet(i);
	mappingSet[1] = reactantLists[1]->getMappingSet(j);
}

void DOR2RxnClass::pickMappingSets(double randNumber) const
{
	if (useRuleMonkey) {
		pickRuleMonkeyMappingSets(randNumber);
		return;
	}
	//here we cannot just select a random molecule.  This is where all of our hard
	//(as well as all the other reactants.  So here we go...
	//double rateFactorMultiplier = baseRate;
	for(unsigned int i=0; i<n_reactants; i++) {
		if( i!=(unsigned)DORreactantIndex1 && i!=(unsigned)DORreactantIndex2) {
			if ( isPopulationType[i] ) {
				reactantLists[i]->pickRandomFromPopulation(mappingSet[i]);
			} else {
				reactantLists[i]->pickRandom(mappingSet[i]);
			}
			//rateFactorMultiplier*=getReactantCount(i);
		}
	}

	double randNumber1 = system->getRNG().random( reactantTree1->getRateFactorSum() );
	reactantTree1->pickReactantFromValue( mappingSet[DORreactantIndex1], randNumber1, 1.0);

	double randNumber2 = system->getRNG().random( reactantTree2->getRateFactorSum() );
	reactantTree2->pickReactantFromValue( mappingSet[DORreactantIndex2], randNumber2, 1.0);

}


void DOR2RxnClass::notifyRateFactorChange(Molecule * m, int reactantIndex, int rxnListIndex) {
	if (reactantIndex==DORreactantIndex1) {
		double newValue = evaluateLocalFunctions1(reactantTree1->getMappingSet(rxnListIndex));
		reactantTree1->updateValue(rxnListIndex,newValue);
	}
	else if (reactantIndex==DORreactantIndex2) {
		double newValue = evaluateLocalFunctions2(reactantTree2->getMappingSet(rxnListIndex));
		reactantTree2->updateValue(rxnListIndex,newValue);
	}
	else {
		cout<<"Internal Error in DORRxnClass::notifyRateFactorChange!!  : trying to change a rate\n";
		cout<<"factor of a non-DOR reactant.  That means this function was called in error!\n";
		exit(1);
	}
}


void DOR2RxnClass::printDetails() const
{
	cout<<"DOR2RxnClass: " << name <<"  ( baseRate="<<baseRate<<",  a="<<a<<", fired="<<fireCounter<<" times )"<<endl;
	for(unsigned int r=0; r<n_reactants; r++)
	{
		if( r==(unsigned)DORreactantIndex1) {
			cout<<"      -(DOR1) |"<< getReactantCount(r)<<" mappings|\t";
			cout<<reactantTemplates[r]->getPatternString()<<"\n";
			cout<<"             (rateFactorSum="<<reactantTree1->getRateFactorSum();
			cout<<")."<<endl;
		}
		else if( r==(unsigned)DORreactantIndex2) {
			cout<<"      -(DOR2) |"<< getReactantCount(r)<<" mappings|\t";
			cout<<reactantTemplates[r]->getPatternString()<<"\n";
			cout<<"             (rateFactorSum="<<reactantTree2->getRateFactorSum();
			cout<<")."<<endl;
		} else {
			cout<<"      -|"<< getReactantCount(r)<<" mappings|\t";
			cout<<reactantTemplates[r]->getPatternString()<<"\n";

		}
	}

	if (n_reactants==0)
		cout << "      >No Reactants: so this rule either creates new species or does nothing."<<endl;
}
