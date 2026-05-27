CREATE OR REPLACE TRIGGER trg_stock_audit
AFTER INSERT OR UPDATE OF quantity ON Inventory
FOR EACH ROW
BEGIN
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
        CASE WHEN INSERTING THEN 'INSERT' ELSE 'UPDATE' END,
        CASE WHEN INSERTING THEN NULL ELSE :OLD.quantity END,
        :NEW.quantity,
        :NEW.updated_by,
        SYSTIMESTAMP
    );
END;
/
