class CommunicationError(Exception):
    def __init__(self, description, request, response):
        super().__init__(f'{description}.\nSent: {request}.\nReceived: {response}')
        self.request = request
        self.response = response

class MissingPathToExecutable(Exception):
    pass

class NodeIsNotRunning(Exception):
    pass

class NotSupported(Exception):
    pass

class ParseError(Exception):
    pass

class NameAlreadyInUse(Exception):
    pass
