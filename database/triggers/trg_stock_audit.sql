CREATE OR REPLACE TRIGGER trg_stock_audit
AFTER INSERT OR UPDATE OF quantity ON Inventory
FOR EACH ROW
DECLARE
    v_action_type  VARCHAR2(16);
    v_old_quantity NUMBER(18);
BEGIN
    IF INSERTING THEN
        v_action_type := 'INSERT';
        v_old_quantity := NULL;
    ELSE
        v_action_type := 'UPDATE';
        v_old_quantity := :OLD.quantity;
    END IF;

    INSERT INTO Audit_Logs (
        warehouse_id,
        sku_id,
        action_type,
        old_quantity,
        new_quantity,
        changed_by,
        changed_at
    ) VALUES (
        :NEW.warehouse_id,
        :NEW.sku_id,
        v_action_type,
        v_old_quantity,
        :NEW.quantity,
        :NEW.updated_by,
        SYSTIMESTAMP
    );
END;
/
