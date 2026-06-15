-- =============================================================================
-- WMS — 一键重置数据库（开发用）
-- 用法：sqlplus wms/123123@localhost:1522/FREEPDB1 @reset_all.sql
-- =============================================================================

SET SERVEROUTPUT ON

-- 清理
BEGIN
    FOR r IN (SELECT table_name FROM user_tables) LOOP
        EXECUTE IMMEDIATE 'DROP TABLE ' || r.table_name || ' CASCADE CONSTRAINTS';
    END LOOP;
    FOR s IN (SELECT sequence_name FROM user_sequences) LOOP
        EXECUTE IMMEDIATE 'DROP SEQUENCE ' || s.sequence_name;
    END LOOP;
END;
/

-- 重建
@@tables.sql
@@sequences.sql
@@seed.sql

SELECT '=== 重建完成 ===' AS status FROM DUAL;
SELECT table_name FROM user_tables ORDER BY table_name;
