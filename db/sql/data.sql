CREATE TABLE "sensor_mapping" (
    "sensor_id" INTEGER PRIMARY KEY NOT NULL,
    "peer_id" TEXT NOT NULL,
    "remote_id" INTEGER NOT NULL
);
CREATE TABLE "prog" (
  "id" INTEGER PRIMARY KEY,
  "description" TEXT NOT NULL,
  "sensor_fts_id" INTEGER NOT NULL,
  "call_peer_id" TEXT NOT NULL,
  "good_value" REAL NOT NULL,
  "good_delta" REAL NOT NULL,
  "check_interval" INTEGER NOT NULL,
  "cope_duration" INTEGER NOT NULL,
  "phone_number_group_id" INTEGER NOT NULL,
  "sms" INTEGER NOT NULL,
  "ring" INTEGER NOT NULL,
  "enable" INTEGER NOT NULL,
  "load" INTEGER NOT NULL
);
