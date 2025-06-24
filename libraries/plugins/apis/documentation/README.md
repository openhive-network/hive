**1. Start HTTP server in current directory**

```
python3 start-swagger.py --url 192.168.6.7
```

```
starting server on: 0.0.0.0:3000
192.168.6.189 - - [03/Jun/2025 11:06:26] "GET / HTTP/1.1" 200 -
192.168.6.189 - - [03/Jun/2025 11:06:26] "GET /swagger-ui.css HTTP/1.1" 304 -
192.168.6.189 - - [03/Jun/2025 11:06:26] "GET /swagger-ui-bundle.js HTTP/1.1" 304 -
192.168.6.189 - - [03/Jun/2025 11:06:27] "GET /openapi.json HTTP/1.1" 304 -
```

**2. Launch proxy server**
```
npm i http-proxy
node proxy.js
```
```
Start proxy on port 7777 for https://api.hive.blog
```
*Note that in `proxy.js` script sets:*
```
const options = {
  port: 7777,
  target: "api.hive.blog"
};
```

**3. Launch hive documentation in webbrowser**

http://192.168.6.7:3000/
