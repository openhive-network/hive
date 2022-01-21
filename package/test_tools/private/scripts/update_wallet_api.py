import builtins
import keyword
from pathlib import Path
import re
import shutil
from typing import List

from test_tools import Wallet


class Parameter:
    def __init__(self, name, *, type_):
        self.name: str = name
        self.type: str = type_

    def __repr__(self):
        return f'Parameter(name=\'{self.name}\', type_=\'{self.type}\')'


class Method:
    def __init__(self, name, *, return_value, parameters):
        self.name: str = name
        self.return_value: str = return_value
        self.parameters: List[Parameter] = parameters

    def __repr__(self):
        return f'Method(name=\'{self.name}\', return_value=\'{self.return_value}\', parameters={self.parameters})'


class WalletApiTranslator:
    """Translates C++ wallet API, retrieved from wallet help, to Python."""

    TYPE_TRANSLATIONS = {
    }

    RESTRICTED_NAMES = {*keyword.kwlist, *dir(builtins)}

    def __init__(self):
        self.api_methods: List[Method] = []

    def parse_wallet_methods(self) -> None:
        wallet = Wallet()
        method_signatures = wallet.api.help()['result'].split('\n')[:-1]
        wallet_directory = wallet.directory
        wallet.close()

        # Temporary workaround for missing configurable cleanup policy for wallets
        shutil.rmtree(wallet_directory, ignore_errors=True)

        for signature in method_signatures:
            method_match_result = re.match(r'\s*(.+) ([\w_]+)\((.*)\)', signature)

            method = Method(name=method_match_result[2], return_value=method_match_result[1], parameters=[])

            for parameter in method_match_result[3].split(', '):
                if parameter == '':
                    continue

                parameter_match_result = re.match(r'(.*) ([\w_]+)', parameter)
                method.parameters.append(
                    Parameter(
                        name=parameter_match_result[2],
                        type_=parameter_match_result[1],
                    )
                )

            self.api_methods.append(method)

    def generate_python_api(self) -> List[str]:
        api = []
        for method in self.api_methods:
            api.extend([
                f'def {method.name}(self{self.__generate_python_method_parameters(method)}):',
                f'    return self.__send(\'{method.name}\'{self.__generate_python_call_parameters(method)})',
                '',
            ])
        return api[:-1]

    @classmethod
    def __generate_python_method_parameters(cls, method: Method) -> str:
        """
        Example:
            ['int i', 'bool b', 'std::string type'] -> ', i: int, b: bool, type_: str'
            [] -> ''
        """

        result = ''
        for parameter in method.parameters:
            result += f', {cls.__sanitize_name(parameter.name)}'

            translated_type = cls.__translate_type(parameter.type)
            if translated_type is not None:
                result += f': {translated_type}'

            # Handle special case with 'broadcast' parameter
            if parameter.name == 'broadcast':
                result += '=None'

        # Append 'only_result' parameter
        result += ', only_result: bool = True'

        return result

    @classmethod
    def __sanitize_name(cls, name: str) -> str:
        return f'{name}_' if name in cls.RESTRICTED_NAMES else name

    @classmethod
    def __translate_type(cls, type_: str) -> str:
        return cls.TYPE_TRANSLATIONS[type_] if type_ in cls.TYPE_TRANSLATIONS else None

    @classmethod
    def __generate_python_call_parameters(cls, method: Method) -> str:
        """
        Example:
            ['int i', 'bool b', 'std::string type'] -> ', i=i, b=b, type=type_'
            [] -> ''
        """

        result = ''
        for parameter in method.parameters:
            sanitized_name = cls.__sanitize_name(parameter.name)
            result += f', {sanitized_name}={sanitized_name}'

        # Append 'only_result' parameter
        result += ', only_result=only_result'

        return result


def replace_file_content_between_tags(file_path: Path, new_content: List[str], begin_tag: str, end_tag: str) -> None:
    with open(file_path, 'r', encoding='utf-8') as file:
        file_content = file.read().split('\n')

    indentation = ''
    begin_tag_index, end_tag_index = None, None
    for line_index, line_content in enumerate(file_content):
        if begin_tag in line_content:
            begin_tag_index = line_index
            indentation = line_content.find(begin_tag) * ' '
        elif end_tag in line_content:
            if begin_tag_index is None:
                raise RuntimeError('End tag found before begin tag')

            end_tag_index = line_index
            break
    else:
        raise RuntimeError('Begin and end tags were not found')

    file_content[begin_tag_index + 1:end_tag_index] = [indentation + line if line else line for line in new_content]

    with open(file_path, 'w', encoding='utf-8') as file:
        file.write('\n'.join(file_content))


if __name__ == '__main__':
    translator = WalletApiTranslator()
    translator.parse_wallet_methods()
    python_api = translator.generate_python_api()

    import test_tools.wallet
    replace_file_content_between_tags(
        file_path=Path(test_tools.wallet.__file__),
        new_content=python_api,
        begin_tag='# Begin of machine generated code',
        end_tag='# End of machine generated code',
    )
