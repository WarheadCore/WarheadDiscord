-- ----------------------------
-- Table structure for account
-- ----------------------------
DROP TABLE IF EXISTS `account`;
CREATE TABLE `account`  (
  `ID` int(11) NOT NULL AUTO_INCREMENT,
  `Name` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NULL DEFAULT NULL,
  `RealmName` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NULL DEFAULT NULL,
  `Salt` binary(32) NOT NULL,
  `Verifier` binary(32) NOT NULL,
  `JoinDate` timestamp NOT NULL DEFAULT current_timestamp(),
  `LastIP` varchar(15) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NOT NULL DEFAULT '127.0.0.1',
  `LastAttemptIp` varchar(15) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NOT NULL DEFAULT '127.0.0.1',
  `CoreName` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NOT NULL DEFAULT 'WarheadCore',
  `CoreVersion` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NOT NULL DEFAULT 'unknown',
  `ModuleVersion` int(10) NOT NULL DEFAULT 100000,
  PRIMARY KEY (`ID`) USING BTREE
) ENGINE = InnoDB AUTO_INCREMENT = 15 CHARACTER SET = utf8mb4 COLLATE = utf8mb4_general_ci ROW_FORMAT = Dynamic;

-- ----------------------------
-- Records of account
-- ----------------------------
INSERT INTO `account` VALUES (1, 'WARHEAD', 'WarheadCore', 0xA1A1FE88971DA853F3C2B8E9F19A04A7AC98BC218326A8656BDECD3B5BC92375, 0x0FBB6FAC1062CF8ABF3C6514EB1C675CB591BFF389C2A539B6A8723A97013907, '2022-04-05 04:06:30', '127.0.0.1', '127.0.0.1', 'WarheadCore', 'unknown', 100000);
