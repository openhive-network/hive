# API list

- Condenser Api
- Bridge
- Account By Key Api
- Account History Api
- Block Api
- Database Api
- Debug Node Api (NOT TESTED)
- Follow Api
- Jsonrpc
- Market History Api
- Network Broadcast Api (NOT TESTED)
- Rc Api 
- Reputation Api
- Rewards Api (NOT TESTED)
- Tags Api
- Transaction Status Api (NOT TESTED)
- Witness Api (NOT TESTED)
- Broadcast Ops (NOT TESTED)
- Broadcast Ops Communities (NOT TESTED)


## condenser_api


## bridge

`account_notifications` and `get_ranked_posts` have problems with comparision, but methods work.

Rest of methods works fine, sometimes with differences at `id`.

## account_by_key_api
Every method works.

## account_history_api
`get_transaction` method doesn't work, the rest is fine.

## block_api
Every method works with good comparision.

## database_api
Tested every method. They generally works fine, details are described in documentation. Some of them needs longer waiting time to finish.

## follow_api
Some method are not implemented in APIs, they're not tested also. To have this tests work well we need to have list of exception parameters.

## jsonrpc_api
As descibed in documentation. `get_methods` throws error, rest is fine.

## market_history_api
Worked fine, every methods work, no diffs in compares.

## reputation_api
Currently not working. Waiting for response from Gandalf.

## tags_api
As in documentation. A lot of methods are deprecated, some have other arguments than in docs. And finally, multiple list arguments are in documentation, but isn't implemented in API.