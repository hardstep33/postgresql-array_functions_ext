-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION array_functions_ext" to load this file.\quit

-- array_map_concat: concatenate a suffix to each text element in a text array
CREATE OR REPLACE FUNCTION array_map_concat(text[], text) RETURNS text[]
AS 'MODULE_PATHNAME', 'array_map_concat'
LANGUAGE C IMMUTABLE STRICT;

-- array_sum: sum all elements of a numeric array (int2, int4, int8, float4, float8, numeric)
CREATE OR REPLACE FUNCTION array_sum(anyarray) RETURNS anyelement
AS 'MODULE_PATHNAME', 'array_sum'
LANGUAGE C IMMUTABLE STRICT;

-- array_exists: check if an element exists in any type of array
CREATE OR REPLACE FUNCTION array_exists(anyarray, anyelement) RETURNS boolean
AS 'MODULE_PATHNAME', 'array_exists'
LANGUAGE C IMMUTABLE;

-- array_match: check if any text element matches a regular expression
CREATE OR REPLACE FUNCTION array_match(text[], text) RETURNS boolean
AS 'MODULE_PATHNAME', 'array_match'
LANGUAGE C IMMUTABLE;

-- =========================================
-- Tests
-- =========================================

DO $$
DECLARE
    result text[];
BEGIN
    -- array_map_concat tests
    result := array_map_concat(ARRAY['foo','bar','baz']::text[], '!!!');
    IF result <> ARRAY['foo!!!','bar!!!','baz!!!']::text[] THEN
        RAISE EXCEPTION 'array_map_concat test 1 failed: %', result;
    END IF;

    result := array_map_concat(ARRAY[NULL,'x',NULL]::text[], 'A');
    IF array_length(result, 1) <> 3 OR result[1] IS NOT NULL OR result[2] <> 'xA' OR result[3] IS NOT NULL THEN
        RAISE EXCEPTION 'array_map_concat test 2 failed: %', result;
    END IF;

    RAISE NOTICE 'array_map_concat: OK';
END $$;

DO $$
BEGIN
    -- array_sum: int
    IF array_sum(ARRAY[1,2,3,4]::int[]) <> 10 THEN
        RAISE EXCEPTION 'array_sum int test failed';
    END IF;

    -- array_sum: bigint
    IF array_sum(ARRAY[10000000000, 20000000000]::bigint[]) <> 30000000000 THEN
        RAISE EXCEPTION 'array_sum bigint test failed';
    END IF;

    -- array_sum: float
    IF abs(array_sum(ARRAY[1.5, 2.5, 3.0]::float[]) - 7.0) > 1e-10 THEN
        RAISE EXCEPTION 'array_sum float test failed';
    END IF;

    -- array_sum: numeric
    IF array_sum(ARRAY['1.1','2.2','3.3']::numeric[]) <> '6.6'::numeric THEN
        RAISE EXCEPTION 'array_sum numeric test failed';
    END IF;

    -- array_sum: empty array returns 0
    IF array_sum(ARRAY[]::int[]) IS NULL OR array_sum(ARRAY[]::int[]) <> 0 THEN
        RAISE EXCEPTION 'array_sum empty int array test failed';
    END IF;

    RAISE NOTICE 'array_sum: OK';
END $$;

DO $$
BEGIN
    -- array_exists: text
    IF NOT array_exists(ARRAY['a','b','c']::text[], 'b'::text) THEN
        RAISE EXCEPTION 'array_exists text found test failed';
    END IF;
    IF array_exists(ARRAY['a','b','c']::text[], 'd'::text) THEN
        RAISE EXCEPTION 'array_exists text not-found test failed';
    END IF;

    -- array_exists: NULL element inside array, searching for NULL
    IF NOT array_exists(ARRAY['a',NULL,'c']::text[], NULL) THEN
        RAISE EXCEPTION 'array_exists text null search test failed';
    END IF;
    -- array_exists: no NULL inside array, searching for NULL
    IF array_exists(ARRAY['a','b','c']::text[], NULL) THEN
        RAISE EXCEPTION 'array_exists text no-null test failed';
    END IF;

    -- array_exists: int
    IF NOT array_exists(ARRAY[1,2,3]::int[], 2) THEN
        RAISE EXCEPTION 'array_exists int found test failed';
    END IF;
    IF array_exists(ARRAY[1,2,3]::int[], 4) THEN
        RAISE EXCEPTION 'array_exists int not-found test failed';
    END IF;
    IF NOT array_exists(ARRAY[1,NULL,3]::int[], NULL::int) THEN
        RAISE EXCEPTION 'array_exists int null search test failed';
    END IF;

    -- array_exists: numeric
    IF NOT array_exists(ARRAY[1.1,2.2,3.3]::numeric[], 2.2) THEN
        RAISE EXCEPTION 'array_exists numeric found test failed';
    END IF;

    -- array_exists: boolean
    IF NOT array_exists(ARRAY[true,false]::boolean[], false) THEN
        RAISE EXCEPTION 'array_exists boolean test failed';
    END IF;

    RAISE NOTICE 'array_exists: OK';
END $$;

DO $$
BEGIN
    -- array_match: regex match found
    IF NOT array_match(ARRAY['abc','dfg','123']::text[], '^a') THEN
        RAISE EXCEPTION 'array_match test 1 failed';
    END IF;

    -- array_match: regex match not found
    IF array_match(ARRAY['abc','dfg','123']::text[], '^t') THEN
        RAISE EXCEPTION 'array_match test 2 failed';
    END IF;

    -- array_match: digit-only regex
    IF NOT array_match(ARRAY['abc','dfg','123']::text[], '^\d+$') THEN
        RAISE EXCEPTION 'array_match test 3 failed';
    END IF;

    -- array_match: NULL pattern, no NULL in array => false
    IF array_match(ARRAY['abc','dfg','123']::text[], NULL) THEN
        RAISE EXCEPTION 'array_match test 4 failed';
    END IF;

    -- array_match: NULL pattern, NULL in array => true
    IF NOT array_match(ARRAY['abc','dfg',NULL]::text[], NULL) THEN
        RAISE EXCEPTION 'array_match test 5 failed';
    END IF;

    RAISE NOTICE 'array_match: OK';
END $$;
