### Scope of Foundation Layer Tests

By name "foundation layer" we mean low level parts of blockchain, which allow to build more complicated things on top of
it.

To make it possible to create account (or perform any other operation) in blockchain, there were need to be implemented
many things earlier. Mechanism of block production, composition of blocks containing transactions containing operations,
unique IDs, undo mechanism, broadcasting transactions, ensuring that transactions are applied completely or rejected
(are not applied partially). This is not a complete list of features, but I hope now you have some intuition which tests
should be here.

Here shouldn't be tested any feature built on top of foundation layer like proposals' system, market or RC delegations.
But such higher level operations can be used to test foundation layer. For example, tester can create a comment (surely
a higher level operation), to have some operation and check its ID or to check if it was broadcast correctly. In this
example, creation of comment is not a point of a test. It could be any other operation. But it allowed to test some
foundation layer part of blockchain.
