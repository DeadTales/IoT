-- Migra la instalación anterior solo cuando conserva las tres tablas originales.
DO $$
BEGIN
    IF to_regclass('dashboard_users') IS NOT NULL
       AND to_regclass('dashboard_devices') IS NOT NULL
       AND to_regclass('dashboard_readings') IS NOT NULL
       AND to_regclass('users') IS NULL
       AND to_regclass('devices') IS NULL
       AND to_regclass('readings') IS NULL THEN
        ALTER TABLE dashboard_users RENAME TO users;
        ALTER TABLE dashboard_devices RENAME TO devices;
        ALTER TABLE devices RENAME COLUMN device_id TO id;
        ALTER TABLE devices
            ADD COLUMN device_name text,
            ADD COLUMN device_desc text;
        ALTER TABLE dashboard_readings RENAME TO readings;
    END IF;
END;
$$;

-- Cuentas del panel y datos descriptivos de los dispositivos.
CREATE TABLE IF NOT EXISTS users (
    id bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    username text NOT NULL UNIQUE,
    password_hash text NOT NULL
);

CREATE TABLE IF NOT EXISTS devices (
    id text PRIMARY KEY,
    device_name text,
    device_desc text
);

-- El identificador de registro permite descartar reenvíos del mismo dispositivo.
CREATE TABLE IF NOT EXISTS readings (
    id bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    device_id text NOT NULL REFERENCES devices(id),
    record_id text,
    measured_at timestamptz,
    received_at timestamptz NOT NULL DEFAULT clock_timestamp(),
    -- La lectura en vivo puede usar la hora de recepción si falta la hora de medición.
    event_at timestamptz GENERATED ALWAYS AS (coalesce(measured_at, received_at)) STORED,
    temperature double precision,
    humidity double precision,
    gas double precision,
    calibrated boolean NOT NULL,
    UNIQUE (device_id, record_id)
);

-- La vista en vivo usa event_at. El historial y las gráficas usan measured_at.
CREATE INDEX IF NOT EXISTS dashboard_readings_device_event_idx
    ON readings (device_id, event_at DESC, id DESC);

CREATE INDEX IF NOT EXISTS dashboard_readings_device_measured_idx
    ON readings (device_id, measured_at)
    WHERE measured_at IS NOT NULL;
