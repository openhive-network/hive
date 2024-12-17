How to establish HTTPS/WSS connection?

https://stackoverflow.com/questions/60030906/self-signed-certificate-only-works-with-localhost-not-127-0-0-1

There are generated files in `example` directory. Using configuration files `root.cfg` and `server.cfg` it's possible to prepare a certificate and a private key needed for HTTPS/WSS connection. There is an example in directory for localhost and 127.0.0.1.

How to do it?

openssl req -x509 -new -keyout root.key -out root.crt -config root.cfg
openssl req -nodes -new -keyout server.key -out server.csr -config server.cfg
openssl x509 -req -in server.csr -CA root.crt -CAkey root.key -set_serial 123 -out server.crt -extfile server.cfg -extensions x509_ext

HTTPS command line parameters:

`webserver-https-endpoint` Local https endpoint for webserver requests
`webserver-https-certificate-file-name` File name with a server's certificate. Default `server.crt`.
`webserver-https-key-file-name` File name with a server's private key. Default `server.key`.

Example of connection:

*****HTTPS server*****
./beekeeper --webserver-https-endpoint 127.0.0.1:6666 --notifications-endpoint 127.0.0.1:8000 --webserver-https-certificate-file-name my_server.crt --webserver-https-key-file-name my_server.key

*****HTTPS client*****
We have two possibilities:

a) A server's certificate is explicitly given by `--cacert`
curl --cacert my_server.crt --data '{"jsonrpc":"2.0", "method":"beekeeper_api.create_session", "params":{ "salt" : "banana", "notifications_endpoint" : "127.0.0.1:8001" }, "id":1}' https://127.0.0.1:6666 | jq

b) A server's certificate is put into /etc/ssl/certs/ca-certificates.crt
curl --data '{"jsonrpc":"2.0", "method":"beekeeper_api.create_session", "params":{ "salt" : "banana", "notifications_endpoint" : "127.0.0.1:8001" }, "id":1}' https://127.0.0.1:6666 | jq

WSS command line parameters:

`webserver-wss-endpoint` Local wss endpoint for webserver requests
`webserver-wss-certificate-file-name` File name with a server's certificate. Default `server.crt`.
`webserver-wss-key-file-name` File name with a server's private key. Default `server.key`.

*****WSS server*****
./beekeeper --webserver-wss-endpoint localhost:6666 --notifications-endpoint 127.0.0.1:8000 --webserver-wss-certificate-file-name /home/mario/src/server.crt --webserver-wss-key-file-name /home/mario/src/server.key

*****WSS client*****
a) connect to server( here is used `-n` switch, because there is used "self-signed certificate" in this example ):
wscat -n -c wss://127.0.0.1:6666

b) send data:
{"jsonrpc":"2.0", "method":"beekeeper_api.create_session", "params":{ "salt" : "banana", "notifications_endpoint" : "127.0.0.1:8001" }, "id":1}