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

 Date: 19/07/2022 11:39:59
*/

SET NAMES utf8mb4;
SET FOREIGN_KEY_CHECKS = 0;

-- ----------------------------
-- Table structure for account_banned
-- ----------------------------
DROP TABLE IF EXISTS `account_banned`;
CREATE TABLE `account_banned`  (
  `id` int(10) UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Account id',
  `bandate` int(10) UNSIGNED NOT NULL DEFAULT 0,
  `unbandate` int(10) UNSIGNED NOT NULL DEFAULT 0,
  `bannedby` varchar(50) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NOT NULL,
  `banreason` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NOT NULL,
  `active` tinyint(3) UNSIGNED NOT NULL DEFAULT 1,
  PRIMARY KEY (`id`, `bandate`) USING BTREE
) ENGINE = InnoDB CHARACTER SET = utf8mb4 COLLATE = utf8mb4_general_ci COMMENT = 'Ban List' ROW_FORMAT = Dynamic;

-- ----------------------------
-- Records of account_banned
-- ----------------------------

SET FOREIGN_KEY_CHECKS = 1;
