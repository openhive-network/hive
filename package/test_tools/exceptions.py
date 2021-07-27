class CommunicationError(Exception):
    def __init__(self, description, request, response):
        super().__init__(f'{description}.\nSent: {request}.\nReceived: {response}')
        self.request = request
        self.response = response

class MissingPathToExecutable(Exception):
    pass

class NodeIsNotRunning(Exception):
    pass

class NodeProcessRunFailedError(Exception):
    def __init__(self, description, return_code):
        super().__init__(description)
        self.return_code = return_code

class NotSupported(Exception):
    pass
