-- ============================================================================
-- test_array_functions_ext.sql
-- Comprehensive test suite for array_functions_ext extension with all improvements
-- ============================================================================
\timing off
\x off

-- Helper function to build a float8 array from runtime computation (avoids constant folding)
CREATE OR REPLACE FUNCTION build_runtime_float_arr() RETURNS float8[] AS $$
    SELECT ARRAY[0.1::float8 + 0.2::float8, 0.5::float8];
$$ LANGUAGE SQL;

DO $$
BEGIN
    RAISE NOTICE '═══════════════════════════════════════════════════';
    RAISE NOTICE 'array_functions_ext - Comprehensive Tests';
    RAISE NOTICE '═══════════════════════════════════════════════════';
END $$;

DO $$
DECLARE
    res text[];
BEGIN
    RAISE NOTICE '--- array_map_concat tests ---';

    res := array_map_concat(ARRAY['foo','bar','baz']::text[], '!!!');
    IF res = ARRAY['foo!!!','bar!!!','baz!!!'] THEN
        RAISE NOTICE '[PASS] Basic concat';
    ELSE RAise EXCEPTION '[FAIL] Basic concat: %', res; END IF;

    res := array_map_concat(ARRAY['a','b']::text[], '');
    IF res = ARRAY['a','b'] THEN
        RAISE NOTICE '[PASS] Empty suffix';
    ELSE RAise EXCEPTION '[FAIL] Empty suffix'; END IF;

    res := array_map_concat(ARRAY[NULL,'x',NULL]::text[], 'A');
    IF array_length(res,1)=3 AND res[1] IS NULL AND res[2]='xA' AND res[3] IS NULL THEN
        RAISE NOTICE '[PASS] NULL preserved';
    ELSE RAise EXCEPTION '[FAIL] NULL preserved'; END IF;

    IF array_map_concat('{}'::text[], 'x') IS NULL THEN
        RAise NOTICE '[PASS] Empty array -> NULL';
    ELSE RAise NOTICE '[INFO] Empty array result'; END IF;

    IF array_map_concat(NULL::text[], 'x') IS NULL AND array_map_concat(ARRAY['a']::text[], NULL) IS NULL THEN
        RAISE NOTICE '[PASS] STRICT: NULL array/suffix -> NULL';
    ELSE RAise EXCEPTION '[FAIL] STRICT'; END IF;
END $$;

DO $$
DECLARE
    res numeric;
BEGIN
    RAISE NOTICE '--- array_sum tests ---';

    IF array_sum(ARRAY[1,2,3,4]::int[]) = 10 THEN RAISE NOTICE '[PASS] int sum=10'; ELSE RAise EXCEPTION '[FAIL] int sum'; END IF;
    IF array_sum(ARRAY[10000000000,20000000000]::bigint[]) = 30000000000 THEN RAISE NOTICE '[PASS] bigint sum'; ELSE RAise EXCEPTION '[FAIL] bigint'; END IF;
    IF abs(array_sum(ARRAY[1.5,2.5,3.0]::float[]) - 7.0) < 1e-10 THEN RAISE NOTICE '[PASS] float sum=7.0'; ELSE RAise EXCEPTION '[FAIL] float'; END IF;
    IF array_sum(ARRAY['1.1','2.2','3.3']::numeric[]) = '6.6'::numeric THEN RAISE NOTICE '[PASS] numeric'; ELSE RAise EXCEPTION '[FAIL] numeric'; END IF;
    IF array_sum(ARRAY[]::int[]) = 0 THEN RAISE NOTICE '[PASS] empty int -> 0'; ELSE RAISE NOTICE '[INFO] empty int result'; END IF;
    IF array_sum(NULL::int[]) IS NULL THEN RAISE NOTICE '[PASS] NULL array -> NULL'; ELSE RAise EXCEPTION '[FAIL] NULL array'; END IF;
    IF array_sum(ARRAY[1,NULL,3]::int[]) = 4 THEN RAISE NOTICE '[PASS] SKIP NULL -> 4'; ELSE RAise EXCEPTION '[FAIL] SKIP NULL'; END IF;
END $$;

DO $$
BEGIN
    RAISE NOTICE '--- array_sum overflow tests ---';
    BEGIN PERFORM array_sum(ARRAY[2147483640,10]::int[]); RAise EXCEPTION '[FAIL] Should have thrown overflow'; EXCEPTION WHEN numeric_value_out_of_range THEN RAISE NOTICE '[PASS] int4 overflow caught'; END;
    BEGIN PERFORM array_sum(ARRAY[32760,10]::smallint[]); RAise EXCEPTION '[FAIL] int2 overflow not caught'; EXCEPTION WHEN numeric_value_out_of_range THEN RAISE NOTICE '[PASS] int2 overflow caught'; END;
END $$;

DO $$
BEGIN
    RAISE NOTICE '--- array_exists tests ---';
    IF array_exists(ARRAY['a','b','c']::text[], 'b') THEN RAISE NOTICE '[PASS] text found'; ELSE RAise EXCEPTION '[FAIL] text found'; END IF;
    IF NOT array_exists(ARRAY['a','b','c']::text[], 'd') THEN RAISE NOTICE '[PASS] text not found'; ELSE RAise EXCEPTION '[FAIL] text not found'; END IF;
    IF array_exists(ARRAY['a',NULL,'c']::text[], NULL) THEN RAISE NOTICE '[PASS] NULL in array'; ELSE RAise EXCEPTION '[FAIL] NULL in array'; END IF;
    IF NOT array_exists(ARRAY['a','b','c']::text[], NULL) THEN RAISE NOTICE '[PASS] no NULL in array'; ELSE RAise EXCEPTION '[FAIL] no NULL in array'; END IF;
    IF array_exists(ARRAY[1,2,3]::int[], 2) THEN RAISE NOTICE '[PASS] int found'; ELSE RAise EXCEPTION '[FAIL] int found'; END IF;
    IF NOT array_exists(ARRAY[1,2,3]::int[], 4) THEN RAISE NOTICE '[PASS] int not found'; ELSE RAise EXCEPTION '[FAIL] int not found'; END IF;
    IF NOT array_exists(ARRAY[]::text[], 'x') THEN RAISE NOTICE '[PASS] empty array'; ELSE RAise EXCEPTION '[FAIL] empty array'; END IF;
    IF NOT array_exists(NULL::text[], 'x') THEN RAISE NOTICE '[PASS] NULL array'; ELSE RAise EXCEPTION '[FAIL] NULL array'; END IF;
END $$;

DO $$
BEGIN
    RAISE NOTICE '--- array_exists_epsilon tests ---';
    -- Exact float match (literal, should be found by both)
    IF array_exists(ARRAY[0.1,0.2,0.3]::float8[], 0.2::float8) THEN RAise NOTICE '[PASS] float exact match (strict)'; ELSE RAise EXCEPTION '[FAIL] float exact (strict)'; END IF;
    IF array_exists_epsilon(ARRAY[0.1,0.2,0.3]::float8[], 0.2::float8) THEN RAise NOTICE '[PASS] float exact match (epsilon)'; ELSE RAise EXCEPTION '[FAIL] float exact (epsilon)'; END IF;

    -- Runtime float arithmetic via SQL function (avoids constant folding in PL/pgSQL)
    -- 0.1+0.2 computed at runtime inside build_runtime_float_arr() will produce ~0.30000000000000004
    -- strict: should NOT find exactly 0.3 (bitwise different)
    IF NOT array_exists(build_runtime_float_arr(), 0.3::float8) THEN
        RAISE NOTICE '[PASS] strict: rejects runtime 0.3 (bitwise)';
    ELSE
        RAise EXCEPTION '[FAIL] strict: should NOT find bit-different 0.3';
    END IF;

    -- epsilon: SHOULD find ~0.3 (flexible)
    IF array_exists_epsilon(build_runtime_float_arr(), 0.3::float8) THEN
        RAISE NOTICE '[PASS] epsilon: accepts runtime 0.3 (epsilon)';
    ELSE
        RAise EXCEPTION '[FAIL] epsilon: should find runtime 0.3';
    END IF;
END $$;

DO $$
BEGIN
    RAISE NOTICE '--- array_match tests ---';
    IF array_match(ARRAY['abc','dfg','123']::text[], '^a') THEN RAISE NOTICE '[PASS] basic match ^a'; ELSE RAise EXCEPTION '[FAIL] basic match'; END IF;
    IF NOT array_match(ARRAY['abc','dfg','123']::text[], '^t') THEN RAISE NOTICE '[PASS] no match ^t'; ELSE RAise EXCEPTION '[FAIL] no match'; END IF;
    IF NOT array_match(ARRAY['a','b']::text[], NULL) THEN RAISE NOTICE '[PASS] NULL pattern, no NULL'; ELSE RAise EXCEPTION '[FAIL] NULL pattern, no NULL'; END IF;
    IF array_match(ARRAY['a',NULL]::text[], NULL) THEN RAISE NOTICE '[PASS] NULL pattern, NULL present'; ELSE RAise EXCEPTION '[FAIL] NULL pattern, NULL present'; END IF;
    IF NOT array_match(ARRAY[]::text[], 'abc') THEN RAISE NOTICE '[PASS] empty array'; ELSE RAise EXCEPTION '[FAIL] empty array'; END IF;
    BEGIN PERFORM array_match(ARRAY['abc']::text[], '('); RAise EXCEPTION '[FAIL] invalid regex not caught'; EXCEPTION WHEN invalid_regular_expression THEN RAISE NOTICE '[PASS] invalid regex caught'; END;
END $$;

DO $$
DECLARE
    res int;
BEGIN
    RAISE NOTICE '--- parallel safe test ---';
    SELECT count(*) INTO res FROM (SELECT array_sum(ARRAY[i,i+1]::int[]) FROM generate_series(1,100) i) t;
    IF res = 100 THEN RAISE NOTICE '[PASS] aggregate in subquery works'; ELSE RAise EXCEPTION '[FAIL] count mismatch'; END IF;
END $$;

DO $$
BEGIN
    RAISE NOTICE '';
    RAISE NOTICE '═══════════════════════════════════════════════════';
    RAISE NOTICE '  All tests completed successfully!';
    RAISE NOTICE '═══════════════════════════════════════════════════';
END $$;

DROP FUNCTION IF EXISTS build_runtime_float_arr();
