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

 Date: 19/07/2022 11:40:09
*/

SET NAMES utf8mb4;
SET FOREIGN_KEY_CHECKS = 0;

-- ----------------------------
-- Table structure for ip_banned
-- ----------------------------
DROP TABLE IF EXISTS `ip_banned`;
CREATE TABLE `ip_banned`  (
  `ip` varchar(15) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NOT NULL DEFAULT '127.0.0.1',
  `bandate` int(10) UNSIGNED NOT NULL,
  `unbandate` int(10) UNSIGNED NOT NULL,
  `bannedby` varchar(50) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NOT NULL DEFAULT '[Console]',
  `banreason` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NOT NULL DEFAULT 'no reason',
  `active` tinyint(3) UNSIGNED NOT NULL DEFAULT 1,
  PRIMARY KEY (`ip`, `bandate`) USING BTREE
) ENGINE = InnoDB CHARACTER SET = utf8mb4 COLLATE = utf8mb4_general_ci COMMENT = 'Banned IPs' ROW_FORMAT = Dynamic;

-- ----------------------------
-- Records of ip_banned
-- ----------------------------

SET FOREIGN_KEY_CHECKS = 1;
