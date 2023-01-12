"""
This file contains global constants closely related to the behavior of the blockchain, its configuration and specific
rules prevailing in the hive network. e.g.:
 - base accounts created at the start of the test network,
 - single block generation time,
 - number of witnesses in the schedule.

Do not place here variables strictly related to specific test cases. Their place is depending on the scope - module
or package in which they will be used. e.g.:
 - create in the test .py module - when used by tests only in this file,
 - create in proper place in the hive-local-tools - when used by tests in multiple modules/packages.
"""
