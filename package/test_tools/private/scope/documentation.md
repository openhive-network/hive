## High level description

Package `scope` introduces scopes for:
- limiting lifetime of objects,
- provide scope context.

### Limiting lifetime of objects

This package introduces guarantee, that for each `ScopedObject` created in scope, when object will leave scope, its `at_exit_from_scope` method will be called. Objects cleanup is performed in reversed order of creation. Note, that object is not deleted then. Only its method is called, which should perform cleanup and free allocated resources.

### Provide scope context

Each scope provides access to predefined information. For now user code can get access to:
- directory, where files should be created
- logger, which should be used to register logs

Above list is open and can be extended in the future.

Within scope its context information can be modified. After exit from scope, context information from above scope will be restored.

## How it works

Below examples shows what happens on library side when `scopes` are used.

### Manual tests:

* (When importing TestTools) Create `ScopesStack` singleton
  * Register in `atexit`
  * Enter scope: **"root"**
    * Set directory in **root's** `Context`: Path('./generated')
* Run test
  * (for example) Create node
    * `ContextInterface.get_current_directory()` (now directory is really created)
* `atexit` calls `ScopesStack` teardown
  * Exit scope: **"root"**
    * Optionally remove directory from **root's** `Context`

### Automatic tests (function scope):

* (When importing TestTools) Create `ScopesStack` singleton
  * Register in `atexit`
  * Enter scope: **"root"**
* Pytest runs module-fixture setup
  * Enter scope: **"module"**
    * Set directory in **module's** `Context`: Path('./generated_during_test...')
* Pytest runs function-fixture setup
  * Enter scope: **"function"**
    * Set directory in **function's** `Context`: Path('./test...')
* Run test
  * (for example) Create node
    * `ContextInterface.get_current_directory()` (now directory is really created)
* Pytest runs function-fixture teardown
  * Exit scope: **"function"**
    * Optionally remove directory from **function's** `Context`
* Pytest runs module-fixture teardown
  * Exit scope: **"module"**
    * Optionally remove directory from **module's** `Context`
* `atexit` calls `ScopesStack` teardown
  * Exit scope: **"root"**
    * Optionally remove directory from **root's** `Context`
