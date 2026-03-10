#pragma once

// Shared test macros for split object access patterns.
// Used by tests that need to access account_details_object or tiny_account_object.

// Lookup account_details_object by account name string
#define GET_ASSETS( account_name ) (db->get_account_details( db->get_account( account_name ).get_id() ))

// Lookup account_details_object by account object
#define GET_ASSETS_FOR_ACC( acc ) (db->get_account_details( (acc).get_id() ))

// Lookup tiny_account_object by account name string
#define GET_TINY( account_name ) (*db->get_index< tiny_account_index, by_name >().find( account_name ))
