CREATE OR REPLACE PROCEDURE proc_inbound (
    p_sku_id       IN VARCHAR2,
    p_quantity     IN NUMBER,
    p_warehouse_id IN VARCHAR2 DEFAULT 'DEFAULT',
    p_operator_id  IN VARCHAR2 DEFAULT NULL
) AS
    v_current_quantity Inventory.quantity%TYPE;
    v_sku_count         NUMBER;
BEGIN
    IF p_sku_id IS NULL THEN
        RAISE_APPLICATION_ERROR(-20001, 'SKU must not be empty');
    END IF;

    IF p_quantity IS NULL OR p_quantity <= 0 THEN
        RAISE_APPLICATION_ERROR(-20002, 'Inbound quantity must be greater than zero');
    END IF;

    SELECT COUNT(*)
      INTO v_sku_count
      FROM SKU
     WHERE sku_id = p_sku_id
       AND is_active = 1;

    IF v_sku_count = 0 THEN
        RAISE_APPLICATION_ERROR(-20003, 'SKU does not exist or is inactive');
    END IF;

    BEGIN
        SELECT quantity
          INTO v_current_quantity
          FROM Inventory
         WHERE warehouse_id = NVL(p_warehouse_id, 'DEFAULT')
           AND sku_id = p_sku_id
         FOR UPDATE;

        UPDATE Inventory
           SET quantity = v_current_quantity + p_quantity,
               updated_by = p_operator_id,
               updated_at = SYSTIMESTAMP
         WHERE warehouse_id = NVL(p_warehouse_id, 'DEFAULT')
           AND sku_id = p_sku_id;
    EXCEPTION
        WHEN NO_DATA_FOUND THEN
            INSERT INTO Inventory (
                warehouse_id,
                sku_id,
                quantity,
                updated_by,
                updated_at
            ) VALUES (
                NVL(p_warehouse_id, 'DEFAULT'),
                p_sku_id,
                p_quantity,
                p_operator_id,
                SYSTIMESTAMP
            );
    END;

    INSERT INTO Inbound_Orders (
        warehouse_id,
        sku_id,
        quantity,
        status,
        operator_id,
        created_at
    ) VALUES (
        NVL(p_warehouse_id, 'DEFAULT'),
        p_sku_id,
        p_quantity,
        'COMPLETED',
        p_operator_id,
        SYSTIMESTAMP
    );
END proc_inbound;
/
