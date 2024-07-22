## Branches
- `master`: Points to the latest version of code that is production-ready and has been tested in production. This is the branch we recommend for exchanges. Any tag pointing to an official release version (i.e. `v1.27.0`) is set to commits contained in this branch.
- `develop`: The active development branch. We will strive to keep `develop` in a working state. All MRs must pass automated tests before being merged. While this is not a guarantee that `develop` is bug-free, it will guarantee that the branch is buildable in our standard build configuration and passes the current suite of tests. That being said, running a node from `develop` has risks.  We recommend that any block producing node build from `master` or even using the latest offically tagged release (i.e. `v1.27.5`). If you want to create and test new features, you should create your feature branch from the `develop` branch.

### Releases

There are two release procedures, depending on if the release is a major or minor release. Every major release (i.e. change from 1.26.x -> 1.27.0) enforces a hard fork and earlier hived versions will stop working when the hard fork is triggered. If the release is a minor release, i.e. 1.27.0 -> 1.27.1, hived versions coming from 1.27.x release line will work together, altough upgrading from one version to another may require a full replay of the hived node. Every release should have a proper tag created for it (tagging a commit in the `master` branch).

### Feature Branches

All changes should be developed in their own branch. These branches should branch off `develop` and then be merged back into the actual head of the `develop` branch when the given feature implementation is ready. The naming scheme we use is the issue number, then a dash, followed by a shorthand description of the issue. For example, issue 22 is to allow the removal of an upvote. Branch `22-undo-vote` was used to develop the patch for this issue. If a branch is being developed by a single programmer, we often prefix the branch name with the programmer's initials.

Repository has set a Fast-Forward Merge scheme, what means that any feature branch must be rebased against origin/develop to merge the changes onto the target's branch top. Below is the basic workflow related to git operations:

```
  git fetch # Update our local git repo. Don't use git pull, since it can automatically merge changes incoming from remote
  git checkout origin/develop # Checkout origin/develop HEAD commit to start from
  git submodule update --init --recursive # Required after each branch change to potentially update changed submodules
  git checkout -b issue_1313_best_world_feature
  # ... Here your work including several commits
  # Once you finished
  git fetch # Probably develop changed in the meantime, get its fresh version
  git checkout issue_1313_best_world_feature # be sure my local branch is active one
  git rebase origin/develop
  git submodule update --init --recursive
  # Build, test locally and now it's time to push changes. 
  git push # add -f is you already pushed in the past and rebase changed your base commit
```

When your branch is ready to be merged to develop, create a merge request (MR), with your feature branch as the source and `develop` as the target branch. 

If you want a review or comments from other devs on a long lasting feature branch before it is ready for merging (highly recommended), create a draft merge request by prefixing the merge request title with "Draft:". Marking a merge request as draft will prevent maintainers from prematurely merging it. When the branch is fully tested and ready to merge, simply remove "Draft:" from the title.

Use fixup commits and interactive rebase to cleanup set of changes finally prepared to merge.

## Pull Requests

All changes to `develop` are done through GitHub Pull Requests (PRs)/Gitlab Merge Requests. This is done for several reasons:

- It enforces testing. All pull requests undergo automated testing before they are allowed to be merged.
- It enforces best practices. Because of the cost of a pull request, developers are encouraged to do more testing themselves and be certain of the correctness of their solutions.
- It enforces code review. All pull requests must be reviewed by a developer other than the creator of the request. When a developer reviews and approves a pull request they should +1 the request or leave a comment mentioning their approval of the request. Otherwise they should describe what the problem is with the request so the developer can make changes and resubmit the request.

All pull requests should reference the issue(s) they relate to in order to create a chain of documentation.

If your pull request has merge conflicts, please rebase locally your feature branch against origin/develop.

Until finished, please mark your PR/MR as Draft to notify reviewers that solution is not yet ready for processing.

If you make changes that impact data in the blockchain state file (shared_memory file), mark your PR/MR using the 'Invalidates-chain-state' label, to indicate the changes will require a chain replay. If possible, please describe this fact in the commit message for the change change or even mark it also by tag similar to this one:

https://gitlab.syncad.com/hive/hive/-/tags/Invalidates-chain-state_2022_12_06

Above will clarify dilemma to perform hived node replay during internal testing.

## Policies

### Force-Push Policy

- In general, the protected branches `master` and `develop` must never be force pushed.
- Individual patch branches may be force-pushed at any time, at the discretion of the developer or team working on the patch. Do not force push another developer's branch without their permission.

### Tagging Policy

- Tags can be specific for releases. The version scheme is `vMajor.Hardfork.Release` (Ex. v0.5.0 is the version for the Hardfork 5 release).
- Tags should be also used to mark important changes i.e. breaking hived state compatibility (requirement to replay hived node).

### Code Review Policy / PR Merge Process

- Two developers *must* review *every* change before it moves into `master`, enforced through pull requests. Seemingly benign changes can have profound impacts on consensus. Every change should be treated as a potential consensus breaking change.
- One of the reviewers may be the author of the change.
- This policy is designed to encourage you to take off your "writer hat" and put on your "critic/reviewer hat."  If this were a patch from an unfamiliar community contributor, would you accept it?  Can you understand what the patch does and check its correctness based only on its commit message and diff? Does it break any existing tests, or need new tests to be written? Is it stylistically sloppy -- trailing whitespace, multiple unrelated changes in a single patch, mixing bug fixes and features, or overly verbose debug logging?
- Having multiple people look at a patch reduces the probability it will contain uncaught bugs.
