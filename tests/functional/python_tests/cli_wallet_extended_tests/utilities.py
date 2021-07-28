
def check_key( node_name, result, key ):
  _key_auths = result[node_name]['key_auths']
  assert len(_key_auths) == 1
  __key_auths = _key_auths[0]
  assert len(__key_auths) == 2
  __key_auths[0] == key

def check_keys( result, key_owner, key_active, key_posting, key_memo ):
    check_key( 'owner', result, key_owner )
    check_key( 'active', result, key_active )
    check_key( 'posting', result, key_posting )
    assert result['memo_key'] == key_memo
