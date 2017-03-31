
DROP SCHEMA if exists alr CASCADE;
CREATE SCHEMA alr;


CREATE TABLE alr.config
(
  app_class character varying(32) NOT NULL,
  db_public character varying(256) NOT NULL,
  udp_port character varying(32) NOT NULL,
  pid_path character varying(32) NOT NULL,
  udp_buf_size character varying(32) NOT NULL,
  db_data character varying(32) NOT NULL,
  db_log character varying(32) NOT NULL,
  cycle_duration_us character varying(32) NOT NULL,
  sms_peer_id character varying(32) NOT NULL,
  CONSTRAINT config_pkey PRIMARY KEY (app_class)
)
WITH (
  OIDS=FALSE
);

CREATE TABLE alr.prog
(
  id integer NOT NULL,
  description character varying(32) NOT NULL default 'descr',
  sensor_fts_id integer NOT NULL default 1,
  param_id integer NOT NULL default -1,
  active integer NOT NULL default 1,
  CONSTRAINT prog_pkey PRIMARY KEY (id)
)
WITH (
  OIDS=FALSE
);

CREATE TABLE alr.param
(
  id integer NOT NULL,
  good_value real NOT NULL default 0.0,
  good_delta real NOT NULL default 1.0,
  check_interval interval NOT NULL default '600',
  cope_duration interval NOT NULL default '3600',
  phone_number_group_id integer NOT NULL,
  CONSTRAINT param_pkey PRIMARY KEY (id)
)
WITH (
  OIDS=FALSE
);


CREATE TABLE alr.sensor_mapping
(
  app_class character varying(32) NOT NULL,
  sensor_id integer NOT NULL,
  peer_id character varying(32) NOT NULL,
  remote_id integer NOT NULL,
  CONSTRAINT sensor_mapping_pkey PRIMARY KEY (app_class, sensor_id, peer_id, remote_id)
)
WITH (
  OIDS=FALSE
);


