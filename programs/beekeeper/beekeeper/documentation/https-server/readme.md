How to establish HTTPS connection?

https://stackoverflow.com/questions/60030906/self-signed-certificate-only-works-with-localhost-not-127-0-0-1

There are generated files in `example` directory. Using configuration files `root.cfg` and `server.cfg` it's possible to prepare a certificate and a private key needed for HTTPS connection. There is an example in directory for localhost and 127.0.0.1.

How to do it?

openssl req -x509 -new -keyout root.key -out root.crt -config root.cfg
openssl req -nodes -new -keyout server.key -out server.csr -config server.cfg
openssl x509 -req -in server.csr -CA root.crt -CAkey root.key -set_serial 123 -out server.crt -extfile server.cfg -extensions x509_ext

HTTPS command line parameters:

`webserver-https-endpoint` Local https endpoint for webserver requests
`webserver-https-certificate-file-name` File name with a server's certificate. Default `server.crt`.
`webserver-https-key-file-name` File name with a server's private key. Default `server.key`.

Example of connection:

*****server*****
./beekeeper --webserver-https-endpoint 127.0.0.1:6666 --webserver-https-certificate-file-name my_server.crt --webserver-https-key-file-name my_server.key

*****client*****
We have two possibilities:

a) A server's certificate is explicitly given by `--cacert`
curl --cacert my_server.crt --data '{"jsonrpc":"2.0", "method":"beekeeper_api.create_session", "params":{ "salt" : "banana" }, "id":1}' https://127.0.0.1:6666 | jq

b) A server's certificate is put into /etc/ssl/certs/ca-certificates.crt
curl --data '{"jsonrpc":"2.0", "method":"beekeeper_api.create_session", "params":{ "salt" : "banana" },"id":1}' https://127.0.0.1:6666 | jq