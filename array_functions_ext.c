#include "postgres.h"
#include "fmgr.h"
#include "utils/array.h"
#include "utils/builtins.h"
#include "catalog/pg_type.h"
#include "catalog/pg_collation_d.h"
#include "utils/numeric.h"
#include "utils/lsyscache.h"
#include "utils/fmgroids.h"
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include <float.h>

#define ARRAY_SIZE_LIMIT 1000000

PG_MODULE_MAGIC;

static void
overflow_err(const char *type, int64 val)
{
    ereport(ERROR,
            (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE),
             errmsg("integer overflow in %s summation (value: %ld)", type, (long) val)));
}

/*------------------- array_size_check -------------------*/
static void
array_size_check(int nelems, const char *funcname)
{
    if (unlikely(nelems > ARRAY_SIZE_LIMIT))
        ereport(ERROR,
                (errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED),
                 errmsg("array too big: %d elements, max %d in function %s", nelems, ARRAY_SIZE_LIMIT, funcname)));
}

/*-------------------------- array_map_concat --------------------------*/
PG_FUNCTION_INFO_V1(array_map_concat);

Datum
array_map_concat(PG_FUNCTION_ARGS)
{
    ArrayType  *input_array;
    text       *suffix;
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
    if (PG_ARGISNULL(1))
        PG_RETURN_NULL();

    input_array = PG_GETARG_ARRAYTYPE_P(0);

    deconstruct_array(input_array,
                      TEXTOID,
                      -1, false, 'i',
                      &elements, &nulls, &nelems);

    if (nelems == 0)
        PG_RETURN_ARRAYTYPE_P(input_array);

    array_size_check(nelems, "array_map_concat");

    suffix = PG_GETARG_TEXT_PP(1);

    result_elems = (Datum *) palloc(sizeof(Datum) * nelems);
    result_nulls = (bool *) palloc(sizeof(bool) * nelems);

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
            int32 elem_len = VARSIZE_ANY_EXHDR(elem);
            int32 suffix_len = VARSIZE_ANY_EXHDR(suffix);
            int64 new_len = (int64) elem_len + (int64) suffix_len;
            text *new_text;

            if (new_len > 0x3FFFFFFF - VARHDRSZ)
                ereport(ERROR,
                        (errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED),
                         errmsg("result element too big: max %d bytes", 0x3FFFFFFF)));

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
    int16       typlen;
    bool        typbyval;
    char        typalign;
    int64       sum_int64 = 0;
    double      sum_float8 = 0.0;
    float       sum_float4 = 0.0f;
    Datum       sum_numeric = (Datum) 0;
    bool        numeric_initialized = false;

    if (PG_ARGISNULL(0))
        PG_RETURN_NULL();

    input_array = PG_GETARG_ARRAYTYPE_P(0);
    element_type = ARR_ELEMTYPE(input_array);

    get_typlenbyvalalign(element_type, &typlen, &typbyval, &typalign);

    deconstruct_array(input_array,
                      element_type,
                      typlen,
                      typbyval,
                      typalign,
                      &elements, &nulls, &nelems);

    array_size_check(nelems, "array_sum");

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
            case INT2OID:
                sum_int64 += DatumGetInt16(elements[i]);
                break;
            case INT4OID:
            {
                int64 val = DatumGetInt32(elements[i]);
                if ((val > 0 && sum_int64 > INT32_MAX - val) ||
                    (val < 0 && sum_int64 < INT32_MIN - val))
                {
                    overflow_err("int4", val);
                }
                sum_int64 += val;
                break;
            }
            case INT8OID:
            {
                int64 val = DatumGetInt64(elements[i]);
                if ((val > 0 && sum_int64 > INT64_MAX - val) ||
                    (val < 0 && sum_int64 < INT64_MIN - val))
                {
                    overflow_err("int8", val);
                }
                sum_int64 += val;
                break;
            }
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
                ereport(ERROR,
                        (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
                         errmsg("unsupported array element type for array_sum: %u", element_type)));
        }
    }

    switch (element_type)
    {
        case INT2OID:
            if (sum_int64 < INT16_MIN || sum_int64 > INT16_MAX)
                overflow_err("int2", sum_int64);
            PG_RETURN_INT16((int16) sum_int64);
        case INT4OID:
            if (sum_int64 < INT32_MIN || sum_int64 > INT32_MAX)
                overflow_err("int4", sum_int64);
            PG_RETURN_INT32((int32) sum_int64);
        case INT8OID:
            PG_RETURN_INT64(sum_int64);
        case FLOAT4OID:  PG_RETURN_FLOAT4((float) sum_float4);
        case FLOAT8OID:  PG_RETURN_FLOAT8(sum_float8);
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

    array_size_check(nelems, "array_exists");

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
            if (elements[i] == search)
                PG_RETURN_BOOL(true);
        }
        else if (typlen == -1)
        {
            struct varlena *a = (struct varlena *) DatumGetPointer(elements[i]);
            struct varlena *b = (struct varlena *) DatumGetPointer(search);
            int alen = VARSIZE_ANY_EXHDR(a);
            int blen = VARSIZE_ANY_EXHDR(b);

            if (alen == blen && memcmp(VARDATA_ANY(a), VARDATA_ANY(b), alen) == 0)
                PG_RETURN_BOOL(true);
        }
        else if (typlen > 0)
        {
            if (memcmp(DatumGetPointer(elements[i]), DatumGetPointer(search), typlen) == 0)
                PG_RETURN_BOOL(true);
        }
    }

    PG_RETURN_BOOL(false);
}

/*-------------------------- array_exists_epsilon --------------------------*/
PG_FUNCTION_INFO_V1(array_exists_epsilon);

Datum
array_exists_epsilon(PG_FUNCTION_ARGS)
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

    if (element_type == FLOAT4OID)
    {
        float4 a = DatumGetFloat4(search);
        for (i = 0; i < nelems; i++)
        {
            if (nulls[i]) continue;
            if (fabsf(a - DatumGetFloat4(elements[i])) <= FLT_EPSILON * fmaxf(fabsf(a), 1.0f) * nelems)
                PG_RETURN_BOOL(true);
        }
        PG_RETURN_BOOL(false);
    }

    if (element_type == FLOAT8OID)
    {
        float8 a = DatumGetFloat8(search);
        for (i = 0; i < nelems; i++)
        {
            if (nulls[i]) continue;
            if (fabs(a - DatumGetFloat8(elements[i])) <= DBL_EPSILON * fmax(fabs(a), 1.0) * nelems)
                PG_RETURN_BOOL(true);
        }
        PG_RETURN_BOOL(false);
    }

    /* Fallback for other types: exact Datum comparison */
    for (i = 0; i < nelems; i++)
    {
        if (nulls[i])
            continue;

        if (typbyval)
        {
            if (elements[i] == search)
                PG_RETURN_BOOL(true);
        }
        else if (typlen == -1)
        {
            struct varlena *a = (struct varlena *) DatumGetPointer(elements[i]);
            struct varlena *b = (struct varlena *) DatumGetPointer(search);
            int alen = VARSIZE_ANY_EXHDR(a);
            int blen = VARSIZE_ANY_EXHDR(b);

            if (alen == blen && memcmp(VARDATA_ANY(a), VARDATA_ANY(b), alen) == 0)
                PG_RETURN_BOOL(true);
        }
        else if (typlen > 0)
        {
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
    Datum       match;

    if (PG_ARGISNULL(0))
        PG_RETURN_BOOL(false);

    input_array = PG_GETARG_ARRAYTYPE_P(0);

    deconstruct_array(input_array,
                      TEXTOID,
                      -1, false, 'i',
                      &elements, &nulls, &nelems);

    array_size_check(nelems, "array_match");

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
