from pathlib import Path
import shutil


class BlockLog:
    def __init__(self, owner, path):
        self.__owner = owner
        self.__path = Path(path)

    def __repr__(self):
        return f'<BlockLog: path={self.__path}>'

    def copy_to(self, destination):
        shutil.copy(self.__path, destination)
