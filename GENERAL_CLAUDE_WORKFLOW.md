# General Claude Workflow Guidelines

This document defines workflow rules for Claude Code sessions when working on projects in the hive ecosystem. These rules ensure consistent, high-quality contributions.

## GitLab Configuration

- **GitLab Instance**: gitlab.syncad.com (use this URL even when "gitlab" is referenced without qualification)
- **CLI Tool**: `glab` is available and pre-configured with an access token for GitLab interactions

### Usage Notes

When interacting with GitLab (creating MRs, viewing issues, API calls, etc.), always use:
- Host: `gitlab.syncad.com`
- Tool: `glab` CLI (already authenticated)

Example commands:
```bash
# List merge requests
glab mr list

# Create a merge request
glab mr create --title "Title" --description "Description"

# View project issues
glab issue list
```

## Context Management

When working with Claude Code, **minimize context usage** by filtering tool outputs:

1. **Filter command output** - Use `grep`, `head`, `tail` to extract only relevant information
2. **Limit search results** - Use specific patterns and limit result counts
3. **Avoid verbose output** - Prefer summary views over full dumps

```bash
# Good: Filter to specific jobs
glab ci status | grep -E "(testnet_node_build|prepare_hived_image)"

# Bad: Full pipeline status (100+ lines)
glab ci status

# Good: Last N lines of logs
glab api "projects/198/jobs/ID/trace" | tail -50

# Good: Search with limited context
git log --oneline -10

# Bad: Full git log
git log
```

**Why this matters:**
- Claude has a finite context window
- Large outputs consume tokens without adding value
- Filtered output enables more conversation turns
- Faster response times with less data to process

## Workflow Rules

### Finding Work To Do

When asked to find or identify work, check for assigned items on GitLab:

```bash
# List merge requests assigned to current user
glab mr list --assignee=@me

# List issues assigned to current user
glab issue list --assignee=@me

# Get details of a specific MR
glab mr view <MR_NUMBER>

# Get details of a specific issue
glab issue view <ISSUE_NUMBER>
```

**Workflow:**
1. Check assigned MRs first - these often represent in-progress work requiring attention
2. Check assigned issues - these represent planned work items
3. Review MR/issue descriptions and comments for context and requirements
4. Use the issue/MR details to understand the work plan and expected deliverables

**Task Continuation:**
- When a task is **finished** or **blocked** (e.g., MR waiting for approval), look for other work:
  1. Check for other assigned MRs that need attention
  2. Check for assigned issues to start new work
  3. Continue with the next available task

### Branch Management

**Before starting any work:**
1. Verify the currently checked out branch is the correct one for the task
2. If not, checkout the correct branch and ensure it matches the remote

```bash
# Check current branch
git branch --show-current

# Fetch latest from remote
git fetch origin

# Verify branch is up to date with remote
git status
```

**Starting new work:**
1. Always create a feature branch from a fresh `origin/develop`:

```bash
git fetch origin
git checkout -b <feature-branch-name> origin/develop
```

2. Create an MR based on this branch targeting `develop`

### Merge Request Rules

- **All MRs must be created as Draft** until approved by a project maintainer
- **Draft MRs cannot be merged** into `develop` by you
- Only after maintainer approval should the Draft status be removed
- **MR remains Draft if it contains fixup or WIP commits** - These must be autosquashed via local rebase after approval, before removing Draft status

**Fast-Forward Merge Strategy:**
- All projects use **fast-forward merge** git strategy
- Before merging an approved MR:
  1. Verify the branch is rebased on fresh `origin/develop`
  2. Ensure pipeline passes after rebase
- If not rebased, rebase first and wait for pipeline to pass

```bash
# Create a draft MR
glab mr create --draft --title "Title" --description "Description" --target-branch develop

# View MR status
glab mr view <MR_NUMBER>

# After approval, squash fixup/WIP commits before removing draft status
git rebase -i --autosquash origin/develop
git push --force-with-lease

# Before merge: ensure rebased on latest develop and pipeline passes
git fetch origin
git rebase origin/develop
git push --force-with-lease
# Wait for pipeline to pass, then merge
```

### Working With MR Feedback

When continuing work on an existing MR:

1. **Analyze discussion threads** - Review all comments and reported problems in the MR
2. **Address requested changes** using appropriate commit strategy:
   - Use **fixup commits** for small corrections to specific previous commits
   - Use **new commits** for additional changes or distinct fixes

```bash
# View MR discussions
glab mr view <MR_NUMBER> --comments

# Create a fixup commit for a specific commit
git commit --fixup=<commit-hash>

# Later, squash fixups with interactive rebase (before final merge)
git rebase -i --autosquash origin/develop
```

### Git History Best Practices

- **Keep git history clean** - Each commit should be atomic and self-contained
- **Split logical work into separate commits** - This simplifies code review and future analysis
- **Write meaningful commit messages** - Describe the "why", not just the "what"
- **One logical change per commit** - Don't mix unrelated changes

```bash
# Stage specific files/hunks for logical separation
git add -p <file>

# Amend the last commit if needed (only before pushing)
git commit --amend
```

### Local Build Verification

**Always build locally before pushing changes** to catch errors early and save CI resources:

1. **Build the specific target** - Use project CLAUDE.md for build commands
2. **Run affected tests** - Execute relevant unit tests locally
3. **Only push after local success** - This prevents wasted CI cycles

```bash
# Example: Build testnet with tests (from hive project CLAUDE.md)
mkdir -p build && cd build
cmake -DCMAKE_BUILD_TYPE=Debug -DBUILD_HIVE_TESTNET=ON -GNinja ../
ninja chain_test

# Run the specific test
./tests/chain_test --run_test=crypto_memo_tests
```

**Why build locally first?**
- CI builds take 7+ minutes; local builds are faster for iteration
- Saves shared CI resources for the team
- Immediate feedback without waiting for pipeline queues
- Catches obvious errors before they reach the remote

### Pipeline Monitoring

After pushing changes to an MR branch, **always monitor the pipeline** until critical jobs complete:

1. **Monitor build jobs** - Wait for `testnet_node_build` and `prepare_hived_image` to complete
2. **Check for failures early** - Don't wait for the entire pipeline; catch build errors quickly
3. **Stay engaged** - Continue monitoring while working on other tasks or reviewing feedback

```bash
# Check overall pipeline status
glab ci status

# Monitor specific jobs (build jobs are critical)
glab ci status | grep -E "(testnet_node_build|prepare_hived_image|chain_test)"

# Get pipeline URL for web view
glab ci status | tail -3
```

**Why monitor?**
- Build failures block all downstream tests
- Early detection allows faster iteration
- Ensures your changes don't break the build for others

### Pipeline Failure Handling

When a pipeline associated with your MR or branch fails:

1. **Analyze the cause** - Review pipeline logs to identify the failure
2. **If caused by MR changes** - Fix the issue in the current MR
3. **If NOT caused by MR changes** (pre-existing issue):
   - Create a new issue describing the problem
   - Assign the issue to yourself
   - Work on it in a **separate work cycle/instance**
   - **Do not mix unrelated fixes** into the current MR

```bash
# View pipeline status
glab ci status

# View pipeline logs
glab ci view

# Create issue for unrelated failure
glab issue create --title "Pipeline failure: <description>" --assignee=@me
```

### Multi-Repository / Multi-MR Work

Some work requires multiple MRs across repositories in the `gitlab.syncad.com/hive` group.

**Linking MRs:**
- MRs must be linked together to document the dependency chain
- Reference related MRs in the description (e.g., `Depends on hive/hive!123`)

**Merge Priority:**
- Approved MRs with **longer dependency chains** have higher merge priority
- The final commit in `develop` from a base MR must exist before dependent repos can reference it
- Merge order: base dependencies first, then dependent MRs

**Example dependency chain:**
```
hive/hive!100 (base) → hive/haf!30 → hive/hivemind!50
```
Merge `hive/hive!100` first, then `hive/haf!30`, then `hive/hivemind!50`.
