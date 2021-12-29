#!/usr/bin/python3

import subprocess as sub
from os import mkdir, system
from shutil import rmtree
from time import perf_counter as perf
from os.path import join as joinpath
import argparse

# commented tests can't be speeded up because of incorrect calculations when boosted

Tests = [
    "list_voter_proposal_sort.py",
    "proposal_payment_test_001.py",
    "proposal_payment_test_002.py",
    "proposal_payment_test_003.py",
    "proposal_payment_test_004.py",
    "proposal_payment_test_005.py",
    "proposal_payment_test_006.py",
    "proposal_payment_test_007.py",
    "proposal_payment_test_008.py",
    "proposal_payment_test_009.py",
    "proposal_payment_test_with_governance_010.py"
]

parser = argparse.ArgumentParser()
parser.add_argument("creator", help="Account to create test accounts with")
parser.add_argument("treasury", help="Treasury account")
parser.add_argument("wif", help="Private key for creator account")
parser.add_argument("--run-hived", dest="hived_path",
                    help="Path to hived executable. Warning: using this option will erase contents of selected hived working directory.")
parser.add_argument("--working-dir", dest="hived_working_dir",
                    default="/tmp/hived_datadir_proposals_tests/", help="Path to hived working directory")
parser.add_argument("--config-path", dest="hived_config_path",
                    default="../../hive_utils/resources/config.ini.in", help="Path to source config.ini file")
parser.add_argument("--artifacts", dest="arti", default="artifacts",
                    help="Path to directory with test dumps")

args = parser.parse_args()
rmtree(args.arti, True)
mkdir(args.arti)

result = {}
ret = 0

for test in Tests:
    rmtree(args.hived_working_dir, True)
    start = perf()
    proc = None
    try:
        proc = sub.run(
            [
                "python3",
                test,
                args.creator,
                args.treasury,
                args.wif,
                "--run-hived",
                args.hived_path,
                "--working-dir",
                args.hived_working_dir,
                "--config-path",
                args.hived_config_path
            ],
            stderr=sub.PIPE,
            stdout=sub.PIPE
        )
    except:
        pass
    finally:
        stop = perf() - start
        ret += abs( 1 if proc.returncode is None else proc.returncode )
        if proc.returncode == 0:
            print(f'[SUCCESS][{stop :.2f}s] {test} passed')
        else:
            print(f'[FAIL][{stop :.2f}s] {test} failed')

        try:
            v = f"""ps -A -o pid,cmd | grep "{args.hived_working_dir}" | grep -v SCREEN | grep -v grep | grep -v python3 | cut -d '/' -f 1 | xargs -r kill -9 """
            print(v)
            system(v)
            v = f"""mv {joinpath(args.hived_working_dir, "*.log")} {joinpath(args.arti, test.replace('.py', '_hived.log'))} 2>/dev/null"""
            print(v)
            system(v)
        except:
            pass

        result[test] = {"retcode": proc.returncode, "time_s": stop}
        with open(joinpath(args.arti, f'{test}.log'), 'w') as f:
            f.write("\nstdout:\n")
            f.write(proc.stdout.decode('utf-8'))
            f.write("\nstderr:\n")
            f.write(proc.stderr.decode('utf-8'))

print('RESULTS:')
import json
print(json.dumps(result, indent=4, sort_keys=True))
exit(ret)
