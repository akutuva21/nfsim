The automated code reviewer hallucinated the contents of `Observable::addReferenceToGlobalFunction`.
It claimed the code was:
```cpp
void Observable::addReferenceToGlobalFunction(GlobalFunction *f) {
	depGlobalFunctions.push_back(f);
}
```

However, the actual code in `src/NFcore/observable.cpp` is:
```cpp
void Observable::addReferenceToGlobalFunction(GlobalFunction *f) {
	f->addCounterPointer(&count);
}
```

And `depGlobalFunctions` doesn't even exist in `Observable`.

My test *was* correct, and it *did* verify the actual behavior of the method (adding a pointer to the count).

So, the reviewer is wrong. I will keep the original (correct) test.
