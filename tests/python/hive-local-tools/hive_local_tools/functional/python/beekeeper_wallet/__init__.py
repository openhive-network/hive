from __future__ import annotations


def find_proposals_by_creator_name(_creator, _proposal_list):
    proposals = []
    for rs in _proposal_list.proposals:
        if rs["creator"] == _creator:
            proposals.append(rs)
    return proposals


def find_proposals_by_voter_name(_voter, _proposal_list):
    proposals = []
    for rs in _proposal_list.proposal_votes:
        if rs["voter"] == _voter:
            proposals.append(rs)
    return proposals
