class NodeMessage:
    def __init__(self, method, params, jsonrpc, id_):
        self.__method = method
        self.__params = params
        self.__jsonrpc = jsonrpc
        self.__id = id_

    def as_json(self) -> dict:
        json_representation = {
            'jsonrpc': self.__jsonrpc,
            'id': self.__id,
            'method': self.__method,
        }

        if self.__params is not None:
            json_representation['params'] = self.__params

        return json_representation
