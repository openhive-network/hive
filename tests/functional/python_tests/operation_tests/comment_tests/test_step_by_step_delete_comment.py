import test_tools as tt
from hive_local_tools.functional.python.operation import Comment, get_transaction_timestamp

def test_step_by_step(prepared_node: tt.InitNode, wallet: tt.Wallet):
    # prepared_node = tt.InitNode()
    # prepared_node.config.plugin.append("account_history_api")
    
    # prepared_node.run()
    # wallet = tt.Wallet(attach_to=prepared_node)

    alice_comment = Comment(prepared_node, wallet, 'alice')  # tutaj tworzysz account object oraz konto, konto ma zapisany pewien stan, ale objekt jest czysty.
    alice_obj = alice_comment.author_obj
    alice_obj.update_account_info()  # żeby zapisać stan w obiekcie, trzeba wywołać tą metodę - czyli tutaj stan początkowy == stan w obiekcie

    post_transaction = alice_comment.post()  # wykonujesz operację, stan konta w blockchain się zmienił, ale obiekt trzyma stan przed-operacyjny (z linijki 13)

    # sekcja asserta
    post_rc_cost = int(post_transaction["rc_cost"])
    post_timestamp = get_transaction_timestamp(prepared_node, post_transaction)
    # ten assert sprawdza, czy stan aktualny z blockchaina + poniesiony koszt operacji == stan z obiektu account
    alice_obj.rc_manabar.assert_rc_current_mana_is_reduced(operation_rc_cost=post_rc_cost,
                                                           operation_timestamp=post_timestamp)
    # sprawdziło -> poniesiony koszt się zgadza, więc aktualizujemy obiekt
    alice_obj.update_account_info()  # -> zapisano w obiekcie stan blockchainowy -> stan po operacji `post()`

    # operacja 2, w obj mamy zapisany stan na 'po operacji post()`
    delete_transaction = alice_comment.delete()

    # sekcja asserta 2
    delete_rc_cost = int(delete_transaction["rc_cost"])
    delete_timestamp = get_transaction_timestamp(prepared_node, delete_transaction)
    alice_obj.rc_manabar.assert_rc_current_mana_is_reduced(operation_rc_cost=delete_rc_cost,
                                                           operation_timestamp=delete_timestamp)
    # sprawdzamy, czy stan aktualny z blockchaina + poniesiony koszt operacji2 == stan z obiektu account z linijki 24

