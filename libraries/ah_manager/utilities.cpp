
#include <vector>
#include <cstdint>

#include <hive/chain/util/impacted.hpp>

#include <fc/io/json.hpp>
#include <fc/string.hpp>

using hive::protocol::account_name_type;
using fc::string;

extern "C"
{

#ifdef elog
#pragma push_macro( "elog" )
#undef elog
#endif

#include <postgres.h>
#include <fmgr.h>
#include <catalog/pg_type.h>
#include <utils/builtins.h>
#include <utils/array.h>
#include <utils/lsyscache.h>

#pragma pop_macro("elog")

PG_MODULE_MAGIC;

// PG_FUNCTION_INFO_V1(my_test);
// PG_FUNCTION_INFO_V1(my_test2);
// PG_FUNCTION_INFO_V1(my_test3);
// PG_FUNCTION_INFO_V1(my_test4);
// PG_FUNCTION_INFO_V1(my_test5);
PG_FUNCTION_INFO_V1(get_impacted_accounts);

// Datum my_test(PG_FUNCTION_ARGS)
// {
//     int32 arg = PG_GETARG_INT32(0);

//     PG_RETURN_INT32(arg + 13);
// }

// Datum my_test2(PG_FUNCTION_ARGS)
// {
//     text* arg = PG_GETARG_TEXT_PP(0);

//     PG_RETURN_TEXT_P(arg);
// }

// Datum my_test3(PG_FUNCTION_ARGS)
// {
//     ArrayType  *result;
//     Oid         element_type = get_fn_expr_argtype(fcinfo->flinfo, 0);
//     Datum       element;
//     bool        isnull;
//     int16       typlen;
//     bool        typbyval;
//     char        typalign;
//     int         ndims;
//     int         dims[MAXDIM];
//     int         lbs[MAXDIM];

//     if (!OidIsValid(element_type))
//         elog(ERROR, "could not determine data type of input");

//     /* get the provided element, being careful in case it's NULL */
//     isnull = PG_ARGISNULL(0);
//     if (isnull)
//         element = (Datum) 0;
//     else
//         element = PG_GETARG_DATUM(0);

//     /* we have one dimension */
//     ndims = 1;
//     /* and one element */
//     dims[0] = 1;
//     /* and lower bound is 1 */
//     lbs[0] = 1;

//     /* get required info about the element type */
//     get_typlenbyvalalign(element_type, &typlen, &typbyval, &typalign);

//     /* now build the array */
//     result = construct_md_array(&element, &isnull, ndims, dims, lbs,
//                                 element_type, typlen, typbyval, typalign);

//     PG_RETURN_ARRAYTYPE_P(result);
// }

// Datum my_test4(PG_FUNCTION_ARGS)
// {
//     Datum       elements[4];
//     ArrayType   *array;

//     int16 typlen;
//     bool typbyval;
//     char typalign;

//     int i;

//     for (i = 0; i < 4; i++)
//         elements[i] = i + 10;

//     get_typlenbyvalalign(INT8OID, &typlen, &typbyval, &typalign);
//     array = construct_array(elements, 4, INT8OID, typlen, typbyval, typalign);

//     PG_RETURN_POINTER(array);
// }

// Datum my_test5(PG_FUNCTION_ARGS)
// {
//     Datum*     elements;
//     ArrayType* array;

//     int16 typlen;
//     bool typbyval;
//     char typalign;

//     int i;
//     int nr_elements = 12;

//     elements = (Datum*) palloc( sizeof(Datum) * nr_elements );

//     for (i = 0; i < nr_elements; i++)
//         elements[i] = i + 200;

//     get_typlenbyvalalign(INT8OID, &typlen, &typbyval, &typalign);
//     array = construct_array(elements, nr_elements, INT8OID, typlen, typbyval, typalign);

//     PG_RETURN_POINTER(array);
// }

flat_set<account_name_type> get_accounts( const std::string& operation_body )
{
  hive::protocol::operation _op;
  from_variant( fc::json::from_string( operation_body ), _op );

  flat_set<account_name_type> _impacted;
  hive::app::operation_get_impacted_accounts( _op, _impacted );

  return _impacted;
}

Datum get_impacted_accounts(PG_FUNCTION_ARGS)
{
    Datum*     result = nullptr;
    ArrayType* array  = nullptr;

    int16 typlen  = 0;
    bool typbyval = 0;
    char typalign = 0;

    VarChar* _arg0 = (VarChar*)PG_GETARG_VARCHAR_P(0);
    char* _op_body = (char*)VARDATA(_arg0);

    flat_set<account_name_type> _accounts = get_accounts( _op_body );

    if( _accounts.empty() )
    {
      PG_RETURN_POINTER(array);
    }

    auto _size = _accounts.size();

    result = ( Datum* ) palloc( sizeof( Datum ) * _size );

    uint32_t cnt = 0;
    for( auto& acc : _accounts )
    {
      string _str = (string)acc;
      result[ cnt++ ] = CStringGetTextDatum( _str.c_str() );
    }

    get_typlenbyvalalign(TEXTOID, &typlen, &typbyval, &typalign);
    array = construct_array(result, _size, TEXTOID, typlen, typbyval, typalign);

    PG_RETURN_POINTER(array);
}

}
