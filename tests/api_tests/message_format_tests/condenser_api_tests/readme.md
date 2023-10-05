
Some of tests can't be performed on 5 million and 64 million blocklog. 5 million blocklog contains beginning of blockchain
and not many operations are within it. A lot of methods and features have been introduced after this time and hitting
run on 5 million blocklog returns nothing. In case of 64 million blocklog, it is constantly being updated, and it's
causing problems. For example checking account recovery requests, at this time one account is requesting for recovery,
but after update, this account might no longer be in this state (account can be recovered by this time).
