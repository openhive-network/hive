class NodeConfig:
    class Entry:
        def __init__(self, value, description='description'):
            self.values = {value}
            self.description = description

        def __str__(self):
            result = ' '.join(self.values)

            if self.description:
                max_length = 32

                if len(self.description) > max_length:
                    description = self.description[:max_length] + '(...)'
                else:
                    description = self.description

                result += f' # {description}'

            return result

    def __init__(self):
        self.entries = {}

    def __str__(self):
        return '\n'.join([f'{key}={str(entry)}' for key, entry in self.entries.items()])

    def __contains__(self, key):
        return key in self.entries

    def __getitem__(self, key):
        return list(self.entries[key].values)

    def add_entry(self, key, value, description=None):
        if key not in self.entries:
            self.entries[key] = self.Entry(value, description)
            return

        entry = self.entries[key]
        entry.values.add(value)

    def write_to_file(self, file_path):
        file_entries = []
        with open(file_path, 'w') as file:
            for key, entry in self.entries.items():
                file_entry = f'# {entry.description}\n' if entry.description else ''
                file_entry += f'{key} = '
                file_entry += ' '.join(entry.values)
                file_entry += '\n'

                file_entries.append(file_entry)

            file.write('\n'.join(file_entries))
