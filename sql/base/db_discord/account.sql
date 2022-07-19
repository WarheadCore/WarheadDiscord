/*
 Navicat Premium Data Transfer

 Source Server         : #MariaDB_Local
 Source Server Type    : MariaDB
 Source Server Version : 100901
 Source Host           : localhost:3306
 Source Schema         : warhead_discord

 Target Server Type    : MariaDB
 Target Server Version : 100901
 File Encoding         : 65001

 Date: 19/07/2022 11:39:53
*/

SET NAMES utf8mb4;
SET FOREIGN_KEY_CHECKS = 0;

-- ----------------------------
-- Table structure for account
-- ----------------------------
DROP TABLE IF EXISTS `account`;
CREATE TABLE `account`  (
  `ID` int(11) NOT NULL AUTO_INCREMENT,
  `Name` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NULL DEFAULT NULL,
  `Salt` binary(32) NOT NULL,
  `Verifier` binary(32) NOT NULL,
  `GuildID` bigint(20) NOT NULL DEFAULT 0,
  `RealmName` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NULL DEFAULT NULL,
  `JoinDate` timestamp NOT NULL DEFAULT current_timestamp(),
  `LastIP` varchar(15) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NOT NULL DEFAULT '127.0.0.1',
  `LastAttemptIp` varchar(15) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NOT NULL DEFAULT '127.0.0.1',
  `CoreName` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NOT NULL DEFAULT 'WarheadCore',
  `CoreVersion` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NOT NULL DEFAULT 'unknown',
  `ModuleVersion` int(10) NOT NULL DEFAULT 100000,
  PRIMARY KEY (`ID`) USING BTREE
) ENGINE = InnoDB AUTO_INCREMENT = 1 CHARACTER SET = utf8mb4 COLLATE = utf8mb4_general_ci ROW_FORMAT = Dynamic;

SET FOREIGN_KEY_CHECKS = 1;
