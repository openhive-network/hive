import argparse

args = None

parser = argparse.ArgumentParser(description='Hived cli wallet test args.')
parser.add_argument('--path-to-cli'        , dest='path'               , help ='Path to cli_wallet executable')
parser.add_argument('--creator'            , dest='creator'            , help ='Account to create proposals with', default="initminer")
parser.add_argument('--wif'                , dest='wif'                , help ='Private key for creator account', default ="5JNHfZYKGaomSFvd4NUdQ9qMcEAC43kujbfjueTHpVapX1Kzq2n")
parser.add_argument('--server-rpc-endpoint', dest="server_rpc_endpoint", help = "Set server endpoint [=ws://127.0.0.1:8090]", default = "ws://127.0.0.1:8090")
parser.add_argument('--server-http-endpoint', dest="server_http_endpoint", help = "Set server endpoint [=http://127.0.0.1:8091]", default = "http://127.0.0.1:8091")
parser.add_argument('--cert-auth'          , dest="cert_auth"          , help = "Set cert auth"                 , default = None)
#this argument causes error
#parser.add_argument('--rpc-endpoint'       , dest="rpc_endpoint"       , help = "Set rpc endpoint [=127.0.0.1:8091]"        , default ="127.0.0.1:8091") 
parser.add_argument('--rpc-tls-endpoint'   , dest="rpc_tls_endpoint"   , help = "Set rpc tle endpont"     , default = None) 
parser.add_argument('--rpc-tls-cert'       , dest="rpc_tls_cert"       , help = "Set rpc tls cert"            , default = None)
parser.add_argument('--rpc-http-endpoint'  , dest="rpc_http_endpoint"  , help = "Set rpc http endpoint [='127.0.0.1:8093']"   , default = "127.0.0.1:8093") 
parser.add_argument('--deamon'             , dest="deamon"             , help = "Set to work as deamon"            , default =True)
parser.add_argument('--rpc-allowip'        , dest="rpc_allowip"        , help = "Set allowed rpc ip [=['127.0.0.1']]"                  , default =['127.0.0.1'])
parser.add_argument('--wallet-file'        , dest="wallet_file"        , help = "Set wallet name [=wallet.json]"            , default ="wallet.json")
parser.add_argument('--chain-id'           , dest="chain_id"           , help = "Set chain id", default = None)
parser.add_argument("--hive-path"          , dest="hive_path"          , help = "Path to hived executable. Warning: using this option will erase contents of selected hived working directory.", default="")
parser.add_argument("--hive-working-dir"   , dest="hive_working_dir"   , default="/tmp/hived-data/", help = "Path to hived working directory")
parser.add_argument("--hive-config-path"   , dest="hive_config_path"   , default="../../hive_utils/resources/config.ini.in",help = "Path to source config.ini file")
parser.add_argument("--junit-output"       , dest="junit_output"       , default=None, help="Filename for generating jUnit-compatible XML output")
parser.add_argument("--test-filter"        , dest="test_filter"        , default=None, help="Substring for filtering tests by name")
args = parser.parse_args()