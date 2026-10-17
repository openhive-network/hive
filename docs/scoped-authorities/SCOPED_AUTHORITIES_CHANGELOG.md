# Scoped Authorities Changelog

## Initial audit and design

- Added a protocol proposal and threat model under `docs/scoped-authorities/`.
- No C++ consensus changes, operation definitions, database objects, API endpoints, scripts, or tests are implemented in this changeset yet.
- Repository baseline observed: branch `feature/scoped-authorities`, commit `9d6007615`, describe `1.28.8-rc3-87-g9d6007615`.
- Mainnet hardfork maximum in this source is HF28; alternate-chain configuration includes HF29. The feature requires a separately assigned future activation and is not wired to an existing hardfork.

## Development implementation and regression verification

- Added `create_scoped_authority_operation`, `revoke_scoped_authority_operation`, and `scoped_transfer_operation`; the three tags are appended after historic virtual-operation tags and retain non-virtual classification. Regression assertions pin transfer tag 2, recurrent transfer tag 49, first and last historical virtual tags 50 and 92, and new tags 93 through 95.
- Added a persistent Chainbase scoped-authority object and grantor/grant-ID and actor/grantor/grant-ID indexes, evaluators, HF30 alternate-chain gating, canonical HIVE/HBD limits, fixed-period and lifetime counters, recipient checks, expiration, revocation, and RC operation costs.
- Added `database_api.get_scoped_authorities` and account-history filters for the three new regular operations.
- Added eight consensus tests, including actual block confirmation, 100-block revocation persistence, exact expiration timestamp enforcement, failed multi-operation transaction rollback, and revoke block-pop rollback. The most recent complete chain run passed all 506 cases and 1,834,936 assertions. The latest focused run passed 8 cases and 139 assertions.
- Updated the `rc_stats_object` size and decoded checksum expectations to account for RC slots indexed through the appended operation tags.
- Local RPC smoke test returned an empty grant list before and after restarting the development node. A populated grant restart, signed RPC transfer demo, snapshot restore, full replay determinism, Python integration suite, and external audit remain outstanding. The Python tools package is pinned to a private Hive package registry and could not be resolved in this environment.
- The verified implementation is committed locally; no remote push has been made.

