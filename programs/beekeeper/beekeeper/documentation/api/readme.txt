How to launch beekeeper documentation?

Example:
HTTP server or number ports presented in following information should be treated as an example.

1. Launch beekeeper
./beekeeper --webserver-http-endpoint 127.0.0.1:7778 --notifications-endpoint 127.0.0.1:8000

2. Launch proxy server
node proxy.js

Note that in `proxy.js` script are settings:

const options = {
  port: 7777,
  target: "http://localhost:7778"
};

3. Start HTTP server `serve` in current directory
serve

│   Serving!                                │
  │                                           │
  │   - Local:    http://localhost:3000       │
  │   - Network:  http://192.168.6.150:3000 


4. Launch beekeeper documentation in webbrowser:
http://192.168.6.150:3000/

Note, that in `openapi.json` file a server is declared as follows
  "servers": [
    {
      "url": "http://192.168.6.150:7777"
    }
