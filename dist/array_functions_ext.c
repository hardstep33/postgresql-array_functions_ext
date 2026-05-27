#include "postgres.h"
#include "fmgr.h"
#include "utils/array.h"
#include "utils/builtins.h"
#include "catalog/pg_type.h"
#include "catalog/pg_collation_d.h"
#include "utils/numeric.h"
#include "utils/lsyscache.h"
#include <stdbool.h>

PG_MODULE_MAGIC;

/*-------------------------- array_map_concat --------------------------*/
PG_FUNCTION_INFO_V1(array_map_concat);

Datum
array_map_concat(PG_FUNCTION_ARGS)
{
    ArrayType  *input_array;
    text       *suffix = NULL;
    Datum      *elements;
    bool       *nulls;
    int         nelems;
    Datum      *result_elems;
    bool       *result_nulls;
    int         i;
    int         dims[1];
    int         lbs[1];
    ArrayType  *result;

    if (PG_ARGISNULL(0))
        PG_RETURN_NULL();

    input_array = PG_GETARG_ARRAYTYPE_P(0);

    deconstruct_array(input_array,
                      TEXTOID,
                      -1, false, 'i',
                      &elements, &nulls, &nelems);

    if (nelems == 0)
        PG_RETURN_ARRAYTYPE_P(input_array);

    result_elems = (Datum *) palloc(sizeof(Datum) * nelems);
    result_nulls = (bool *) palloc(sizeof(bool) * nelems);

    if (PG_ARGISNULL(1))
    {
        for (i = 0; i < nelems; i++)
        {
            result_nulls[i] = true;
            result_elems[i] = (Datum) 0;
        }
    }
    else
    {
        suffix = PG_GETARG_TEXT_PP(1);
        for (i = 0; i < nelems; i++)
        {
            if (nulls[i])
            {
                result_nulls[i] = true;
                result_elems[i] = (Datum) 0;
            }
            else
            {
                text *elem = DatumGetTextPP(elements[i]);
                text *new_text;
                int32 elem_len = VARSIZE_ANY_EXHDR(elem);
                int32 suffix_len = VARSIZE_ANY_EXHDR(suffix);
                int32 new_len = elem_len + suffix_len;
                new_text = (text *) palloc(VARHDRSZ + new_len);
                SET_VARSIZE(new_text, VARHDRSZ + new_len);
                if (elem_len > 0)
                    memcpy(VARDATA(new_text), VARDATA_ANY(elem), elem_len);
                if (suffix_len > 0)
                    memcpy(VARDATA(new_text) + elem_len, VARDATA_ANY(suffix), suffix_len);
                result_elems[i] = PointerGetDatum(new_text);
                result_nulls[i] = false;
            }
        }
    }

    dims[0] = nelems;
    lbs[0] = 1;
    result = construct_md_array(result_elems,
                                result_nulls,
                                1,
                                dims,
                                lbs,
                                TEXTOID,
                                -1,
                                false,
                                'i');
    PG_RETURN_ARRAYTYPE_P(result);
}

/*-------------------------- array_sum --------------------------*/
PG_FUNCTION_INFO_V1(array_sum);

Datum
array_sum(PG_FUNCTION_ARGS)
{
    ArrayType  *input_array;
    Datum      *elements;
    bool       *nulls;
    int         nelems;
    int         i;
    Oid         element_type;
    int64       sum_int64 = 0;
    double      sum_float8 = 0.0;
    float       sum_float4 = 0.0f;
    Datum       sum_numeric = (Datum) 0;
    bool        numeric_initialized = false;

    if (PG_ARGISNULL(0))
        PG_RETURN_NULL();

    input_array = PG_GETARG_ARRAYTYPE_P(0);
    element_type = ARR_ELEMTYPE(input_array);

    int16 typlen;
    bool typbyval;
    char typalign;
    get_typlenbyvalalign(element_type, &typlen, &typbyval, &typalign);

    deconstruct_array(input_array,
                      element_type,
                      typlen,
                      typbyval,
                      typalign,
                      &elements, &nulls, &nelems);

    if (nelems == 0)
    {
        switch (element_type)
        {
            case INT2OID:  PG_RETURN_INT16(0);
            case INT4OID:  PG_RETURN_INT32(0);
            case INT8OID:  PG_RETURN_INT64(0);
            case FLOAT4OID:PG_RETURN_FLOAT4(0.0);
            case FLOAT8OID:PG_RETURN_FLOAT8(0.0);
            case NUMERICOID:PG_RETURN_NUMERIC(DirectFunctionCall1(numeric_in, CStringGetDatum("0")));
            default: PG_RETURN_NULL();
        }
    }

    for (i = 0; i < nelems; i++)
    {
        if (nulls[i])
            continue;
        switch (element_type)
        {
            case INT2OID: sum_int64 += DatumGetInt16(elements[i]); break;
            case INT4OID: sum_int64 += DatumGetInt32(elements[i]); break;
            case INT8OID: sum_int64 += DatumGetInt64(elements[i]); break;
            case FLOAT4OID: sum_float4 += DatumGetFloat4(elements[i]); break;
            case FLOAT8OID: sum_float8 += DatumGetFloat8(elements[i]); break;
            case NUMERICOID:
                if (!numeric_initialized)
                {
                    sum_numeric = DirectFunctionCall1(numeric_in, CStringGetDatum("0"));
                    numeric_initialized = true;
                }
                sum_numeric = DirectFunctionCall2(numeric_add, sum_numeric, elements[i]);
                break;
            default:
                elog(ERROR, "unsupported array element type for array_sum");
        }
    }

    switch (element_type)
    {
        case INT2OID: PG_RETURN_INT16((int16) sum_int64);
        case INT4OID: PG_RETURN_INT32((int32) sum_int64);
        case INT8OID: PG_RETURN_INT64(sum_int64);
        case FLOAT4OID: PG_RETURN_FLOAT4((float) sum_float4);
        case FLOAT8OID: PG_RETURN_FLOAT8(sum_float8);
        case NUMERICOID:
            if (!numeric_initialized)
                PG_RETURN_NUMERIC(DirectFunctionCall1(numeric_in, CStringGetDatum("0")));
            PG_RETURN_NUMERIC(sum_numeric);
        default: PG_RETURN_NULL();
    }
}

/*-------------------------- array_exists --------------------------*/
PG_FUNCTION_INFO_V1(array_exists);

Datum
array_exists(PG_FUNCTION_ARGS)
{
    ArrayType  *input_array;
    Datum      *elements;
    bool       *nulls;
    int         nelems;
    int         i;
    Oid         element_type;
    int16       typlen;
    bool        typbyval;
    char        typalign;
    Datum       search;

    if (PG_ARGISNULL(0))
        PG_RETURN_BOOL(false);

    input_array = PG_GETARG_ARRAYTYPE_P(0);
    element_type = ARR_ELEMTYPE(input_array);

    get_typlenbyvalalign(element_type, &typlen, &typbyval, &typalign);
    deconstruct_array(input_array,
                      element_type,
                      typlen,
                      typbyval,
                      typalign,
                      &elements, &nulls, &nelems);

    if (PG_ARGISNULL(1))
    {
        for (i = 0; i < nelems; i++)
        {
            if (nulls[i])
                PG_RETURN_BOOL(true);
        }
        PG_RETURN_BOOL(false);
    }

    search = PG_GETARG_DATUM(1);

    for (i = 0; i < nelems; i++)
    {
        if (nulls[i])
            continue;

        if (typbyval)
        {
            /* pass-by-value: direct Datum comparison */
            if (elements[i] == search)
                PG_RETURN_BOOL(true);
        }
        else if (typlen == -1)
        {
            /* varlena: compare lengths and contents */
            struct varlena *a = (struct varlena *) DatumGetPointer(elements[i]);
            struct varlena *b = (struct varlena *) DatumGetPointer(search);
            int             alen = VARSIZE_ANY_EXHDR(a);
            int             blen = VARSIZE_ANY_EXHDR(b);

            if (alen == blen && memcmp(VARDATA_ANY(a), VARDATA_ANY(b), alen) == 0)
                PG_RETURN_BOOL(true);
        }
        else if (typlen > 0)
        {
            /* fixed-length pass-by-reference */
            if (memcmp(DatumGetPointer(elements[i]), DatumGetPointer(search), typlen) == 0)
                PG_RETURN_BOOL(true);
        }
    }

    PG_RETURN_BOOL(false);
}

/*-------------------------- array_match --------------------------*/
PG_FUNCTION_INFO_V1(array_match);

Datum
array_match(PG_FUNCTION_ARGS)
{
    ArrayType  *input_array;
    Datum      *elements;
    bool       *nulls;
    int         nelems;
    int         i;
    text       *pattern = NULL;

    if (PG_ARGISNULL(0))
        PG_RETURN_BOOL(false);

    input_array = PG_GETARG_ARRAYTYPE_P(0);

    /* deconstruct text array */
    deconstruct_array(input_array,
                      TEXTOID,
                      -1, false, 'i',
                      &elements, &nulls, &nelems);

    /* NULL pattern => search for NULL element */
    if (PG_ARGISNULL(1))
    {
        for (i = 0; i < nelems; i++)
        {
            if (nulls[i])
                PG_RETURN_BOOL(true);
        }
        PG_RETURN_BOOL(false);
    }

    pattern = PG_GETARG_TEXT_PP(1);

    for (i = 0; i < nelems; i++)
    {
        Datum match;

        if (nulls[i])
            continue;

        match = DirectFunctionCall2Coll(textregexeq,
                                        DEFAULT_COLLATION_OID,
                                        elements[i],
                                        PointerGetDatum(pattern));
        if (DatumGetBool(match))
            PG_RETURN_BOOL(true);
    }

    PG_RETURN_BOOL(false);
}

