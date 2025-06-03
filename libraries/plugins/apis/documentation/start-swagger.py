from __future__ import annotations
import http.server
import socketserver
import argparse
import json
import shutil
import os

def patch_openapi_json(proxy_url: str, src: str = "openapi.json", dst: str = "new-openapi.json") -> None:
    with open(src, "r", encoding="utf-8") as f:
        data = json.load(f)
    # Patch the servers[0].url
    if "servers" in data and len(data["servers"]) > 0:
        data["servers"][0]["url"] = f"http://{proxy_url}:7777"
    with open(dst, "w", encoding="utf-8") as f:
        json.dump(data, f, indent=2)
    print(f"Patched {src} -> {dst} with url: http://{proxy_url}:7777")

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--url", required=True, help="Proxy server URL (e.g. 192.168.6.7)")
    args = parser.parse_args()

    patch_openapi_json(args.url)

    class MyHttpRequestHandler(http.server.SimpleHTTPRequestHandler):
        def do_PUT(self) -> None:  # noqa: N802
            body: str = self.rfile.read(int(self.headers["Content-Length"])).decode()
            print("PUT:", body)  # noqa: T201
            self.send_response_only(204)

    my_server = socketserver.TCPServer(("0.0.0.0", 3000), MyHttpRequestHandler)
    print(f"starting server on: {my_server.server_address[0]}:{my_server.server_address[1]}")
    my_server.serve_forever()