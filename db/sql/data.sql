CREATE TABLE "peer" (
    "id" TEXT NOT NULL,
    "port" INTEGER NOT NULL,
    "ip_addr" TEXT NOT NULL
);
CREATE TABLE "sensor_mapping" (
    "sensor_id" INTEGER PRIMARY KEY NOT NULL,
    "peer_id" TEXT NOT NULL,
    "remote_id" INTEGER NOT NULL
);
CREATE TABLE "em_mapping" (
    "em_id" INTEGER PRIMARY KEY NOT NULL,
    "peer_id" TEXT NOT NULL,
    "remote_id" INTEGER NOT NULL,
    "pwm_rsl" REAL NOT NULL
);
CREATE TABLE "prog" (
  "id" INTEGER PRIMARY KEY,
  "description" TEXT NOT NULL,
  "sensor_id" INTEGER NOT NULL, --sensor_id from sensor_mapping
  "em_id" INTEGER NOT NULL, --em_id from em_mapping
  "good_value" REAL NOT NULL,
  "good_delta" REAL NOT NULL,
  "check_interval_sec" INTEGER NOT NULL,
  "cope_duration_sec" INTEGER NOT NULL,
  "enable" INTEGER NOT NULL,
  "load" INTEGER NOT NULL
);
