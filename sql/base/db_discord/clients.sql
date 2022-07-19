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

 Date: 19/07/2022 11:40:04
*/

SET NAMES utf8mb4;
SET FOREIGN_KEY_CHECKS = 0;

-- ----------------------------
-- Table structure for clients
-- ----------------------------
DROP TABLE IF EXISTS `clients`;
CREATE TABLE `clients`  (
  `GuildID` bigint(20) NOT NULL DEFAULT 0,
  `GuildName` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NULL DEFAULT NULL,
  `MembersCount` int(11) NOT NULL DEFAULT 1,
  `InviteDate` datetime NOT NULL DEFAULT current_timestamp(),
  `AddedAtStartup` tinyint(1) NOT NULL DEFAULT 0,
  PRIMARY KEY (`GuildID`) USING BTREE
) ENGINE = InnoDB CHARACTER SET = utf8mb4 COLLATE = utf8mb4_general_ci ROW_FORMAT = Dynamic;

SET FOREIGN_KEY_CHECKS = 1;
