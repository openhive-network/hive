# comment\_operation, // 1

## 1. Description

Using comment operation a user may create a post or a comment. From the blockchain point of view, it is the same operation – always comment. If a comment has no parent, it is a post. The parent of the comment may be a post or a comment. Users may comment their own comments.    

To modify the comment, a new operation comment\_operation is created. Only {title}, {body} or {json\_metadata} may be modified. The body may contain a new content or only the patch with the changes – it is usually done by the frontend applications.    

Sometimes you may see, that some post or comments are:    
\- muted - custom\_json operation,    
\- reblog - custom\_json operation,     
\- pinned – if you are a moderator of the community    
\- custom\_json operation,    
\- cross-posted  - the comment to the post with tag :\["cross-post"].    

## 2. Parameters

| Parameter name | Description | Example |                                                                                                                              
| -------------- | -------------------------------------------------------------------------------------------------------------- | ------------ |
| parent\_author   | Account name, the author of the commented post or comment. If the operation creates a post, it is empty. It cannot be modified.                                                                                                                                                                                  |         |
| parent\_permlink | The identifier of the commented post or comment.When a user creates a post, it may contain the identifier of the community (e.g. hive-174695) or main tag (e.g. travel).It cannot be modified.                                                                                                                   |         |
| author           | Account name, the author of the post or the comment.It cannot be modified.                                                                                                                                                                                                                                       |         |
| permlink         |  Unique to the author, the identifier of the post or comment.It cannot be modified.                                                                                                                                                                                                                              |         |
| title            | The title of the submitted post, in case of the comment, is often empty. It may be modified.                                                                                                                                                                                                                     |         |
| body             | The content of the post or the comment.It may be modified.                                                                                                                                                                                                                                                       |         |
| json\_metadata   | There is no blockchain validation on json\_metadata, but the structure has been established by the community. <br>From the blockchain point of view it is a json file.For the second layer, the following keys may be used:<br>- app, e.g. peakd/2023.2.3<br>- format,  e.g. markdown<br>- tags, e.g. photography<br>- users<br>- images |         |

## 3. Authority

Posting

## 4. Operation preconditions

You need:    

\- enough Resource Credit (RC) to make an operation. The longer the post or comment is, the more RC you need.

## 5. Impacted state

After the operation:    

\- the post or the comment is published.    

\- RC cost is paid by { author }.    

\- if the post or the comment receives enough votes, after 7 days the post reward is calculated and transferred to the user’s sub balance for the rewards (to transfer funds from sub balance to regular balance the operation claim\_reward\_balance\_operation should be used).    

\- separately for each curator that will receive non zero reward, the virtual operation: curation\_reward\_operation is generated.    

\- if the post or the comment receives the reward (non zero), the virtual operation: comment\_reward\_operation is generated.   

\- if the post or the comment receives the reward for the author and the author balance is increased, the virtual operation: author\_reward\_operation is generated.  
 
\- if the post or the comment receives the reward for the author, the beneficiary is set (using operation comment\_options\_operation) and the beneficiary balance is increased, the virtual operation: comment\_benefactor\_reward\_operation is generated.   

\- if the voting time is over and nevertheless the post or comment receives the reward, the virtual operation: comment\_payout\_update\_operation is generated.
