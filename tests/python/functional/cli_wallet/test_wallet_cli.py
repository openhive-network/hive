from __future__ import annotations

import re
from subprocess import run as run_executable
from typing import TYPE_CHECKING
from urllib.parse import urlsplit

import test_tools as tt

if TYPE_CHECKING:
    from pathlib import Path

# With stdin not being a terminal, cli_wallet writes its prompt to stdout before reading each line.
_PROMPT_COMMANDS = 'set_password "password"\nunlock "password"\nlock\n'
_PROMPT_TIMEOUT_SECONDS = 60


def test_help_option() -> None:
    only_args_to_be_founded = [
        "--help",
        "--version",
        "--offline",
        "--server-rpc-endpoint",
        "--cert-authority",
        "--retry-server-connection",
        "--webserver-ws-endpoint",
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
        "--retry-server-connection-delay",
        "--use-legacy-prompt",
    ]

    cli_wallet_path = tt.paths_to_executables.get_path_of("cli_wallet")
    process = run_executable([cli_wallet_path, "--help"], capture_output=True)
    stdout = process.stdout.decode("utf-8")
    args_founded = [arg for arg in stdout.split() if "--" in arg]
    diff = list(set(args_founded) ^ set(only_args_to_be_founded))
    tt.logger.info(f"found: {diff}")

    assert len(diff) == 0


def test_wallet_help_default_values() -> None:
    cli_wallet_path = tt.paths_to_executables.get_path_of("cli_wallet")
    process = run_executable([cli_wallet_path, "--help"], capture_output=True)
    stdout = process.stdout.decode("utf-8")
    lines = stdout.split("\n")
    default_values: dict[str, str | None] = {}
    for line in lines:
        parameter_match = re.match(r".*(--[\w-]+)", line)
        if parameter_match is not None:
            parameter = parameter_match[1]
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
    assert default_values["--webserver-ws-endpoint"] == "127.0.0.1:8091"
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
    assert default_values["--use-legacy-prompt"] is None


def _prompts_of_session(tmp_path: Path, *arguments: str) -> list[str]:
    cli_wallet_path = tt.paths_to_executables.get_path_of("cli_wallet")
    process = run_executable(
        [cli_wallet_path, "--wallet-file", str(tmp_path / "wallet.json"), *arguments],
        input=_PROMPT_COMMANDS,
        capture_output=True,
        text=True,
        timeout=_PROMPT_TIMEOUT_SECONDS,
    )
    return re.findall(r"(\S+ >>> )", process.stdout)


def _expected_prompts(suffix: str) -> list[str]:
    return [f"{state}{suffix} >>> " for state in ("new", "locked", "unlocked", "locked")]


def test_prompt_shows_connected_server(node: tt.InitNode, tmp_path: Path) -> None:
    server = node.ws_endpoint.as_string(with_protocol=True)
    url = urlsplit(server)

    prompts = _prompts_of_session(tmp_path, f"--server-rpc-endpoint={server}")

    assert prompts == _expected_prompts(f"@{url.hostname}:{url.port}")


def test_prompt_in_offline_mode(tmp_path: Path) -> None:
    assert _prompts_of_session(tmp_path, "--offline") == _expected_prompts("@offline")


def test_legacy_prompt(tmp_path: Path) -> None:
    assert _prompts_of_session(tmp_path, "--offline", "--use-legacy-prompt") == _expected_prompts("")
