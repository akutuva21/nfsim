/*
 * compositeFunction.cpp
 *
 *  Created on: Dec 4, 2008
 *      Author: msneddon
 */

#include "NFfunction.hh"
#include <algorithm>
#include <cctype>
#include <stdexcept>


using namespace std;
using namespace NFcore;
using namespace mu;


CompositeFunction::CompositeFunction(System *s,
					string name,
					string expression,
					vector <string> &functions,
					vector <string> &argNames,
					vector <string> &paramNames)
{

	this->name = name;
	this->originalExpression=expression;
	this->parsedExpression="";


	this->n_params=paramNames.size();
	this->paramNames = new string[n_params];
	for(unsigned int i=0; i<n_params; i++) {
		this->paramNames[i]=paramNames.at(i);
	}

	this->n_args=argNames.size();
	this->argNames=new string[n_args];
	for(unsigned int i=0; i<n_args; i++) {
		this->argNames[i]=argNames.at(i);
	}


	this->n_allFuncs=functions.size();
	allFuncNames = new string[n_allFuncs];
	for(unsigned int i=0; i<n_allFuncs; i++) {
		this->allFuncNames[i] = functions.at(i);
	}

	p=0;
	this->sysPtr = NULL;

	// AS-2021
	this->fileFunc = false;
	this->interpolationMethod = "linear";
	this->currInd = 0;
	this->dataLen = 0;
	this->counter = NULL;
	this->funcPtr = NULL;
	this->ctrType = "";
	this->ctrName = "";
	this->counterParamName = "";
	this->identityLocalFunction = false;
	this->simpleStateSelector = false;
	this->simpleStateSelectorFunction = 0;
	this->simpleStateIntactParamIndex = -1;
	this->simpleStateEndocleavedParamIndex = -1;
	this->simpleStateIntactRate = 0.0;
	this->simpleStateEndocleavedRate = 0.0;
	// AS-2021
}
bool CompositeFunction::isMembershipOnlyRate() const
{
	if (fileFunc || !ctrType.empty() || containsTimeExpression(originalExpression) ||
			n_lfs != 0 || n_refLfs != 0)
		return false;
	for (int i = 0; i < n_gfs; ++i)
		if (gfs[i] == 0 || !gfs[i]->isRuntimeInvariant())
			return false;
	/* Reactant-count references are allowed: they are exactly the quantities
	 * maintained by membership updates. Parameters are parser constants. */
	return true;
}

CompositeFunction::~CompositeFunction()
{
	delete [] allFuncNames;

	delete [] argNames;
	delete [] paramNames;

	delete [] gfNames;
	delete [] gfs;
	delete [] gfValues;

	delete [] lfNames;
	delete [] lfs;

	delete [] refLfInds;
	delete [] refLfRefNames;
	delete [] refLfScopes;
	delete [] refLfValues;

	delete [] reactantCount;

	if(p!=NULL) delete p;

}


void CompositeFunction::setGlobalObservableDependency(ReactionClass *r, System *s) {

	for(int i=0; i<this->n_gfs; i++) {
		GlobalFunction *gf=gfs[i];

		for(int vr=0; vr<gf->getNumOfVarRefs(); vr++) {
			if(gf->getVarRefType(vr)=="Observable" || gf->getVarRefType(vr)=="MoleculeObservable" || gf->getVarRefType(vr)=="SpeciesObservable") {
				Observable *obs = s->getObservableByName(gf->getVarRefName(vr));
				obs->addDependentRxn(r);
			} else {
				cerr<<"When creating a FunctionalRxnClass of name: "+r->getName()+" you provided a function that\n";
				cerr<<"depends on an observable type that I can't yet handle! (which is "+gf->getVarRefType(vr)+"\n";
				cerr<<"try using type: 'MoleculeObservable' for now.\n";
				cerr<<"quiting..."<<endl; exit(1);
			}
		}
	}
}

//call this immediately after you have read in all the functions, but before preparing for the simulation
//or reading in reactions
void CompositeFunction::finalizeInitialization(System *s)
{
	//first, find the global functions by name
	vector <GlobalFunction *> gf_tempVector;
	for(unsigned int i=0; i<n_allFuncs; i++) {
		GlobalFunction *gf=s->getGlobalFunctionByName(allFuncNames[i]);
		if(gf!=0) gf_tempVector.push_back(gf);
	}

	this->n_gfs = gf_tempVector.size();
	this->gfNames = new string[n_gfs];
	this->gfValues = new double[n_gfs];
	this->gfs = new GlobalFunction * [n_gfs];
	for(int i=0; i<n_gfs; i++) {
		gfNames[i] = gf_tempVector.at(i)->getName();
		gfValues[i] = 0;
		gfs[i] = gf_tempVector.at(i);
	}


	//now the local functions...
	vector <LocalFunction *> lf_tempVector;
	for(unsigned int i=0; i<n_allFuncs; i++) {
		LocalFunction *lf=s->getLocalFunctionByName(allFuncNames[i]);
		if(lf!=0) lf_tempVector.push_back(lf);
	}

	this->n_lfs = lf_tempVector.size();
	this->lfNames = new string[n_lfs];
	this->lfs = new LocalFunction * [n_lfs];
	for(int i=0; i<n_lfs; i++) {
		lfNames[i] = lf_tempVector.at(i)->getName();
		lfs[i] = lf_tempVector.at(i);
	}






	//Now we have to do the dirty work of parsing out the function expression
	//so that we can properly get at the variables and functions we need
	this->parsedExpression=originalExpression;

	//First
	for(int f=0; f<n_gfs; f++) {
		string::size_type sPos=parsedExpression.find(gfNames[f]);
		for( ; sPos!=string::npos; sPos=parsedExpression.find(gfNames[f],sPos+1)) {

			string::size_type openPar = parsedExpression.find_first_of('(',sPos);
			string::size_type closePar = parsedExpression.find_first_of(')',sPos);
			if(openPar!=string::npos && closePar!=string::npos) {
				if(closePar>openPar) { //if we got here, we found a valid parenthesis to look at
					string inBetween = parsedExpression.substr(openPar+1,closePar-openPar-1);
					NFutil::trim(inBetween);

					if(inBetween.size()==0) {
						parsedExpression.replace(openPar,closePar-openPar+1,"");
					}
				}
			}
		}
	}

//	cout<<"now the expression is: "<<parsedExpression<<endl;


	///////// do the same for local functions here (can be a bit tricky, because different
	///////// composite functions will have different numbers of arguments
	vector <int> lfIndexValues;
	vector <string> lfReferenceName;
	vector <int> lfScope;


	for(int f=0; f<n_lfs; f++) {
		string::size_type sPos=parsedExpression.find(lfs[f]->getName());
		for( ; sPos!=string::npos; sPos=parsedExpression.find(lfNames[f],sPos+1)) {

			string::size_type openPar = parsedExpression.find_first_of('(',sPos);
			string::size_type closePar = parsedExpression.find_first_of(')',sPos);
			if(openPar!=string::npos && closePar!=string::npos) {
				if(closePar>openPar) { //if we got here, we found a valid parenthesis to look at
					string possibleArg = parsedExpression.substr(openPar+1,closePar-openPar-1);
					NFutil::trim(possibleArg);

					for(unsigned int a=0; a<n_args; a++) {

						if(possibleArg==argNames[a]) {

							string identifier = "_"+argNames[a];
							parsedExpression.replace(openPar,closePar-openPar+1,identifier);

							bool found = false;
							for(unsigned int x=0; x<lfIndexValues.size(); x++) {
								if(lfReferenceName.at(x)==(lfNames[f]+identifier)){
									found=true;
								}
							}
							if(!found) {
								lfIndexValues.push_back(f);
								lfReferenceName.push_back(lfNames[f]+identifier);
								lfScope.push_back(a);
							}

							break; //break cause we're done with this scope...
						}
					}
				}
			}
		}
	}



	this->n_refLfs=lfIndexValues.size();
	this->refLfInds = new int[n_refLfs];
	this->refLfRefNames = new string[n_refLfs];
	this->refLfScopes = new int[n_refLfs];
	this->refLfValues = new double[n_refLfs];

	for(unsigned int i=0; i<lfIndexValues.size(); i++) {
		this->refLfInds[i]=lfIndexValues.at(i);
		this->refLfRefNames[i]=lfReferenceName.at(i);
		this->refLfScopes[i] = lfScope.at(i);
		this->refLfValues[i]=0;
	}



	// Parse out the ability to get reactant counts in composite reactions


	int maxReactantIndex = 0;
	string::size_type sPos=parsedExpression.find("reactant_");
	for( ; sPos!=string::npos; sPos=parsedExpression.find("reactant_",sPos+1)) {
		string::size_type openPar = parsedExpression.find_first_of('(',sPos);
		string::size_type closePar = parsedExpression.find_first_of(')',sPos);
		if(openPar!=string::npos && closePar!=string::npos) {
			if(closePar>openPar) { //if we got here, we found a valid parenthesis to look at
				string inBetween = parsedExpression.substr(openPar+1,closePar-openPar-1);
				NFutil::trim(inBetween);
				if(inBetween.size()==0) {
					parsedExpression.replace(openPar,closePar-openPar+1,"");
				}
			}
		}

		string numOneDigit = parsedExpression.substr(sPos+9,1);
		string numTwoDigits = parsedExpression.substr(sPos+9,2);
		NFutil::trim(numTwoDigits);



		int iOneDigit;
		try {
			iOneDigit = NFutil::convertToInt(numOneDigit);
		} catch (std::runtime_error e) {
			cerr<<"When referencing a reactant, you must include the reactant number"<<endl;
			cerr<<e.what()<<endl;
			exit(1);
		}

		bool isTwoDigitNumber = true;
		if(numTwoDigits.size()<2) {
			isTwoDigitNumber = false;
		} else {
			try {
				NFutil::convertToInt(numTwoDigits);
			} catch (std::runtime_error e) {
				isTwoDigitNumber = false;
			}
		}

		if(isTwoDigitNumber) {
			cerr<<"When referencing a reactant, you can only reference reactant numbers up to 9."<<endl;
			exit(1);
		}

		if(iOneDigit>maxReactantIndex) maxReactantIndex = iOneDigit;
	}




	this->n_reactantCounts = maxReactantIndex;
	reactantCount = new double[n_reactantCounts];
	for(int r=0; r<n_reactantCounts; r++) {
		reactantCount[r]=0;
	}

	/* A large class of generated DOR rate laws is only a type-safe
	 * CompositeFunction wrapper around one LocalFunction.  The parser still
	 * builds the wrapper so existing XML is accepted, but evaluating another
	 * muParser bytecode program for the identity adds avoidable work to every
	 * propensity lookup. */
	if (n_gfs == 0 && n_lfs == 1 && n_refLfs == 1 &&
			n_params == 0 && n_reactantCounts == 0 && !fileFunc) {
		string identityExpression = "_" +
				argNames[refLfScopes[0]];
		string normalizedExpression = parsedExpression;
		NFutil::trim(normalizedExpression);
		identityLocalFunction = normalizedExpression == identityExpression;
	}

	/* Recognize the compact state selector emitted by the indexed translation
	 * XML generator.  This remains deliberately narrow: arbitrary composite
	 * expressions continue through the parser, while this exact algebraic
	 * form can be evaluated without a second parser dispatch. */
	if (n_gfs == 0 && n_lfs == 1 && n_refLfs == 1 && n_params == 2 &&
			n_reactantCounts == 0 && !fileFunc && lfs[0] != 0 &&
			lfs[0]->isSimpleStateDependency() && n_args > 0) {
		string normalizedExpression = parsedExpression;
		normalizedExpression.erase(
			std::remove_if(normalizedExpression.begin(), normalizedExpression.end(),
				[](unsigned char c) { return std::isspace(c) != 0; }),
			normalizedExpression.end());
		string stateValue = lfs[0]->getName() + "_" +
				argNames[refLfScopes[0]];
		string expectedIntactFirst = paramNames[0] + "*" + stateValue + "*(2-" +
				stateValue + ")+" + paramNames[1] + "*" + stateValue + "*(" +
				stateValue + "-1)/2";
		string expectedIntactSecond = paramNames[1] + "*" + stateValue + "*(2-" +
				stateValue + ")+" + paramNames[0] + "*" + stateValue + "*(" +
				stateValue + "-1)/2";
		if (normalizedExpression == expectedIntactFirst ||
				normalizedExpression == expectedIntactSecond) {
				simpleStateSelector = true;
				simpleStateSelectorFunction = lfs[0];
				if (normalizedExpression == expectedIntactFirst) {
					simpleStateIntactParamIndex = 0;
					simpleStateEndocleavedParamIndex = 1;
				} else {
					simpleStateIntactParamIndex = 1;
					simpleStateEndocleavedParamIndex = 0;
				}
				simpleStateIntactRate = s->getParameter(
						paramNames[simpleStateIntactParamIndex]);
				simpleStateEndocleavedRate = s->getParameter(
						paramNames[simpleStateEndocleavedParamIndex]);
		}
	}

	/* The channel-preserving control uses one of the complementary state
	 * gates.  It keeps the original pair of reaction channels (important for
	 * fixed-seed event-log parity) while still avoiding a parser evaluation for
	 * the gate itself. */
	if (n_gfs == 0 && n_lfs == 1 && n_refLfs == 1 && n_params == 1 &&
			n_reactantCounts == 0 && !fileFunc && lfs[0] != 0 &&
			lfs[0]->isSimpleStateDependency() && n_args > 0) {
		string normalizedExpression = parsedExpression;
		normalizedExpression.erase(
			std::remove_if(normalizedExpression.begin(), normalizedExpression.end(),
				[](unsigned char c) { return std::isspace(c) != 0; }),
			normalizedExpression.end());
		string stateValue = lfs[0]->getName() + "_" +
				argNames[refLfScopes[0]];
		string expectedIntact = paramNames[0] + "*" + stateValue + "*(2-" +
				stateValue + ")";
		string expectedEndocleaved = paramNames[0] + "*" + stateValue + "*(" +
				stateValue + "-1)/2";
		if (normalizedExpression == expectedIntact ||
				normalizedExpression == expectedEndocleaved) {
			simpleStateSelector = true;
			simpleStateSelectorFunction = lfs[0];
			if (normalizedExpression == expectedIntact) {
				simpleStateIntactParamIndex = 0;
				simpleStateEndocleavedParamIndex = -1;
			} else {
				simpleStateIntactParamIndex = -1;
				simpleStateEndocleavedParamIndex = 0;
			}
			simpleStateIntactRate = simpleStateIntactParamIndex >= 0
					? s->getParameter(paramNames[simpleStateIntactParamIndex]) : 0.0;
			simpleStateEndocleavedRate = simpleStateEndocleavedParamIndex >= 0
					? s->getParameter(paramNames[simpleStateEndocleavedParamIndex]) : 0.0;
		}
	}

}

int CompositeFunction::getNumOfArgs() const {
	return this->n_args;
}
string CompositeFunction::getArgName(int aIndex) const {
	return this->argNames[aIndex];
}



void CompositeFunction::updateParameters(System *s)
{
	/* A simple state selector is evaluated by the direct C++ path below and
	 * deliberately has no muParser program.  Keep parameter refreshes for the
	 * general path, but never dereference the absent parser for this compact
	 * representation. */
	if (!simpleStateSelector && p != 0) {
		for(unsigned int i=0; i<n_params; i++) {
			p->DefineConst(paramNames[i],s->getParameter(paramNames[i]));
		}
	}
	if (simpleStateSelector) {
		simpleStateIntactRate = simpleStateIntactParamIndex >= 0
				? s->getParameter(paramNames[simpleStateIntactParamIndex]) : 0.0;
		simpleStateEndocleavedRate = simpleStateEndocleavedParamIndex >= 0
				? s->getParameter(paramNames[simpleStateEndocleavedParamIndex]) : 0.0;
	}
}

void CompositeFunction::prepareForSimulation(System *s)
{
	/* The indexed Rasi selectors are finite-state rate lookups.  Their
	 * evaluator is entirely in C++ (simpleStateSelector), so constructing a
	 * second muParser object for each selector only increases parse/startup
	 * memory and time.  General composite functions retain the existing path. */
	if (simpleStateSelector)
		return;

	try {
		const string expression = normalizeTimeExpression(this->parsedExpression);
		p=FuncFactory::create();
		if (expression != this->parsedExpression) {
			p->DefineVar("time", s->getCurrentTimePointer());
			s->setHasTimeDependentFunctions(true);
		}
		for(int f=0; f<n_gfs; f++) {
			p->DefineVar(gfNames[f],&gfValues[f]);
		}

		//Define local function variables here...
		for(int f=0; f<this->n_refLfs; f++) {
			p->DefineVar(refLfRefNames[f],&refLfValues[f]);
		}

		for(unsigned int i=0; i<n_params; i++) {
			p->DefineConst(paramNames[i],s->getParameter(paramNames[i]));
		}

		for(int r=0; r<n_reactantCounts; r++) {
			string reactantStr = "reactant_"+NFutil::toString((r+1));
			p->DefineVar(reactantStr,&reactantCount[r]);
		}

		if (this->fileFunc && !this->ctrName.empty()) {
			p->DefineConst(this->ctrName, 0.0);
		}

		p->SetExpr(expression);
	}
	catch (mu::Parser::exception_type &e)
	{
		cout<<"Error preparing function "<<name<<" in class CompositeFunction!!  This is what happened:"<<endl;
		cout<< "  "<<e.GetMsg() << endl;
		cout<<"Quitting."<<endl;
		exit(1);
	}


//	exit(0);
}


void CompositeFunction::printDetails(System *s) {

	cout<<"Composite Function: '"<< this->name << "()'"<<endl;
	cout<<" = "<<this->originalExpression<<endl;
	cout<<" parsed expression = "<<this->parsedExpression<<endl;
	cout<<"   -Function References:"<<endl;
	cout<<"looping over funcs, n funcs: "<<n_gfs<<endl;
	for(int f=0; f<n_gfs; f++) {
		// AS-2021
		if (gfs[f]->fileFunc==true) {
			gfs[f]->fileUpdate();
		} 
		// AS-2021
		gfValues[f]=FuncFactory::Eval(gfs[f]->p);
		cout<<"         global function: "<<gfNames[f]<<" = "<<gfValues[f]<<endl;

		gfs[f]->printDetails(s);
	}
	for(int f=0; f<n_lfs; f++) {
		cout<<"         local function: "<<lfs[f]->getNiceName()<<endl;
	}


	if(n_args>0) {
		cout<<"   -Arguments:"<<endl;
		for(unsigned int a=0; a<n_args; a++)
			cout<<"         "<<argNames[a]<<endl;
	}

	if(n_params>0) {
		cout<<"   -Constant Parameters:"<<endl;
		for(unsigned int i=0; i<n_params; i++) {
			cout<<"         "<<paramNames[i]<<" = " << s->getParameter(paramNames[i])<<endl;
		}
	}

	if(p!=0) {
		// AS-2021
		if (this->fileFunc==true) {
			this->fileUpdate();
		}
		// AS-2021
		cout<<"   Function last evaluated to: "<<FuncFactory::Eval(p)<<endl;
	}
		



}



void CompositeFunction::addTypeIMoleculeDependency(MoleculeType *mt,
		ReactionClass *rxn, int reactionPosition) {

	for(int i=0; i<n_lfs; i++) {
		// add typeI dependency, which means this local function influences
		//  the propensity of some DOR reaction for which mt is the head template molecule.
		lfs[i]->addTypeIMoleculeDependency(mt, rxn, reactionPosition);
		if ( refLfScopes[i]==LocalFunction::SPECIES ) {
			// enable complex-scoped evaluation for this local fcn!
			lfs[i]->setEvaluateComplexScope( true );
		}
	}
}


double CompositeFunction::evaluateOn(Molecule **molList, int *scope, int *curReactantCounts, int n_reactants) {
	if (simpleStateSelector) {
		if (molList == 0 || scope == 0) {
			cout<<"Error evaluating composite function: "<<name<<endl;
			cout<<"This function depends on a local function, but you gave no molecules"<<endl;
			cout<<"or scope when calling this function.  Time to quit."<<endl;
			exit(1);
		}
		try {
			return evaluateSimpleStateSelector(molList[refLfScopes[0]]);
		} catch (LocalFunctionException &lfe) {
			lfe.setIndex(0);
			throw lfe;
		}
	}

	if (identityLocalFunction) {
		if (molList == 0 || scope == 0) {
			cout<<"Error evaluating composite function: "<<name<<endl;
			cout<<"This function depends on a local function, but you gave no molecules"<<endl;
			cout<<"or scope when calling this function.  Time to quit."<<endl;
			exit(1);
		}
		try {
			return lfs[refLfInds[0]]->getValue(
					molList[refLfScopes[0]], scope[refLfScopes[0]]);
		} catch (LocalFunctionException &lfe) {
			lfe.setIndex(0);
			throw lfe;
		}
	}

	//1 evaluate all global functions
	for(int f=0; f<n_gfs; f++) {
		// AS-2021
		if (gfs[f]->fileFunc==true) {
			gfs[f]->fileUpdate();
		}
		// AS-2021
		gfValues[f]=FuncFactory::Eval(gfs[f]->p);
	}

	//2 evaluate all local functions
	
	if(n_lfs>0) {

		if(molList!=0 && scope!=0) {

			for(int i=0; i<n_refLfs; i++) {
				try{
					this->refLfValues[i] = this->lfs[refLfInds[i]]->getValue(molList[refLfScopes[i]],scope[refLfScopes[i]]);
				}
				catch (LocalFunctionException &lfe){
					//the parameter that we sent in is incorrect
					lfe.setIndex(i);
					throw lfe;
				}
			}

		} else {

			cout<<"Error evaluating composite function: "<<name<<endl;
			cout<<"This function depends on local functions, but you gave no molecules"<<endl;
			cout<<"or scope when calling this function.  Time to quit."<<endl;
			exit(1);
		}

	}


	//3 update reactant counts as need be
	if(n_reactants<this->n_reactantCounts) {
		cerr<<"Not given enough reactants for this composite function!"<<this->name<<endl;
	}
	for(int r=0; r<n_reactantCounts; r++) {
		reactantCount[r]=curReactantCounts[r];
	}


	// cout<<"evaluating composite function: "<<name<<endl;
	// AS-2021
	if (this->fileFunc==true) {
		this->fileUpdate();
	} 
	// AS-2021
	return FuncFactory::Eval(p);
	//evaluate this function
}

double CompositeFunction::evaluateSimpleStateSelector(Molecule *m)
{
	if (!simpleStateSelector || m == 0)
		return 0.0;
	if (m->getMoleculeType() !=
			simpleStateSelectorFunction->getSimpleStateMoleculeType())
		return simpleStateSelectorFunction->getValue(m, LocalFunction::MOLECULE);
	simpleStateSelectorFunction->cacheSimpleStateValue(m);
	double stateCode = simpleStateSelectorFunction->getSimpleStateValueForState(
			m->getComponentState(
				simpleStateSelectorFunction->getSimpleStateComponent()));
	return simpleStateIntactRate * stateCode * (2.0 - stateCode) +
			 simpleStateEndocleavedRate * stateCode * (stateCode - 1.0) / 2.0;
}

// AS-2021
void CompositeFunction::loadParamFile(const string& filePath)
{
	string callerName = this->name + " in class CompositeFunction";
	NFutil::TimeSeries ts = NFutil::loadTimeSeries(filePath, callerName);
	this->data.push_back(ts.time);
	this->data.push_back(ts.values);
}

void CompositeFunction::addFunctionPointer(GlobalFunction *fPtr) {
	this->ctrType = "Function";
	this->funcPtr = fPtr;
}

void CompositeFunction::addCounterPointer(double *count) {
	this->ctrType = "Observable";
	this->counter = count;
}

void CompositeFunction::setCtrName(const string& name) {
	this->ctrName = name;
}

void CompositeFunction::setInterpolationMethod(const string& method) {
	string normalized = method;
	std::transform(normalized.begin(), normalized.end(), normalized.begin(),
		[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	if (normalized.empty()) normalized = "linear";
	if (normalized != "linear" && normalized != "step") {
		cerr<<"Error preparing function "<<name<<" in class CompositeFunction!!"<<endl;
		cerr<<"Unsupported TFUN interpolation method '"<<method<<"'."<<endl;
		cerr<<"Quitting."<<endl;
		exit(1);
	}
	this->interpolationMethod = normalized;
}

void CompositeFunction::setCounterFromTime(System *s) {
	this->addSystemPointer(s);
}

void CompositeFunction::setCounterFromParameter(System *s, string paramName) {
	this->ctrType = "Parameter";
	this->sysPtr = s;
	this->counterParamName = paramName;
}

void CompositeFunction::addSystemPointer(System *s) {
	this->ctrType = "System";
	this->sysPtr = s;
}

void CompositeFunction::enableFileDependency(const string& filePath, const string& method) {
	try {
		this->loadParamFile(filePath);
	} catch (exception const & e) {
			throw std::runtime_error("Error preparing function " + name + " in class CompositeFunction!!\n" + std::string(e.what()));
	};
	// we just want to keep a record of this
	this->filePath = filePath;
	// this sets it up so that this function knows it's supposed
	// to be pulling values from a file
	this->fileFunc = true;
	// initialize internal index
	this->currInd = 0;
	// pull data lenght so we can reuse it
	this->dataLen = data[0].size();
	// set interpolation method if specified
	if (!method.empty()) {
		this->setInterpolationMethod(method);
	}
}

void CompositeFunction::enableInlineDependency(
	const vector<double> &xs,
	const vector<double> &ys,
	const string& method)
{
	this->data.clear();
	this->data.push_back(xs);
	this->data.push_back(ys);
	this->filePath = "<inline>";
	this->fileFunc = true;
	this->setInterpolationMethod(method);
	this->currInd = 0;
	this->dataLen = static_cast<int>(xs.size());
}

double CompositeFunction::getCounterValue() {
	double ctrVal = 0.0;
	if (ctrType == "Function") {
		if (this->funcPtr == NULL) {
			cerr<<"Error preparing function "<<name<<" in class CompositeFunction!!"<<endl;
			cerr<<"Function TFUN counter pointer is null."<<endl;
			cerr<<"Quitting."<<endl;
			exit(1);
		}
		ctrVal = FuncFactory::Eval(this->funcPtr->p);
	} else if (ctrType == "Observable") {
		if (this->counter == NULL) {
			cerr<<"Error preparing function "<<name<<" in class CompositeFunction!!"<<endl;
			cerr<<"Observable TFUN counter pointer is null."<<endl;
			cerr<<"Quitting."<<endl;
			exit(1);
		}
		ctrVal = (*counter);
	} else if (ctrType == "System") {
		if (this->sysPtr == NULL) {
			cerr<<"Error preparing function "<<name<<" in class CompositeFunction!!"<<endl;
			cerr<<"System TFUN counter pointer is null."<<endl;
			cerr<<"Quitting."<<endl;
			exit(1);
		}
		ctrVal = this->sysPtr->getCurrentTime();
	} else if (ctrType == "Parameter") {
		if (this->sysPtr == NULL || this->counterParamName.empty()) {
			cerr<<"Error preparing function "<<name<<" in class CompositeFunction!!"<<endl;
			cerr<<"Parameter TFUN counter is not configured."<<endl;
			cerr<<"Quitting."<<endl;
			exit(1);
		}
		ctrVal = this->sysPtr->getParameter(counterParamName);
	} else {
		cerr<<"Error preparing function "<<name<<" in class CompositeFunction!!"<<endl;
		cerr<<"TFUN counter type '"<<ctrType<<"' is not supported."<<endl;
		cerr<<"Quitting."<<endl;
		exit(1);
	}
	return ctrVal;
}
void CompositeFunction::fileUpdate() {
	if (data.size() < 2 || data[0].size() == 0) {
		cerr << "Error in function " << this->name << " in class CompositeFunction!!" << endl;
		cerr << "Data for file update is empty or malformed." << endl;
		cerr << "Quitting." << endl;
		exit(1);
	}

	double ctrVal = this->getCounterValue();
	double y = tfun_interpolate_value(data[0], data[1], interpolationMethod, ctrVal);
	p->DefineConst(ctrName, y);
	return;
}
// AS-2021
