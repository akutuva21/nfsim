1. **Add error checks to `CompositeFunction::fileUpdate()`**
   - Use `replace_with_git_merge_diff` to add error checks to `src/NFfunction/compositeFunction.cpp`. The checks should ensure `data` and `dataLen` are valid, and throw an error with `cerr` and `exit(1)` if `ctrVal` leaps backwards relative to `currInd`.

2. **Add error checks to `GlobalFunction::fileUpdate()`**
   - Use `replace_with_git_merge_diff` to add the exact same error checks to `src/NFfunction/function.cpp`.

3. **Verify changes**
   - Use `cat` in `run_in_bash_session` to read the modified sections of `src/NFfunction/compositeFunction.cpp` and `src/NFfunction/function.cpp` and ensure the checks were correctly added.

4. **Run unit tests**
   - Use `run_in_bash_session` to execute the tests via `python validate/validate.py validate/.` and confirm there are no test failures.

5. **Complete pre-commit steps**
   - Complete pre-commit steps to ensure proper testing, verification, review, and reflection are done.

6. **Submit**
   - Use `submit` to push the changes under the branch name `fix/fileupdate-error-checks`.
