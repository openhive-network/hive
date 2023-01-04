import re
from subprocess import PIPE
from subprocess import run as run_executable

import test_tools as tt


def test_help_option():
    only_args_to_be_founded = [
        "--help",
        "--version",
        "--offline",
        "--server-rpc-endpoint",
        "--cert-authority",
        "--retry-server-connection",
        "--rpc-endpoint",
        "--rpc-tls-endpoint",
        "--rpc-tls-certificate",
        "--rpc-http-endpoint",
        "--unlock",
        "--daemon",
        "--rpc-http-allowip",
        "--wallet-file",
        "--chain-id",
        "--output-formatter",
        "--transaction-serialization",
        "--store-transaction",
    ]

    cli_wallet_path = tt.paths_to_executables.get_path_of("cli_wallet")
    process = run_executable([cli_wallet_path, "--help"], stdout=PIPE, stderr=PIPE)
    stdout = process.stdout.decode("utf-8")
    args_founded = [arg for arg in stdout.split() if "--" in arg]
    diff = list(set(args_founded) ^ set(only_args_to_be_founded))
    tt.logger.info(f"found: {diff}")

    assert len(diff) == 0


def test_wallet_help_default_values():
    cli_wallet_path = tt.paths_to_executables.get_path_of("cli_wallet")
    process = run_executable([cli_wallet_path, "--help"], stdout=PIPE, stderr=PIPE)
    stdout = process.stdout.decode("utf-8")
    lines = stdout.split("\n")
    default_values = {}
    for line in lines:

        parameter = re.match(r".*(--[\w-]+)", line)
        if parameter is not None:
            parameter = parameter[1]
        else:
            continue

        default_value = re.match(r".*\s?\(=(.*)\).*", line)
        default_values[parameter] = default_value[1] if default_value is not None else None

    assert default_values["--help"] is None
    assert default_values["--version"] is None
    assert default_values["--offline"] is None
    assert default_values["--server-rpc-endpoint"] == "ws://127.0.0.1:8090"
    assert default_values["--cert-authority"] == "_default"
    assert default_values["--retry-server-connection"] is None
    assert default_values["--rpc-endpoint"] == "127.0.0.1:8091"
    assert default_values["--rpc-tls-endpoint"] == "127.0.0.1:8092"
    assert default_values["--rpc-tls-certificate"] == "server.pem"
    assert default_values["--rpc-http-endpoint"] == "127.0.0.1:8093"
    assert default_values["--unlock"] is None
    assert default_values["--daemon"] is None
    assert default_values["--rpc-http-allowip"] is None
    assert default_values["--wallet-file"] == "wallet.json"
    assert default_values["--chain-id"] == "18dcf0a285365fc58b71f18b3d3fec954aa0c141c44e4e5cb4cf777b9eab274e"
    assert default_values["--output-formatter"] is None
    assert default_values["--transaction-serialization"] == "legacy"
    assert default_values["--store-transaction"] is None
