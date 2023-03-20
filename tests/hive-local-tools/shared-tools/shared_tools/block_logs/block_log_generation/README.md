
Here is an example how to create a whole architecture of sub networks.
If you want to create your own custom network, just create a new config JSON.

config = {
    "networks": [
                    {
                        "InitNode"     : True,
                        "ApiNode"      : True,
                        "WitnessNodes" :[3, 3, 2, 2]
                    },
                    {
                        "ApiNode"      : True,
                        "WitnessNodes" :[3, 3, 2, 2]
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
   (ApiNode)
   (WitnessNode-0 (witness10-beta)(witness11-beta)(witness12-beta))
   (WitnessNode-1 (witness13-beta)(witness14-beta)(witness15-beta))
   (WitnessNode-2 (witness16-beta)(witness17-beta))
   (WitnessNode-3 (witness18-beta)(witness19-beta)) 
