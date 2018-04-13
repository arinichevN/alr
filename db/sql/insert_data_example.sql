DELETE FROM "peer";
INSERT INTO "peer" VALUES('gwu18_1',49161,'127.0.0.1');
INSERT INTO "peer" VALUES('gwu22_1',49162,'127.0.0.1');
INSERT INTO "peer" VALUES('gwu18_2',49161,'127.0.0.1');
INSERT INTO "peer" VALUES('gwu22_2',49162,'127.0.0.1');
INSERT INTO "peer" VALUES('gwu74_1',49163,'127.0.0.1');
INSERT INTO "peer" VALUES('gwu59_1',49164,'127.0.0.1');
INSERT INTO "peer" VALUES('lck_1',49175,'127.0.0.1');
INSERT INTO "peer" VALUES('lgr_1',49172,'127.0.0.1');
INSERT INTO "peer" VALUES('alr_1',49174,'127.0.0.1');
INSERT INTO "peer" VALUES('chp_1',49176,'127.0.0.1');
INSERT INTO "peer" VALUES('chv_1',49177,'127.0.0.1');
INSERT INTO "peer" VALUES('regsmp_1',49192,'127.0.0.1');
INSERT INTO "peer" VALUES('regonf_1',49191,'127.0.0.1');
INSERT INTO "peer" VALUES('stp_1',49179,'127.0.0.1');
INSERT INTO "peer" VALUES('obj_1',49178,'127.0.0.1');
INSERT INTO "peer" VALUES('swr_1',49183,'127.0.0.1');
INSERT INTO "peer" VALUES('swf_1',49182,'127.0.0.1');

DELETE FROM "sensor_mapping";
INSERT INTO "sensor_mapping" VALUES(1,'obj_1',1);
INSERT INTO "sensor_mapping" VALUES(2,'obj_1',2);
INSERT INTO "sensor_mapping" VALUES(3,'obj_1',3);
INSERT INTO "sensor_mapping" VALUES(4,'obj_1',4);

DELETE FROM "em_mapping";
INSERT INTO "em_mapping" VALUES(1,'alr_1',1,1.0);
INSERT INTO "em_mapping" VALUES(2,'alr_1',2,1.0);
INSERT INTO "em_mapping" VALUES(3,'alr_1',3,1.0);
INSERT INTO "em_mapping" VALUES(4,'alr_1',4,1.0);

DELETE FROM "prog";
INSERT INTO "prog" VALUES(1,'test_1',1,1,20.0,1.0,5,10,1,1);
