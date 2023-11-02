# delete\_comment\_operation, // 17

## 1. Description

The post or comment may be deleted by the author. If the post or comment is deleted, the {permlink} may be reused. 

The delete doesn’t mean that the comment is removed from the blockchain. 


## 2. Parameters

| Parameter name | Description | Example |                                                                                                                              
| -------------- | ---------------------------------------------------- | ------- |
| author         | Account name, the author of the post or the comment. |         |
| permlink       | The identifier of the post or the comment.           |         |


## 3. Authority

Posting


## 4. Operation preconditions

You need:

\- you have to be an author of the comment.

\- the comment cannot have any replay.

\- the comment cannot have a net positive weight of votes (it may have a negative weight).

\- the comment cannot be already paid.

\- enough Resource Credit (RC)  to make an operation.


## 5. Impacted state

After the operation:

\- the original operation is still available in the blockchain, it is only marked as deleted in the hivemind.

\- the { permlink } of the deleted comment may be reused.

\- RC cost is paid by { author }.

\- up to HF19, if the comment could not be deleted, the virtual operation: ineffective\_delete\_comment\_operation was generated.
