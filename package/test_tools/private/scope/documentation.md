## High level description

Package `scope` introduces scopes for limiting lifetime of objects.

### Limiting lifetime of objects

This package introduces guarantee, that for each `ScopedObject` created in scope, when object will leave scope, its `at_exit_from_scope` method will be called. Objects cleanup is performed in reversed order of creation. Note, that object is not deleted then. Only its method is called, which should perform cleanup and free allocated resources.

## How it works

Below examples shows what happens on library side when `scopes` are used.

### Manual tests:

* (When importing TestTools) Create `ScopesStack` singleton
  * Register in `atexit`
  * Enter scope: **"root"**
* Run test
* `atexit` calls `ScopesStack` teardown
  * Exit scope: **"root"**

### Automatic tests (function scope):

* (When importing TestTools) Create `ScopesStack` singleton
  * Register in `atexit`
  * Enter scope: **"root"**
* Pytest runs module-fixture setup
  * Enter scope: **"module"**
* Pytest runs function-fixture setup
  * Enter scope: **"function"**
* Run test
* Pytest runs function-fixture teardown
  * Exit scope: **"function"**
* Pytest runs module-fixture teardown
  * Exit scope: **"module"**
* `atexit` calls `ScopesStack` teardown
  * Exit scope: **"root"**
