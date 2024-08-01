#include <boost/test/unit_test.hpp>
#include <fc/crypto/bip39.hpp>

#include <fc/crypto/sha512.hpp>
#include <fc/crypto/sha256.hpp>
#include <fc/crypto/hmac.hpp>
#include <fc/crypto/hex.hpp>
#include <fc/crypto/elliptic.hpp>
#include <fc/exception/exception.hpp>

#include <iostream>
#include <bitset>


BOOST_AUTO_TEST_SUITE(fc_crypto)
BOOST_AUTO_TEST_SUITE(bip39)

namespace {
  std::vector<unsigned char> from_hex(const std::string& hex) {
    std::vector<unsigned char> result(hex.size() / 2);
    fc::from_hex(hex, (char*)result.data(), result.size());
    return result;
  }
}

BOOST_AUTO_TEST_CASE(pbkdf2_test)
{
  // test vectors from https://github.com/brycx/Test-Vector-Generation/blob/master/PBKDF2/pbkdf2-hmac-sha2-test-vectors.md
  struct {
    std::string password;
    std::string salt;
    uint32_t iterations;
    uint32_t dk_len;
    std::vector<unsigned char> expected_output;
  } vectors[] = {
    {
      "password",
      "salt",
      1,
      20,
      from_hex("867f70cf1ade02cff3752599a3a53dc4af34c7a6")
    },
    {
      "password",
      "salt",
      2,
      20,
      from_hex("e1d9c16aa681708a45f5c7c4e215ceb66e011a2e")
    },
    {
      "password",
      "salt",
      4096,
      20,
      from_hex("d197b1b33db0143e018b12f3d1d1479e6cdebdcc")
    },
    {
      "password",
      "salt",
      16777216,
      20,
      from_hex("6180a3ceabab45cc3964112c811e0131bca93a35")
    },
    {
      "passwordPASSWORDpassword",
      "saltSALTsaltSALTsaltSALTsaltSALTsalt",
      4096,
      25,
      from_hex("8c0511f4c6e597c6ac6315d8f0362e225f3c501495ba23b868")
    },
    {
      std::string("pass\0word", 9),
      std::string("sa\0lt", 5),
      4096,
      16,
      from_hex("9d9e9c4cd21fe4be24d5b8244c759665")
    },
    {
      "passwd",
      "salt",
      1,
      128,
      from_hex("c74319d99499fc3e9013acff597c23c5baf0a0bec5634c46b8352b793e324723d55caa76b2b25c43402dcfdc06cdcf66f95b7d0429420b39520006749c51a04ef3eb99e576617395a178ba33214793e48045132928a9e9bf2661769fdc668f31798597aaf6da70dd996a81019726084d70f152baed8aafe2227c07636c6ddece")
    },
    {
      "Password",
      "NaCl",
      80000,
      128,
      from_hex("e6337d6fbeb645c794d4a9b5b75b7b30dac9ac50376a91df1f4460f6060d5addb2c1fd1f84409abacc67de7eb4056e6bb06c2d82c3ef4ccd1bded0f675ed97c65c33d39f81248454327aa6d03fd049fc5cbb2b5e6dac08e8ace996cdc960b1bd4530b7e754773d75f67a733fdb99baf6470e42ffcb753c15c352d4800fb6f9d6")
    },
    {
      "Password",
      std::string("sa\0lt", 5),
      4096,
      256,
      from_hex("10176fb32cb98cd7bb31e2bb5c8f6e425c103333a2e496058e3fd2bd88f657485c89ef92daa0668316bc23ebd1ef88f6dd14157b2320b5d54b5f26377c5dc279b1dcdec044bd6f91b166917c80e1e99ef861b1d2c7bce1b961178125fb86867f6db489a2eae0022e7bc9cf421f044319fac765d70cb89b45c214590e2ffb2c2b565ab3b9d07571fde0027b1dc57f8fd25afa842c1056dd459af4074d7510a0c020b914a5e202445d4d3f151070589dd6a2554fc506018c4f001df6239643dc86771286ae4910769d8385531bba57544d63c3640b90c98f1445ebdd129475e02086b600f0beb5b05cc6ca9b3633b452b7dad634e9336f56ec4c3ac0b4fe54ced8")
    }
  };

  for (const auto& vector : vectors)
    BOOST_CHECK(fc::pbkdf2<fc::hmac_sha512>(vector.password, vector.salt, vector.iterations, vector.dk_len) == vector.expected_output);
}

BOOST_AUTO_TEST_CASE(bip39_test)
{
  // english test vectors linked from bip39
  // https://github.com/trezor/python-mnemonic/blob/master/vectors.json
  // these only test that we can generate the correct seed from the mnemonic phrase

  struct {
    std::string input_entropy;
    std::string mnemonic;
    std::string	seed;
    std::string master_private_key;
  } vectors[] = {
    {
      "00000000000000000000000000000000",
      "abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon about",
      "c55257c360c07c72029aebc1b53c05ed0362ada38ead3e3e9efa3708e53495531f09a6987599d18264c1e1c92f2cf141630c7a3c4ab7c81b2f001698e7463b04",
      "xprv9s21ZrQH143K3h3fDYiay8mocZ3afhfULfb5GX8kCBdno77K4HiA15Tg23wpbeF1pLfs1c5SPmYHrEpTuuRhxMwvKDwqdKiGJS9XFKzUsAF"
    },
    {
      "7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f",
      "legal winner thank year wave sausage worth useful legal winner thank yellow",
      "2e8905819b8723fe2c1d161860e5ee1830318dbf49a83bd451cfb8440c28bd6fa457fe1296106559a3c80937a1c1069be3a3a5bd381ee6260e8d9739fce1f607",
      "xprv9s21ZrQH143K2gA81bYFHqU68xz1cX2APaSq5tt6MFSLeXnCKV1RVUJt9FWNTbrrryem4ZckN8k4Ls1H6nwdvDTvnV7zEXs2HgPezuVccsq"
    },
    {
      "80808080808080808080808080808080",
      "letter advice cage absurd amount doctor acoustic avoid letter advice cage above",
      "d71de856f81a8acc65e6fc851a38d4d7ec216fd0796d0a6827a3ad6ed5511a30fa280f12eb2e47ed2ac03b5c462a0358d18d69fe4f985ec81778c1b370b652a8",
      "xprv9s21ZrQH143K2shfP28KM3nr5Ap1SXjz8gc2rAqqMEynmjt6o1qboCDpxckqXavCwdnYds6yBHZGKHv7ef2eTXy461PXUjBFQg6PrwY4Gzq"
    },
    {
      "ffffffffffffffffffffffffffffffff",
      "zoo zoo zoo zoo zoo zoo zoo zoo zoo zoo zoo wrong",
      "ac27495480225222079d7be181583751e86f571027b0497b5b5d11218e0a8a13332572917f0f8e5a589620c6f15b11c61dee327651a14c34e18231052e48c069",
      "xprv9s21ZrQH143K2V4oox4M8Zmhi2Fjx5XK4Lf7GKRvPSgydU3mjZuKGCTg7UPiBUD7ydVPvSLtg9hjp7MQTYsW67rZHAXeccqYqrsx8LcXnyd"
    },
    {
      "000000000000000000000000000000000000000000000000",
      "abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon agent",
      "035895f2f481b1b0f01fcf8c289c794660b289981a78f8106447707fdd9666ca06da5a9a565181599b79f53b844d8a71dd9f439c52a3d7b3e8a79c906ac845fa",
      "xprv9s21ZrQH143K3mEDrypcZ2usWqFgzKB6jBBx9B6GfC7fu26X6hPRzVjzkqkPvDqp6g5eypdk6cyhGnBngbjeHTe4LsuLG1cCmKJka5SMkmU"
    },
    {
      "7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f",
      "legal winner thank year wave sausage worth useful legal winner thank year wave sausage worth useful legal will",
      "f2b94508732bcbacbcc020faefecfc89feafa6649a5491b8c952cede496c214a0c7b3c392d168748f2d4a612bada0753b52a1c7ac53c1e93abd5c6320b9e95dd",
      "xprv9s21ZrQH143K3Lv9MZLj16np5GzLe7tDKQfVusBni7toqJGcnKRtHSxUwbKUyUWiwpK55g1DUSsw76TF1T93VT4gz4wt5RM23pkaQLnvBh7"
    },
    {
      "808080808080808080808080808080808080808080808080",
      "letter advice cage absurd amount doctor acoustic avoid letter advice cage absurd amount doctor acoustic avoid letter always",
      "107d7c02a5aa6f38c58083ff74f04c607c2d2c0ecc55501dadd72d025b751bc27fe913ffb796f841c49b1d33b610cf0e91d3aa239027f5e99fe4ce9e5088cd65",
      "xprv9s21ZrQH143K3VPCbxbUtpkh9pRG371UCLDz3BjceqP1jz7XZsQ5EnNkYAEkfeZp62cDNj13ZTEVG1TEro9sZ9grfRmcYWLBhCocViKEJae"
    },
    {
      "ffffffffffffffffffffffffffffffffffffffffffffffff",
      "zoo zoo zoo zoo zoo zoo zoo zoo zoo zoo zoo zoo zoo zoo zoo zoo zoo when",
      "0cd6e5d827bb62eb8fc1e262254223817fd068a74b5b449cc2f667c3f1f985a76379b43348d952e2265b4cd129090758b3e3c2c49103b5051aac2eaeb890a528",
      "xprv9s21ZrQH143K36Ao5jHRVhFGDbLP6FCx8BEEmpru77ef3bmA928BxsqvVM27WnvvyfWywiFN8K6yToqMaGYfzS6Db1EHAXT5TuyCLBXUfdm"
    },
    {
      "0000000000000000000000000000000000000000000000000000000000000000",
      "abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon art",
      "bda85446c68413707090a52022edd26a1c9462295029f2e60cd7c4f2bbd3097170af7a4d73245cafa9c3cca8d561a7c3de6f5d4a10be8ed2a5e608d68f92fcc8",
      "xprv9s21ZrQH143K32qBagUJAMU2LsHg3ka7jqMcV98Y7gVeVyNStwYS3U7yVVoDZ4btbRNf4h6ibWpY22iRmXq35qgLs79f312g2kj5539ebPM"
    },
    {
      "7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f",
      "legal winner thank year wave sausage worth useful legal winner thank year wave sausage worth useful legal winner thank year wave sausage worth title",
      "bc09fca1804f7e69da93c2f2028eb238c227f2e9dda30cd63699232578480a4021b146ad717fbb7e451ce9eb835f43620bf5c514db0f8add49f5d121449d3e87",
      "xprv9s21ZrQH143K3Y1sd2XVu9wtqxJRvybCfAetjUrMMco6r3v9qZTBeXiBZkS8JxWbcGJZyio8TrZtm6pkbzG8SYt1sxwNLh3Wx7to5pgiVFU"
    },
    {
      "8080808080808080808080808080808080808080808080808080808080808080",
      "letter advice cage absurd amount doctor acoustic avoid letter advice cage absurd amount doctor acoustic avoid letter advice cage absurd amount doctor acoustic bless",
      "c0c519bd0e91a2ed54357d9d1ebef6f5af218a153624cf4f2da911a0ed8f7a09e2ef61af0aca007096df430022f7a2b6fb91661a9589097069720d015e4e982f",
      "xprv9s21ZrQH143K3CSnQNYC3MqAAqHwxeTLhDbhF43A4ss4ciWNmCY9zQGvAKUSqVUf2vPHBTSE1rB2pg4avopqSiLVzXEU8KziNnVPauTqLRo"
    },
    {
      "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff",
      "zoo zoo zoo zoo zoo zoo zoo zoo zoo zoo zoo zoo zoo zoo zoo zoo zoo zoo zoo zoo zoo zoo zoo vote",
      "dd48c104698c30cfe2b6142103248622fb7bb0ff692eebb00089b32d22484e1613912f0a5b694407be899ffd31ed3992c456cdf60f5d4564b8ba3f05a69890ad",
      "xprv9s21ZrQH143K2WFF16X85T2QCpndrGwx6GueB72Zf3AHwHJaknRXNF37ZmDrtHrrLSHvbuRejXcnYxoZKvRquTPyp2JiNG3XcjQyzSEgqCB"
    },
    {
      "9e885d952ad362caeb4efe34a8e91bd2",
      "ozone drill grab fiber curtain grace pudding thank cruise elder eight picnic",
      "274ddc525802f7c828d8ef7ddbcdc5304e87ac3535913611fbbfa986d0c9e5476c91689f9c8a54fd55bd38606aa6a8595ad213d4c9c9f9aca3fb217069a41028",
      "xprv9s21ZrQH143K2oZ9stBYpoaZ2ktHj7jLz7iMqpgg1En8kKFTXJHsjxry1JbKH19YrDTicVwKPehFKTbmaxgVEc5TpHdS1aYhB2s9aFJBeJH"
    },
    {
      "6610b25967cdcca9d59875f5cb50b0ea75433311869e930b",
      "gravity machine north sort system female filter attitude volume fold club stay feature office ecology stable narrow fog",
      "628c3827a8823298ee685db84f55caa34b5cc195a778e52d45f59bcf75aba68e4d7590e101dc414bc1bbd5737666fbbef35d1f1903953b66624f910feef245ac",
      "xprv9s21ZrQH143K3uT8eQowUjsxrmsA9YUuQQK1RLqFufzybxD6DH6gPY7NjJ5G3EPHjsWDrs9iivSbmvjc9DQJbJGatfa9pv4MZ3wjr8qWPAK"
    },
    {
      "68a79eaca2324873eacc50cb9c6eca8cc68ea5d936f98787c60c7ebc74e6ce7c",
      "hamster diagram private dutch cause delay private meat slide toddler razor book happy fancy gospel tennis maple dilemma loan word shrug inflict delay length",
      "64c87cde7e12ecf6704ab95bb1408bef047c22db4cc7491c4271d170a1b213d20b385bc1588d9c7b38f1b39d415665b8a9030c9ec653d75e65f847d8fc1fc440",
      "xprv9s21ZrQH143K2XTAhys3pMNcGn261Fi5Ta2Pw8PwaVPhg3D8DWkzWQwjTJfskj8ofb81i9NP2cUNKxwjueJHHMQAnxtivTA75uUFqPFeWzk"
    },
    {
      "c0ba5a8e914111210f2bd131f3d5e08d",
      "scheme spot photo card baby mountain device kick cradle pact join borrow",
      "ea725895aaae8d4c1cf682c1bfd2d358d52ed9f0f0591131b559e2724bb234fca05aa9c02c57407e04ee9dc3b454aa63fbff483a8b11de949624b9f1831a9612",
      "xprv9s21ZrQH143K3FperxDp8vFsFycKCRcJGAFmcV7umQmcnMZaLtZRt13QJDsoS5F6oYT6BB4sS6zmTmyQAEkJKxJ7yByDNtRe5asP2jFGhT6"
    },
    {
      "6d9be1ee6ebd27a258115aad99b7317b9c8d28b6d76431c3",
      "horn tenant knee talent sponsor spell gate clip pulse soap slush warm silver nephew swap uncle crack brave",
      "fd579828af3da1d32544ce4db5c73d53fc8acc4ddb1e3b251a31179cdb71e853c56d2fcb11aed39898ce6c34b10b5382772db8796e52837b54468aeb312cfc3d",
      "xprv9s21ZrQH143K3R1SfVZZLtVbXEB9ryVxmVtVMsMwmEyEvgXN6Q84LKkLRmf4ST6QrLeBm3jQsb9gx1uo23TS7vo3vAkZGZz71uuLCcywUkt"
    },
    {
      "9f6a2878b2520799a44ef18bc7df394e7061a224d2c33cd015b157d746869863",
      "panda eyebrow bullet gorilla call smoke muffin taste mesh discover soft ostrich alcohol speed nation flash devote level hobby quick inner drive ghost inside",
      "72be8e052fc4919d2adf28d5306b5474b0069df35b02303de8c1729c9538dbb6fc2d731d5f832193cd9fb6aeecbc469594a70e3dd50811b5067f3b88b28c3e8d",
      "xprv9s21ZrQH143K2WNnKmssvZYM96VAr47iHUQUTUyUXH3sAGNjhJANddnhw3i3y3pBbRAVk5M5qUGFr4rHbEWwXgX4qrvrceifCYQJbbFDems"
    },
    {
      "23db8160a31d3e0dca3688ed941adbf3",
      "cat swing flag economy stadium alone churn speed unique patch report train",
      "deb5f45449e615feff5640f2e49f933ff51895de3b4381832b3139941c57b59205a42480c52175b6efcffaa58a2503887c1e8b363a707256bdd2b587b46541f5",
      "xprv9s21ZrQH143K4G28omGMogEoYgDQuigBo8AFHAGDaJdqQ99QKMQ5J6fYTMfANTJy6xBmhvsNZ1CJzRZ64PWbnTFUn6CDV2FxoMDLXdk95DQ"
    },
    {
      "8197a4a47f0425faeaa69deebc05ca29c0a5b5cc76ceacc0",
      "light rule cinnamon wrap drastic word pride squirrel upgrade then income fatal apart sustain crack supply proud access",
      "4cbdff1ca2db800fd61cae72a57475fdc6bab03e441fd63f96dabd1f183ef5b782925f00105f318309a7e9c3ea6967c7801e46c8a58082674c860a37b93eda02",
      "xprv9s21ZrQH143K3wtsvY8L2aZyxkiWULZH4vyQE5XkHTXkmx8gHo6RUEfH3Jyr6NwkJhvano7Xb2o6UqFKWHVo5scE31SGDCAUsgVhiUuUDyh"
    },
    {
      "066dca1a2bb7e8a1db2832148ce9933eea0f3ac9548d793112d9a95c9407efad",
      "all hour make first leader extend hole alien behind guard gospel lava path output census museum junior mass reopen famous sing advance salt reform",
      "26e975ec644423f4a4c4f4215ef09b4bd7ef924e85d1d17c4cf3f136c2863cf6df0a475045652c57eb5fb41513ca2a2d67722b77e954b4b3fc11f7590449191d",
      "xprv9s21ZrQH143K3rEfqSM4QZRVmiMuSWY9wugscmaCjYja3SbUD3KPEB1a7QXJoajyR2T1SiXU7rFVRXMV9XdYVSZe7JoUXdP4SRHTxsT1nzm"
    },
    {
      "f30f8c1da665478f49b001d94c5fc452",
      "vessel ladder alter error federal sibling chat ability sun glass valve picture",
      "2aaa9242daafcee6aa9d7269f17d4efe271e1b9a529178d7dc139cd18747090bf9d60295d0ce74309a78852a9caadf0af48aae1c6253839624076224374bc63f",
      "xprv9s21ZrQH143K2QWV9Wn8Vvs6jbqfF1YbTCdURQW9dLFKDovpKaKrqS3SEWsXCu6ZNky9PSAENg6c9AQYHcg4PjopRGGKmdD313ZHszymnps"
    },
    {
      "c10ec20dc3cd9f652c7fac2f1230f7a3c828389a14392f05",
      "scissors invite lock maple supreme raw rapid void congress muscle digital elegant little brisk hair mango congress clump",
      "7b4a10be9d98e6cba265566db7f136718e1398c71cb581e1b2f464cac1ceedf4f3e274dc270003c670ad8d02c4558b2f8e39edea2775c9e232c7cb798b069e88",
      "xprv9s21ZrQH143K4aERa2bq7559eMCCEs2QmmqVjUuzfy5eAeDX4mqZffkYwpzGQRE2YEEeLVRoH4CSHxianrFaVnMN2RYaPUZJhJx8S5j6puX"
    },
    {
      "f585c11aec520db57dd353c69554b21a89b20fb0650966fa0a9d6f74fd989d8f",
      "void come effort suffer camp survey warrior heavy shoot primary clutch crush open amazing screen patrol group space point ten exist slush involve unfold",
      "01f5bced59dec48e362f2c45b5de68b9fd6c92c6634f44d6d40aab69056506f0e35524a518034ddc1192e1dacd32c1ed3eaa3c3b131c88ed8e7e54c49a5d0998",
      "xprv9s21ZrQH143K39rnQJknpH1WEPFJrzmAqqasiDcVrNuk926oizzJDDQkdiTvNPr2FYDYzWgiMiC63YmfPAa2oPyNB23r2g7d1yiK6WpqaQS"
    }
  };

  for (const auto& vector : vectors)
  {
    // test that we generate the correct mnemonic given the entropy
    BOOST_CHECK_EQUAL(fc::generate_bip39_mnemonic(from_hex(vector.input_entropy)), vector.mnemonic);

    // then that we convert the mnemonic to the correct seed
    // and then that we convert that seed into the correct master private key
    fc::ecc::extended_private_key master_seed = fc::bip39_mnemonic_to_extended_private_key(vector.mnemonic, "TREZOR");
    BOOST_CHECK_EQUAL(master_seed.str(), vector.master_private_key);
  }
}

// Test vectors from BIP-32
BOOST_AUTO_TEST_CASE(test_extended_keys_1)
{
  const std::string TEST1_SEED = "000102030405060708090a0b0c0d0e0f";
  const std::string TEST1_M_PUB = "xpub661MyMwAqRbcFtXgS5sYJABqqG9YLmC4Q1Rdap9gSE8NqtwybGhePY2gZ29ESFjqJoCu1Rupje8YtGqsefD265TMg7usUDFdp6W1EGMcet8";
  const std::string TEST1_M_PRIV = "xprv9s21ZrQH143K3QTDL4LXw2F7HEK3wJUD2nW2nRk4stbPy6cq3jPPqjiChkVvvNKmPGJxWUtg6LnF5kejMRNNU3TGtRBeJgk33yuGBxrMPHi";
  const std::string TEST1_M_0H_PUB = "xpub68Gmy5EdvgibQVfPdqkBBCHxA5htiqg55crXYuXoQRKfDBFA1WEjWgP6LHhwBZeNK1VTsfTFUHCdrfp1bgwQ9xv5ski8PX9rL2dZXvgGDnw";
  const std::string TEST1_M_0H_PRIV = "xprv9uHRZZhk6KAJC1avXpDAp4MDc3sQKNxDiPvvkX8Br5ngLNv1TxvUxt4cV1rGL5hj6KCesnDYUhd7oWgT11eZG7XnxHrnYeSvkzY7d2bhkJ7";
  const std::string TEST1_M_0H_1_PUB = "xpub6ASuArnXKPbfEwhqN6e3mwBcDTgzisQN1wXN9BJcM47sSikHjJf3UFHKkNAWbWMiGj7Wf5uMash7SyYq527Hqck2AxYysAA7xmALppuCkwQ";
  const std::string TEST1_M_0H_1_PRIV = "xprv9wTYmMFdV23N2TdNG573QoEsfRrWKQgWeibmLntzniatZvR9BmLnvSxqu53Kw1UmYPxLgboyZQaXwTCg8MSY3H2EU4pWcQDnRnrVA1xe8fs";
  const std::string TEST1_M_0H_1_2H_PUB = "xpub6D4BDPcP2GT577Vvch3R8wDkScZWzQzMMUm3PWbmWvVJrZwQY4VUNgqFJPMM3No2dFDFGTsxxpG5uJh7n7epu4trkrX7x7DogT5Uv6fcLW5";
  const std::string TEST1_M_0H_1_2H_PRIV = "xprv9z4pot5VBttmtdRTWfWQmoH1taj2axGVzFqSb8C9xaxKymcFzXBDptWmT7FwuEzG3ryjH4ktypQSAewRiNMjANTtpgP4mLTj34bhnZX7UiM";
  const std::string TEST1_M_0H_1_2H_2_PUB = "xpub6FHa3pjLCk84BayeJxFW2SP4XRrFd1JYnxeLeU8EqN3vDfZmbqBqaGJAyiLjTAwm6ZLRQUMv1ZACTj37sR62cfN7fe5JnJ7dh8zL4fiyLHV";
  const std::string TEST1_M_0H_1_2H_2_PRIV = "xprvA2JDeKCSNNZky6uBCviVfJSKyQ1mDYahRjijr5idH2WwLsEd4Hsb2Tyh8RfQMuPh7f7RtyzTtdrbdqqsunu5Mm3wDvUAKRHSC34sJ7in334";
  const std::string TEST1_M_0H_1_2H_2_1g_PUB = "xpub6H1LXWLaKsWFhvm6RVpEL9P4KfRZSW7abD2ttkWP3SSQvnyA8FSVqNTEcYFgJS2UaFcxupHiYkro49S8yGasTvXEYBVPamhGW6cFJodrTHy";
  const std::string TEST1_M_0H_1_2H_2_1g_PRIV = "xprvA41z7zogVVwxVSgdKUHDy1SKmdb533PjDz7J6N6mV6uS3ze1ai8FHa8kmHScGpWmj4WggLyQjgPie1rFSruoUihUZREPSL39UNdE3BBDu76";

  char seed[16];
  fc::from_hex(TEST1_SEED, seed, sizeof(seed));
  fc::ecc::extended_private_key master = fc::ecc::extended_private_key::generate_master(seed, sizeof(seed));
  BOOST_CHECK_EQUAL(master.str(), TEST1_M_PRIV);
  BOOST_CHECK_EQUAL(master.get_extended_public_key().str(), TEST1_M_PUB);

  BOOST_CHECK_EQUAL(fc::ecc::extended_private_key::from_base58(TEST1_M_PRIV).str(), TEST1_M_PRIV);
  BOOST_CHECK_EQUAL(fc::ecc::extended_public_key::from_base58(TEST1_M_PUB).str(), TEST1_M_PUB);
  BOOST_CHECK_EQUAL(fc::ecc::extended_private_key::from_base58(TEST1_M_0H_PRIV).str(), TEST1_M_0H_PRIV);
  BOOST_CHECK_EQUAL(fc::ecc::extended_public_key::from_base58(TEST1_M_0H_PUB).str(), TEST1_M_0H_PUB);

  fc::ecc::extended_private_key m_0 = master.derive_child(0x80000000);
  BOOST_CHECK_EQUAL(m_0.str(), TEST1_M_0H_PRIV);
  BOOST_CHECK_EQUAL(m_0.get_extended_public_key().str(), TEST1_M_0H_PUB);

  fc::ecc::extended_private_key m_0_1 = m_0.derive_child(1);
  BOOST_CHECK_EQUAL(m_0_1.str(), TEST1_M_0H_1_PRIV);
  BOOST_CHECK_EQUAL(m_0_1.get_extended_public_key().str(), TEST1_M_0H_1_PUB);
  BOOST_CHECK_EQUAL(m_0.get_extended_public_key().derive_child(1).str(), TEST1_M_0H_1_PUB);

  fc::ecc::extended_private_key m_0_1_2 = m_0_1.derive_child(0x80000002);
  BOOST_CHECK_EQUAL(m_0_1_2.str(), TEST1_M_0H_1_2H_PRIV);
  BOOST_CHECK_EQUAL(m_0_1_2.get_extended_public_key().str(), TEST1_M_0H_1_2H_PUB);

  fc::ecc::extended_private_key m_0_1_2_2 = m_0_1_2.derive_child(2);
  BOOST_CHECK_EQUAL(m_0_1_2_2.str(), TEST1_M_0H_1_2H_2_PRIV);
  BOOST_CHECK_EQUAL(m_0_1_2_2.get_extended_public_key().str(), TEST1_M_0H_1_2H_2_PUB);
  BOOST_CHECK_EQUAL(m_0_1_2.get_extended_public_key().derive_child(2).str(), TEST1_M_0H_1_2H_2_PUB);

  fc::ecc::extended_private_key m_0_1_2_2_1g = m_0_1_2_2.derive_child(1000000000);
  BOOST_CHECK_EQUAL(m_0_1_2_2_1g.str(), TEST1_M_0H_1_2H_2_1g_PRIV);
  BOOST_CHECK_EQUAL(m_0_1_2_2_1g.get_extended_public_key().str(), TEST1_M_0H_1_2H_2_1g_PUB);
  BOOST_CHECK_EQUAL(m_0_1_2_2.get_extended_public_key().derive_child(1000000000).str(), TEST1_M_0H_1_2H_2_1g_PUB);
}

BOOST_AUTO_TEST_CASE(test_extended_keys_2)
{
  const std::string TEST2_SEED = "fffcf9f6f3f0edeae7e4e1dedbd8d5d2cfccc9c6c3c0bdbab7b4b1aeaba8a5a29f9c999693908d8a8784817e7b7875726f6c696663605d5a5754514e4b484542";
  const std::string TEST2_M_PUB = "xpub661MyMwAqRbcFW31YEwpkMuc5THy2PSt5bDMsktWQcFF8syAmRUapSCGu8ED9W6oDMSgv6Zz8idoc4a6mr8BDzTJY47LJhkJ8UB7WEGuduB";
  const std::string TEST2_M_PRIV = "xprv9s21ZrQH143K31xYSDQpPDxsXRTUcvj2iNHm5NUtrGiGG5e2DtALGdso3pGz6ssrdK4PFmM8NSpSBHNqPqm55Qn3LqFtT2emdEXVYsCzC2U";
  const std::string TEST2_M_0_PUB = "xpub69H7F5d8KSRgmmdJg2KhpAK8SR3DjMwAdkxj3ZuxV27CprR9LgpeyGmXUbC6wb7ERfvrnKZjXoUmmDznezpbZb7ap6r1D3tgFxHmwMkQTPH";
  const std::string TEST2_M_0_PRIV = "xprv9vHkqa6EV4sPZHYqZznhT2NPtPCjKuDKGY38FBWLvgaDx45zo9WQRUT3dKYnjwih2yJD9mkrocEZXo1ex8G81dwSM1fwqWpWkeS3v86pgKt";
  const std::string TEST2_M_0_m1_PUB = "xpub6ASAVgeehLbnwdqV6UKMHVzgqAG8Gr6riv3Fxxpj8ksbH9ebxaEyBLZ85ySDhKiLDBrQSARLq1uNRts8RuJiHjaDMBU4Zn9h8LZNnBC5y4a";
  const std::string TEST2_M_0_m1_PRIV = "xprv9wSp6B7kry3Vj9m1zSnLvN3xH8RdsPP1Mh7fAaR7aRLcQMKTR2vidYEeEg2mUCTAwCd6vnxVrcjfy2kRgVsFawNzmjuHc2YmYRmagcEPdU9";
  const std::string TEST2_M_0_m1_1_PUB = "xpub6DF8uhdarytz3FWdA8TvFSvvAh8dP3283MY7p2V4SeE2wyWmG5mg5EwVvmdMVCQcoNJxGoWaU9DCWh89LojfZ537wTfunKau47EL2dhHKon";
  const std::string TEST2_M_0_m1_1_PRIV = "xprv9zFnWC6h2cLgpmSA46vutJzBcfJ8yaJGg8cX1e5StJh45BBciYTRXSd25UEPVuesF9yog62tGAQtHjXajPPdbRCHuWS6T8XA2ECKADdw4Ef";
  const std::string TEST2_M_0_m1_1_m2_PUB = "xpub6ERApfZwUNrhLCkDtcHTcxd75RbzS1ed54G1LkBUHQVHQKqhMkhgbmJbZRkrgZw4koxb5JaHWkY4ALHY2grBGRjaDMzQLcgJvLJuZZvRcEL";
  const std::string TEST2_M_0_m1_1_m2_PRIV = "xprvA1RpRA33e1JQ7ifknakTFpgNXPmW2YvmhqLQYMmrj4xJXXWYpDPS3xz7iAxn8L39njGVyuoseXzU6rcxFLJ8HFsTjSyQbLYnMpCqE2VbFWc";
  const std::string TEST2_M_0_m1_1_m2_2_PUB = "xpub6FnCn6nSzZAw5Tw7cgR9bi15UV96gLZhjDstkXXxvCLsUXBGXPdSnLFbdpq8p9HmGsApME5hQTZ3emM2rnY5agb9rXpVGyy3bdW6EEgAtqt";
  const std::string TEST2_M_0_m1_1_m2_2_PRIV = "xprvA2nrNbFZABcdryreWet9Ea4LvTJcGsqrMzxHx98MMrotbir7yrKCEXw7nadnHM8Dq38EGfSh6dqA9QWTyefMLEcBYJUuekgW4BYPJcr9E7j";

  char seed[64];
  fc::from_hex(TEST2_SEED, seed, sizeof(seed));
  fc::ecc::extended_private_key master = fc::ecc::extended_private_key::generate_master(seed, sizeof(seed));
  BOOST_CHECK_EQUAL(master.str(), TEST2_M_PRIV);
  BOOST_CHECK_EQUAL(master.get_extended_public_key().str(), TEST2_M_PUB);

  fc::ecc::extended_private_key m_0 = master.derive_child(0);
  BOOST_CHECK_EQUAL(m_0.str(), TEST2_M_0_PRIV);
  BOOST_CHECK_EQUAL(m_0.get_extended_public_key().str(), TEST2_M_0_PUB);
  BOOST_CHECK_EQUAL(master.get_extended_public_key().derive_child(0).str(), TEST2_M_0_PUB);

  fc::ecc::extended_private_key m_0_m1 = m_0.derive_child(-1);
  BOOST_CHECK_EQUAL(m_0_m1.str(), TEST2_M_0_m1_PRIV);
  BOOST_CHECK_EQUAL(m_0_m1.get_extended_public_key().str(), TEST2_M_0_m1_PUB);

  fc::ecc::extended_private_key m_0_m1_1 = m_0_m1.derive_child(1);
  BOOST_CHECK_EQUAL(m_0_m1_1.str(), TEST2_M_0_m1_1_PRIV);
  BOOST_CHECK_EQUAL(m_0_m1_1.get_extended_public_key().str(), TEST2_M_0_m1_1_PUB);
  BOOST_CHECK_EQUAL(m_0_m1.get_extended_public_key().derive_child(1).str(), TEST2_M_0_m1_1_PUB);

  fc::ecc::extended_private_key m_0_m1_1_m2 = m_0_m1_1.derive_child(-2);
  BOOST_CHECK_EQUAL(m_0_m1_1_m2.str(), TEST2_M_0_m1_1_m2_PRIV);
  BOOST_CHECK_EQUAL(m_0_m1_1_m2.get_extended_public_key().str(), TEST2_M_0_m1_1_m2_PUB);

  fc::ecc::extended_private_key m_0_m1_1_m2_2 = m_0_m1_1_m2.derive_child(2);
  BOOST_CHECK_EQUAL(m_0_m1_1_m2_2.str(), TEST2_M_0_m1_1_m2_2_PRIV);
  BOOST_CHECK_EQUAL(m_0_m1_1_m2_2.get_extended_public_key().str(), TEST2_M_0_m1_1_m2_2_PUB);
  BOOST_CHECK_EQUAL(m_0_m1_1_m2.get_extended_public_key().derive_child(2).str(), TEST2_M_0_m1_1_m2_2_PUB);
}

BOOST_AUTO_TEST_SUITE_END()
BOOST_AUTO_TEST_SUITE_END()
