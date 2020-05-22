#!/bin/bash

echo hived-testnet: getting deployment scripts from external source

wget -qO- $SCRIPTURL/master/$LAUNCHENV/$APP/testnetinit.sh > /usr/local/bin/testnetinit.sh
wget -qO- $SCRIPTURL/master/$LAUNCHENV/$APP/testnet.config.ini > /etc/hived/testnet.config.ini
wget -qO- $SCRIPTURL/master/$LAUNCHENV/$APP/fastgen.config.ini > /etc/hived/fastgen.config.ini
chmod +x /usr/local/bin/testnetinit.sh

echo hived-testnet: launching testnetinit script

/usr/local/bin/testnetinit.sh
