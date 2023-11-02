# vote\_operation, // 0

## 1. Description

A user may upvote or downvote a post or a comment.

A user has a voting power. The max voting power depends on HP.

There are two manabars related to voting: Voting manabar and Downvoting manabar. Voting and downvoting manabars are defined as a percentage of total vote weight. Downvote manabar has 25% of the vote power weight and vote manabar has 100% of the vote power weight, but a user downvote with the total vote weight (not 25 %, but 100%).

When a user casts a vote, 1/50th of the remaining mana is used for a 100% vote. The voting powers regenerate from 0 to 100% in 5 days (20% per day).

If a voter casts another vote for the same post or comment before rewards are paid, it counts as vote edit. Vote edit cancels all effects of previous vote and acts as completely new vote, except mana used for previous vote is not returned.

The author of the post or the comment may receive the reward, the amount of the author's reward depends on the numbers and powers of the votes. By default the author reward is paid 50% HP and 50 % HBD.

A user who votes for the post or the comment may receive the curation reward. It is paid 100% HP. Its share depends on:

\- voting power

\- weight of the vote – a user may decide about the weight of the vote

\- the time when they vote – the sooner you vote, the higher the share in the curation reward (the first 24 h the weight of the vote is 100% - early voting, next 48 hours the weight is divided by 2, then – till the 7th day - it is divided by 8)

When a post or a comment receives a reward, it is divided between the author's reward and the curation reward. The curation reward is shared among curators. 

The calculated reward should be more than 0.02 HBD to be paid, if it is less, it is not paid.

A downvoting user doesn’t receive the curation reward. Downvoting may affect the author of the comment's reputation when a user who downvotes has a higher reputation than the author. 

Global parameters:     
\- HIVE\_CASHOUT\_WINDOW\_SECONDS, default mainnet value: 7 days


## 2. Parameters

| Parameter name | Description | Example |                                                                                                                              
| -------------- | -------------------------------------------------------------------------------------------------------------- | ------------ |
| voter          | Account name                                                                                                                                                                                          |         |
| author         | Account name, the author of the post or the comment                                                                                                                                                   |         |
| permlink       | The identifier of the post or comment.                                                                                                                                                                |         |
| weight         | It defines how many percent of the non-used voting power a user wants to use.<br>Allowed values from -10000 (-100%) to 10000 (100%).<br>Downvotes: from -10000 (-100%) to 0.<br>Upvotes: from 0 to 10000 (100%). |         |


## 3. Authority

Posting


## 4. Operation preconditions

You need:

\- enough voting or downvoting power.

\- enough Resource Credit (RC) to make an operation.


## 20.5 Impacted state

After the operation:

\- the voting (or downvoting) power is reduced.

\- RC cost is paid by { voter }.

\- if the vote is cast for the first time or edited (but as long as the target comment was not yet cashed out), the virtual operation: effective\_comment\_vote\_operation is generated.

\- if the post or the comment receives enough votes, after 7 days the curator and author’s reward is calculated and transferred to the user’s sub balance for the rewards.

\- if the post or the comment receives the reward for the author  and the author balance is, the virtual operation: author\_reward\_operation is generated.

\- if the post or the comment receives the reward for the curator  and the author balance is, the virtual operation: curation\_reward\_operation is generated.

\- the author reputation is reduced, when the post or comment is downvoted by the user with the higher reputation.
