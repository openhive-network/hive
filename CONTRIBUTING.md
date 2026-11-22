# Hive

[![GitLab](https://img.shields.io/badge/Development%20on-Community%20GitLab-orange)](https://gitlab.syncad.com/hive/hive) [![GitHub](https://img.shields.io/badge/Mirror%20on-GitHub-black)](https://github.com/openhive-network/hive)

Hive is developed on the [community-hosted GitLab](https://gitlab.syncad.com/hive/hive): issues, merge requests and CI are there. The same code is on GitHub at [openhive-network/hive](https://github.com/openhive-network/hive), the Hive community's GitHub organization, kept up to date as a push mirror of GitLab.

# Please Read

Please read these instructions before submitting issues to the Hive repository. The issue tracker is for bugs and specific implementation discussion **only**. It should not be used for other purposes, as described below.

#### **Did you find a bug?**

* **Do not open up a GitLab/GitHub issue if the bug is a security vulnerability, and instead please email it to [outreach+security@hive.io](mailto:outreach+security@hive.io)
* **Ensure the bug was not already reported** by searching on community GitLab under [Issues](https://gitlab.syncad.com/hive/hive/-/issues).
* If there is an existing feature that is not working correctly, or a glitch in the blockchain that is impacting user behaviour - please open an issue to report variance. Include as much relevant information as you can, including screen shots or log output when applicable, and steps to reproduce the issue.

## Enhancement Suggestions

Do **not** use the issue tracker to suggest enhancements or improvements to the platform. The best place for these discussions is on the Hive blockchain itself. If there is a well vetted idea that has the support of the community that you feel should be considered by the development team, please email it to [hiveio+suggestions@protonmail.com](mailto:hiveio+suggestions@protonmail.com) for review.

## Implementation Discussion

The developers frequently open issues to discuss changes that are being worked on. This is to inform the community of the changes being worked on, and to get input from the community and other developers on the implementation.

Issues opened that devolve into lengthy discussion of minor site features will be closed or locked.  The issue tracker is not a general purpose discussion forum.

This is not the place to make suggestions for product improvement (please see the Enhancement Suggestions section above for this). If you are not planning to work on the change yourself - do not open an issue for it.

## Pull Requests

Anybody in the community is welcome and encouraged to submit pull requests with any desired changes to the platform!

Requests to make changes that include working, tested pull requests jump to the top of the queue. There is not a guarantee that all functionality submitted as a PR will be accepted and merged, however. Please read through our [Git Guidelines](doc/git-guidelines.md) prior to submitting a PR.

## Changes spanning hive, wax and hive-local-tools

The Python tests of hive use `tests/python/hive-local-tools`, which pins packages released by other repositories:
`hiveio-wax` and `hiveio-test-tools` (built by [wax](https://gitlab.syncad.com/hive/wax) from its `hive` submodule)
and `hiveio-beekeepy` ([beekeeper](https://gitlab.syncad.com/hive/beekeeper)). A change crossing these repositories
goes through separate merge requests, each merged before the next one uses it, so that `develop` never pins
development builds (`<version>.dev<timestamp>`) of another repository:

1. **hive** - the changes wax needs (skipped when wax only updates its `hive` submodule). After the merge the `develop`
   pipeline publishes the release of `hiveio-api` (`<version>.post<timestamp>`).
2. **wax** - points the `hive` submodule and the ref of the hive CI include at that `develop` commit and pins the
   releases of `hiveio-api` and `hiveio-beekeepy`. After the merge the `develop` pipeline of wax publishes the releases
   of `hiveio-wax` and `hiveio-test-tools`.
3. **hive** again - bumps `hive-local-tools` to the `develop` releases of wax and test-tools (and of beekeepy if it
   changed).

The merge request of step 1 is tested with the pins of `develop`, i.e. with the current releases of wax and test-tools:
it must not break them, and tests needing the new wax come with step 3. Development builds may be pinned on a branch to
try the whole chain out, but such a pin is never merged.
