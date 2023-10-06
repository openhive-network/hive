
Here is an example how to create a whole architecture of sub networks.
If you want to create your own custom network, just create a new config JSON.

If u want to create new block log - set environment variable "GENERATE_NEW_BLOCK_LOG"
The number of witnesses in witness-nodes is specified in the list: "WitnessNodes": [4, 6, 10]
Such a record means that there will be three witness nodes in the network with 4, 6, 10 witnesses.

The number of ApiNode and FullApiNode can be set by specifying the number of nodes that we want to have in the network.


config = {
    "networks": [
                    {
                        "InitNode"     : True,
                        "ApiNode"      : 1,
                        "WitnessNodes" :[3, 3, 2, 2],
                    },
                    {
                        "FullApiNode"      : 1,
                        "WitnessNodes" :[3, 3, 2, 2],
                    }
                ]
}

Result:

 (Network-alpha)
   (InitNode)
   (ApiNode)
   (WitnessNode-0 (witness0-alpha)(witness1-alpha)(witness2-alpha))
   (WitnessNode-1 (witness3-alpha)(witness4-alpha)(witness5-alpha))
   (WitnessNode-2 (witness6-alpha)(witness7-alpha))
   (WitnessNode-3 (witness8-alpha)(witness9-alpha))
 (Network-beta)
   (FullApiNode)
   (WitnessNode-0 (witness10-beta)(witness11-beta)(witness12-beta))
   (WitnessNode-1 (witness13-beta)(witness14-beta)(witness15-beta))
   (WitnessNode-2 (witness16-beta)(witness17-beta))
   (WitnessNode-3 (witness18-beta)(witness19-beta))
