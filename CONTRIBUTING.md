# Please Read

Please read these instructions before submitting issues to the Hive GitHub repository. The issue tracker is for bugs and specific implementation discussion **only**. It should not be used for other purposes, as described below.

## Bug Reports

If there is an existing feature that is not working correctly, or a glitch in the blockchain that is impacting user behaviour - please open an issue to report variance. Include as much relevant information as you can, including screen shots or log output when applicable, and steps to reproduce the issue.

## Enhancement Suggestions

Do **not** use the issue tracker to suggest enhancements or improvements to the platform. The best place for these discussions is on the Hive blockchain itself. If there is a well vetted idea that has the support of the community that you feel should be considered by the development team, please email it to [hiveio+suggestions@protonmail.com](mailto:hiveio+suggestions@protonmail.com) for review.

## Implementation Discussion

The developers frequently open issues to discuss changes that are being worked on. This is to inform the community of the changes being worked on, and to get input from the community and other developers on the implementation.

Issues opened that devolve into lengthy discussion of minor site features will be closed or locked.  The issue tracker is not a general purpose discussion forum.

This is not the place to make suggestions for product improvement (please see the Enhancement Suggestions section above for this). If you are not planning to work on the change yourself - do not open an issue for it.

## Duplicate Issues

Please do a keyword search to see if there is already an existing issue before opening a new one.

## Pull Requests

Anybody in the community is welcome and encouraged to submit pull requests with any desired changes to the platform!

Requests to make changes that include working, tested pull requests jump to the top of the queue. There is not a guarantee that all functionality submitted as a PR will be accepted and merged, however. Please read through our [Git Guidelines](doc/git-guidelines.md) prior to submitting a PR.

## How to create a merge request when you also need to update test-tools for your feature
* Prepare changes for hive's develop and TestTools' master.
* All changes must be rebased to target branches. the merge request in hive should have the TestTools submodule updated to point to your test tools merge request. Both CIs has to be green. (Note: nothing is merged to master/develop yet).
* Create merge requests in hive and TestTools and wait for review. Write a note in the hive merge request mentioning that it needs the merge request from TestTools.