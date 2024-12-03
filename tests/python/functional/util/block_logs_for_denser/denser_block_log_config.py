from __future__ import annotations

import test_tools as tt


class CommunityDetails:
    class Roles:
        def __init__(self, admin=None, guest=None, member=None, mod=None, muted=None) -> None:
            self.admin: list[str] | None = admin
            self.guest: list[str] | None = guest
            self.member: list[str] | None = member
            self.mod: list[str] | None = mod
            self.muted: list[str] | None = muted

    def __init__(
        self,
        owner,
        title,
        about,
        description,
        flag_text="",
        is_nsfw=False,
        subs=None,
        roles=None,
        posts_number=0,
        pinned_posts=0,
    ) -> None:
        if subs is None:
            subs = []
        self.owner: str = owner
        self.title: str = title
        self.about: str = about
        self.is_nsfw: bool = is_nsfw
        self.flag_text: str = flag_text
        self.description: str = description
        self.subs: list[str] = subs
        self.roles: CommunityDetails.Roles = roles
        self.posts_number: int = posts_number
        self.pinned_posts: int = pinned_posts


ACCOUNT_DETAILS = [
    ("denserautotest0", {}),
    ("denserautotest1", {"transfer": [tt.Asset.Hive(1), tt.Asset.Hbd(1)], "vesting": tt.Asset.Hive(1)}),
    ("denserautotest2", {"transfer": [tt.Asset.Hive(10)]}),
    ("denserautotest3", {"transfer": [tt.Asset.Hive(1000), tt.Asset.Hbd(100)], "vesting": tt.Asset.Hive(1000)}),
    ("denserautotest4", {"transfer": [tt.Asset.Hive(100), tt.Asset.Hbd(100)], "vesting": tt.Asset.Hive(100)}),
    *(
        (f"denserautotest-{i}", {"transfer": [tt.Asset.Hive(1), tt.Asset.Hbd(1)], "vesting": tt.Asset.Hive(1)})
        for i in range(5, 10)
    ),
]

COMMUNITY_DETAILS = [
    CommunityDetails(
        owner="hive-100001",
        title="Blank wizard",
        about="The Hive blank community",
        description=(
            "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut "
            "labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco "
            "laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in"
            " voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat"
            " non proident, sunt in culpa qui officia deserunt mollit anim id est laborum."
        ),
        roles=CommunityDetails.Roles(
            admin=["denserautotest4"],
        ),
    ),
    CommunityDetails(
        owner="hive-100002",
        title="LeoFinance",
        about="LeoFinance is a community for crypto finance. Powered by Hive and the LEO token economy.",
        flag_text=(
            "Rules Content should be related to the financial space i.e. crypto equities etc. etc. Posts "
            "created from our interface httpsleofinance.io are eligible for upvotes from leo.voter and will "
            "automatically be posted to our Hive community our front end and other Hive front ends as well "
            "Posts in our community are also eligible to earn our native token LEO in conjunction with HIVE "
            "post rewards If you have any questions or need help with anything feel free to reach out to us on"
            " twitter financeleo or head over to our discord server httpsdiscord.ggKgcVDKQ"
        ),
        description=(
            "Description Using our Hive-based token LEO we reward content creators and users for "
            "engaging on our platform at httpsleofinance.io and within our community on the Hive"
            " blockchain.) Blogging is just the beginning of whats possible in the LeoFinance community"
            " and with the LEO token 1. Trade LEO and other Hive-based tokens on our exchange"
            " httpsleodex.io 2. Track your Hive account statistics at httpshivestats.io 3. Opt-in to"
            " ads on LEO Apps which drives value back into the LEO token economy from ad buybacks. 4."
            " Learn contribute to our crypto-educational resource at httpsleopedia.io 5. Wrap LEO onto"
            " the Ethereum blockchain with our cross-chain token bridge httpswleo.io coming soon Learn"
            " more about us at httpsleopedia.iofaq"
        ),
        subs=["dollarvigilante", "curie", "gavvet"],
        posts_number=20,
        roles=CommunityDetails.Roles(
            admin=["denserautotest3", "denserautotest4"],
            member=["cass"],
            mod=["steempower"],
            muted=["calaber24p"],
        ),
    ),
    CommunityDetails(
        owner="hive-100003",
        title="Worldmappin",
        about="The Hive travel community",
        flag_text=(
            "This community is only for posts pinned to Worldmappin Go to httpsworldmappin.com find the"
            " location of your publication on the map you can do this by scrolling on the map or using the "
            "search bar get the code and add to your Hive post. English Spanish or bi-lingual with one of the"
            " languages being English content only. Original content only no plagiarism no AI generated"
            " content no recycled content. Content should be about places that you have personally travelled"
            " to yourself and your experience. Worldmappin is a travel community please post food and"
            " restaurant reviews in other appropriate communities unless the focus of the post is not about "
            "the food from the restaurant. For questions and support join our Discord httpsdiscord.ggEGtBvSM"
        ),
        description=(
            "Worldmappin Pin all your Hive travel posts on the map Get curated by our team Check it out at"
            " httpsworldmappin.com Please join our Discord httpsdiscord.ggEGtBvSM"
        ),
        subs=["denserautotest1", "dollarvigilante", "curie", "gavvet"],
        posts_number=10,
        roles=CommunityDetails.Roles(
            admin=["denserautotest4"],
            guest=["curie"],
            member=["cass"],
            mod=["calaber24p", "steempower"],
        ),
    ),
    CommunityDetails(
        owner="hive-100004",
        title="Test wizard",
        about="The Hive test community",
        description=(
            "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut "
            "labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco "
            "laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in"
            " voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat"
            " non proident, sunt in culpa qui officia deserunt mollit anim id est laborum."
        ),
        subs=["denserautotest0", "cass", "steempower", "calaber24p"],
        posts_number=5,
        roles=CommunityDetails.Roles(
            admin=["denserautotest3"],
            mod=["denserautotest4"],
        ),
    ),
    CommunityDetails(
        owner="hive-100005",
        title="Lifestyle",
        about=(
            "Lifestyle is content about everyday life Vision-boards Health home personal finances fitness hobbies"
            " work-life."
        ),
        flag_text=(
            "Only content that doesnt have a niche community already. Only Original content Allow. No Porn"
            " Sexual explicit images or nudes allowed. All languages are welcome. Plagiarism Multi-Account or"
            " any kind of abuse to the reward pool or the Blockchain in general will not be tolerated and will"
            " be muted from the community."
        ),
        description=(
            "Lifestyle content is written or visual content about everyday life. The content allow in our "
            "community is any that doesnt have a niche community already like health leisure house home "
            "personal finances fitness green living interior design hobbies work-life balance and vision-boards."
        ),
        subs=["denserautotest4", "dollarvigilante", "curie", "gavvet", "cass", "steempower", "calaber24p"],
        posts_number=20,
        roles=CommunityDetails.Roles(
            admin=["denserautotest4"],
        ),
        pinned_posts=4,
    ),
    CommunityDetails(
        owner="hive-100006",
        title="Photography Lovers",
        about="A place for photography lovers to share their work and photography tips.",
        flag_text=(
            "Original photography only. If you are not the owner of or the model in the photos that you are "
            "sharing you will be flagged and muted. Plagiarism is not tolerated and will get you flagged and "
            "muted. Stock Images unless they are yours will get you flagged and muted. All NON-Photography "
            "posts will be muted. Single image posts with little or no context may be muted. Selfies and Daily "
            "Snapshot posts may be muted at curator discretion. Any and all use of Artificial Intelligence in "
            "your images or text must be disclosed in your post. Artificial Intelligence only posts will be "
            "muted. Spamming posts into this community will also get you muted. Cross Posting is allowed but "
            "will not be curated. Active Users on Steemit andor Blurt will be muted."
        ),
        description=(
            "Are you a photographer Do you love photography Well this is the community for you. It doesnt"
            " matter what skill level you are. You can be a long time professional or a brand new "
            "photographer just starting out with a cell phone. We all have one thing in common we all want to"
            " share the world through our lens and tell its stories. Join the Photography Lovers community "
            "where we can all share our love for the art and learn from each other."
        ),
        subs=["denserautotest3", "dollarvigilante", "curie", "gavvet", "cass", "steempower", "calaber24p"],
        posts_number=10,
        roles=CommunityDetails.Roles(
            admin=["denserautotest4"],
            mod=["steempower"],
            muted=["calaber24p"],
        ),
        pinned_posts=1,
    ),
    CommunityDetails(
        owner="hive-100007",
        title="Vibes",
        about="A weekly music competition thats bringing back good vibes to WEB3",
        flag_text=(
            "Upload a video on X Twitter TikTok or Instagram. The video can include singing or playing an "
            "instrument Djiing. At the beginning of the video participants must audibly say VIBES Web3 Music "
            "Competition Week 123 etc. to indicate the corresponding week of the competition. Write a post on "
            "Vibes containing the X TikTok Instagram song link along with a brief description of the song. The "
            "title of the post should follow the format Song Name - VIBES Week Number. The judging panel "
            "consists of three members. Two judges are competition organizers. The third judge is the community"
            " itself. A poll will be conducted every week and community members will vote for their favorite "
            "entries. Cash prizes HBD will be awarded to the top-performing entries based on the combined "
            "scores from the judges and community. Participants should avoid any form of manipulation or "
            "unethical practices. Any breach of rules will lead to disqualification and exclusion from future "
            "competitions."
        ),
        description=(
            "Competition Overview Every week our Web3 Music Competition invites artists from around the globe"
            " to showcase their musical genius on the decentralized Web3 stage. Utilizing blockchain "
            "technology our competition is committed to empowering musicians their audiences and rewarding "
            "excellence. Weekly Prizes In our quest to support and nurture talent were thrilled to announce "
            "weekly prizes for the most exceptional artists. Vibes believes in putting resources directly "
            "into the hands of artists and each week well be awarding substantial cash prizes as well as "
            "rewards via Hive upvotes to the standout submissions. Growing the Web3 Music Community Beyond "
            "the competition Vibes is dedicated to cultivating a thriving Web3 music community. Connect with "
            "fellow artists explore new collaborations and dive into the decentralized world of music "
            "creation. Our platform is designed to encourage dialogue knowledge-sharing and the building of a"
            " supportive ecosystem."
        ),
        subs=["denserautotest2", "dollarvigilante", "curie", "gavvet", "cass", "steempower", "calaber24p"],
        posts_number=10,
        roles=CommunityDetails.Roles(
            admin=["denserautotest4"],
        ),
    ),
    CommunityDetails(
        owner="hive-100008",
        title="Test nsfw community",
        about="A weekly music competition thats bringing back good vibes to WEB3",
        is_nsfw=True,
        flag_text=(
            "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut "
            "labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco "
            "laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in"
            " voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat"
            " non proident, sunt in culpa qui officia deserunt mollit anim id est laborum."
        ),
        description=(
            "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut "
            "labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco "
            "laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in"
            " voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat"
            " non proident, sunt in culpa qui officia deserunt mollit anim id est laborum."
        ),
        subs=["denserautotest0", "dollarvigilante", "curie", "gavvet"],
        posts_number=10,
        pinned_posts=2,
        roles=CommunityDetails.Roles(admin=["denserautotest4"], mod=["steempower"]),
    ),
]
