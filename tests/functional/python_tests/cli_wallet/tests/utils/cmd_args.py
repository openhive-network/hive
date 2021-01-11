import argparse

args = None

parser = argparse.ArgumentParser(description='Hived cli wallet test args.')
parser.add_argument('--path-to-cli'        , dest='path'               , help ='Path to cli_wallet executable')
parser.add_argument('--creator'            , dest='creator'            , help ='Account to create proposals with')
parser.add_argument('--wif'                , dest='wif'                , help ='Private key for creator account', default ="5JNHfZYKGaomSFvd4NUdQ9qMcEAC43kujbfjueTHpVapX1Kzq2n")
parser.add_argument('--server-rpc-endpoint', dest="server_rpc_endpoint", help = "Set server endpoint [=ws://127.0.0.1:8090]", default ="ws://127.0.0.1:8090")
parser.add_argument('--cert-auth'          , dest="cert_auth"          , help = "Set cert auth [=_default]"                 , default ="_default")
#this argument causes error
#parser.add_argument('--rpc-endpoint'       , dest="rpc_endpoint"       , help = "Set rpc endpoint [=127.0.0.1:8091]"        , default ="127.0.0.1:8091") 
parser.add_argument('--rpc-tls-endpoint'   , dest="rpc_tls_endpoint"   , help = "Set rpc tle endpont [=127.0.0.1:8092]"     , default ="127.0.0.1:8092") 
parser.add_argument('--rpc-tls-cert'       , dest="rpc_tls_cert"       , help = "Set rpc tls cert [=server.pem]"            , default ="server.pem")
parser.add_argument('--rpc-http-endpoint'  , dest="rpc_http_endpoint"  , help = "Set rpc http endpoint [=127.0.0.1:8093]"   , default ="127.0.0.1:8093") 
parser.add_argument('--deamon'             , dest="deamon"             , help = "Set to work as deamon [=False]"            , default =False)
parser.add_argument('--rpc-allowip'        , dest="rpc_allowip"        , help = "Set allowed rpc ip [=[]]"                  , default =[])
parser.add_argument('--wallet-file'        , dest="wallet_file"        , help = "Set wallet name [=wallet.json]"            , default ="wallet.json")
parser.add_argument('--chain-id'           , dest="chain_id"           , help = "Set chain id [=18dcf0a285365fc58b71f18b3d3fec954aa0c141c44e4e5cb4cf777b9eab274e]", default ="18dcf0a285365fc58b71f18b3d3fec954aa0c141c44e4e5cb4cf777b9eab274e")
parser.add_argument("--hive-path"          , dest="hive_path"          , help = "Path to hived executable. Warning: using this option will erase contents of selected hived working directory.", default="")
parser.add_argument("--hive-working-dir"   , dest="hive_working_dir"   , default="/tmp/hived-data/", help = "Path to hived working directory")
parser.add_argument("--hive-config-path"   , dest="hive_config_path"   , default="../../hive_utils/resources/config.ini.in",help = "Path to source config.ini file")
parser.add_argument("--junit-output"       , dest="junit_output"       , default=None, help="Filename for generating jUnit-compatible XML output")
args = parser.parse_args()