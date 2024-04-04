A file `wallet-0.wallet` is an excellent example that shows a case that `fc::aes_decrypt` call from `beekeeper_wallet::unlock` method passes for a wrong password.
Details:

`wallet-0.wallet` contains 1 private key. The password for this wallet is: `password-0`
Unfortunately the password `WRONG_PASSWORD` doesn't raise an exception in `fc::aes_decrypt` function.

Solution:
Before wallets processing it's necessary to check a checksum earlier. Otherwise it is undefined behaviour in further parts of code.

