# Replay Based Tests

#### Note

##### All python files in this directory, will be started after 3.5 million block replay will finish

---

## Flag requirements

- ### `--run-hived <address:port>`

address and port to currently running node

#### Example

    python3 sample_test.py --run-hived 127.0.0.1:8090

- ### `--path-to-config <path>`

path to `config.ini` file of currently running node

#### Example

    python3 sample_test.py --path-to-config /home/user/datadir/config.ini
