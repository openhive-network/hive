import os


def pytest_tavern_beta_before_every_test_run(test_dict, variables):
    if os.getenv("IS_DIRECT_CALL_HAFAH", "").lower() == "true":
        url = test_dict["stages"][0]["request"]["url"]
        method_name = test_dict["stages"][0]["request"]["json"]["method"].split(".")[1]
        test_dict["stages"][0]["request"]["url"] = f"{url}rpc/{method_name}"
        test_dict["stages"][0]["request"]["json"] = test_dict["stages"][0]["request"]["json"]["params"]
