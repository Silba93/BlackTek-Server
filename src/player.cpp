// Copyright 2024 Black Tek Server Authors. All rights reserved.
// Use of this source code is governed by the GPL-2.0 License that can be found in the LICENSE file.

#include "otpch.h"
#include "tools.h"
#include "bed.h"
#include "chat.h"
#include "combat.h"
#include "configmanager.h"
#include "creatureevent.h"
#include "events.h"
#include "game.h"
#include "iologindata.h"
#include "monster.h"
#include "movement.h"
#include "scheduler.h"
#include "weapons.h"
#include "rewardchest.h"
#include "player.h"
#include "spells.h"
#include "accountmanager.h"

extern ConfigManager g_config;
extern Game g_game;
extern Chat* g_chat;
extern Vocations g_vocations;
extern MoveEvents* g_moveEvents;
extern Weapons* g_weapons;
extern CreatureEvents* g_creatureEvents;
extern Events* g_events;

MuteCountMap Player::muteCountMap;

uint32_t Player::playerAutoID = 0x10000000;

// Stuff needed for combat situations

using RawArea = std::vector<uint32_t>;
using RawAreaVec = std::vector<RawArea>;
using DeflectionEffectMap = gtl::flat_hash_map<int, RawAreaVec>;
using DeflectAreaMap = gtl::flat_hash_map<Direction, const DeflectionEffectMap>;

static const DeflectionEffectMap _StandardDeflectionMap = DeflectionEffectMap{
		{1, {{0, 0, 0, 0, 0,
			  0, 0, 0, 0, 0,
			  0, 0, 3, 0, 0,
			  0, 0, 0, 0, 0,
			  0, 0, 0, 0, 0}}},

		{2, {{0, 0, 0, 0, 0,
			  0, 0, 0, 0, 0,
			  0, 1, 3, 0, 0,
			  0, 0, 0, 0, 0,
			  0, 0, 0, 0, 0},
			 {0, 0, 0, 0, 0,
			  0, 0, 0, 0, 0,
			  0, 0, 3, 1, 0,
			  0, 0, 0, 0, 0,
			  0, 0, 0, 0, 0},
			 {0, 0, 0, 0, 0,
			  0, 0, 0, 0, 0,
			  0, 1, 2, 1, 0,
			  0, 0, 0, 0, 0,
			  0, 0, 0, 0, 0}}},

		{3, {{0, 0, 0, 0, 0,
			  0, 0, 0, 0, 0,
			  0, 1, 3, 1, 0,
			  0, 0, 0, 0, 0,
			  0, 0, 0, 0, 0}}},

		{4, {{0, 0, 0, 0, 0,
			  0, 0, 1, 0, 0,
			  0, 1, 3, 1, 0,
			  0, 0, 0, 0, 0,
			  0, 0, 0, 0, 0},
			 {0, 0, 0, 0, 0,
			  0, 1, 1, 1, 0,
			  0, 0, 3, 0, 0,
			  0, 0, 0, 0, 0,
			  0, 0, 0, 0, 0}}},

		{5, {{0, 0, 0, 0, 0,
			  1, 0, 0, 0, 1,
			  0, 1, 3, 1, 0,
			  0, 0, 0, 0, 0,
			  0, 0, 0, 0, 0},
			 {0, 0, 1, 0, 0,
			  0, 0, 1, 0, 0,
			  0, 1, 3, 1, 0,
			  0, 0, 0, 0, 0,
			  0, 0, 0, 0, 0},
			 {0, 0, 1, 0, 0,
			  0, 1, 1, 1, 0,
			  0, 0, 3, 0, 0,
			  0, 0, 0, 0, 0,
			  0, 0, 0, 0, 0}}},

		{6, {{0, 0, 0, 0, 0,
			  1, 0, 1, 0, 1,
			  0, 1, 3, 1, 0,
			  0, 0, 0, 0, 0,
			  0, 0, 0, 0, 0},
			 {0, 1, 0, 1, 0,
			  0, 0, 1, 0, 0,
			  0, 1, 3, 1, 0,
			  0, 0, 0, 0, 0,
			  0, 0, 0, 0, 0},
			 {0, 0, 0, 0, 0,
			  0, 0, 1, 0, 0,
			  0, 1, 3, 1, 0,
			  1, 0, 0, 0, 1,
			  0, 0, 0, 0, 0},
			 {0, 0, 0, 0, 0,
			  0, 1, 1, 1, 0,
			  0, 1, 3, 1, 0,
			  0, 0, 0, 0, 0,
			  0, 0, 0, 0, 0}}}
};


static const DeflectionEffectMap _DiagonalDeflectionMap = DeflectionEffectMap{
	{1,  {{0, 0, 0, 0, 0,
			  0, 0, 0, 0, 0,
			  0, 0, 3, 0, 0,
			  0, 0, 0, 0, 0,
			  0, 0, 0, 0, 0}}},
		{2, { // Double Diagonal
			{0, 0, 0, 0, 0,
			 0, 0, 1, 0, 0,
			 0, 0, 3, 0, 0,
			 0, 0, 0, 0, 0,
			 0, 0, 0, 0, 0},
			{0, 0, 0, 0, 0,
			 0, 0, 0, 0, 0,
			 0, 0, 3, 0, 0,
			 0, 0, 1, 0, 0,
			 0, 0, 0, 0, 0},
			{0, 0, 0, 0, 0,
			 0, 1, 0, 0, 0,
			 0, 0, 3, 0, 0,
			 0, 0, 0, 0, 0,
			 0, 0, 0, 0, 0},
			{0, 0, 0, 0, 0,
			 0, 0, 0, 0, 0,
			 0, 0, 2, 1, 0,
			 0, 0, 1, 0, 0,
			 0, 0, 0, 0, 0}
		}},
		{3, { // Triple Diagonal
			{0, 0, 0, 0, 0,
			 0, 0, 1, 0, 0,
			 0, 1, 3, 0, 0,
			 0, 0, 0, 0, 0,
			 0, 0, 0, 0, 0},
			{0, 0, 0, 0, 0,
			 0, 0, 0, 1, 0,
			 0, 0, 3, 0, 0,
			 0, 1, 0, 0, 0,
			 0, 0, 0, 0, 0},
			{0, 0, 0, 0, 0,
			 0, 0, 0, 0, 0,
			 0, 0, 3, 1, 0,
			 0, 0, 1, 0, 0,
			 0, 0, 0, 0, 0}
		}},
		{4, { // Quad Diagonal
			{0, 0, 0, 0, 0,
			 0, 1, 1, 0, 0,
			 0, 1, 3, 0, 0,
			 0, 0, 0, 0, 0,
			 0, 0, 0, 0, 0},
			{0, 0, 0, 0, 0,
			 0, 1, 0, 1, 0,
			 0, 0, 3, 0, 0,
			 0, 1, 0, 0, 0,
			 0, 0, 0, 0, 0},
			{1, 0, 0, 0, 0,
			 0, 1, 1, 0, 0,
			 0, 1, 2, 0, 0,
			 0, 0, 0, 0, 0,
			 0, 0, 0, 0, 0}
		}},
		{5, { // Quint Diagonal
			{0, 0, 1, 0, 0,
			 0, 0, 1, 0, 0,
			 1, 1, 3, 0, 0,
			 0, 0, 0, 0, 0,
			 0, 0, 0, 0, 0},
			{0, 0, 0, 0, 0,
			 0, 0, 1, 1, 0,
			 0, 1, 3, 0, 0,
			 0, 1, 0, 0, 0,
			 0, 0, 0, 0, 0},
			{0, 0, 1, 0, 0,
			 0, 1, 1, 0, 0,
			 1, 1, 2, 0, 0,
			 0, 0, 0, 0, 0,
			 0, 0, 0, 0, 0}
		}},
		{6, { // Sext Diagonal
			{0, 0, 1, 0, 0,
			 0, 1, 1, 0, 0,
			 1, 1, 3, 0, 0,
			 0, 0, 0, 0, 0,
			 0, 0, 0, 0, 0},
			{0, 0, 0, 0, 0,
			 0, 1, 1, 1, 0,
			 0, 1, 3, 0, 0,
			 0, 1, 0, 0, 0,
			 0, 0, 0, 0, 0},
			{1, 0, 0, 0, 0,
			 0, 1, 1, 1, 0,
			 0, 1, 2, 0, 0,
			 0, 1, 0, 0, 0,
			 0, 0, 0, 0, 0}
		}}
};

static const DeflectAreaMap DeflectAreas = DeflectAreaMap{
	{DIRECTION_NORTH, _StandardDeflectionMap},
	{DIRECTION_SOUTH, _StandardDeflectionMap},
	{DIRECTION_WEST, _StandardDeflectionMap},
	{DIRECTION_EAST, _StandardDeflectionMap},
	{DIRECTION_NORTHWEST, _DiagonalDeflectionMap},
	{DIRECTION_NORTHEAST, _DiagonalDeflectionMap},
	{DIRECTION_SOUTHWEST, _DiagonalDeflectionMap},
	{DIRECTION_SOUTHEAST, _DiagonalDeflectionMap},
};

gtl::flat_hash_map<int, RawAreaVec> deflectionAreas = {
	{1, {{0, 0, 0, 0, 0,
		  0, 0, 0, 0, 0,
		  0, 0, 3, 0, 0,
		  0, 0, 0, 0, 0,
		  0, 0, 0, 0, 0}}},

	{2, {{0, 0, 0, 0, 0,
		  0, 0, 0, 0, 0,
		  0, 1, 3, 0, 0,
		  0, 0, 0, 0, 0,
		  0, 0, 0, 0, 0},
		 {0, 0, 0, 0, 0,
		  0, 0, 0, 0, 0,
		  0, 0, 3, 1, 0,
		  0, 0, 0, 0, 0,
		  0, 0, 0, 0, 0},
		 {0, 0, 0, 0, 0,
		  0, 0, 0, 0, 0,
		  0, 1, 2, 1, 0,
		  0, 0, 0, 0, 0,
		  0, 0, 0, 0, 0}}},

	{3, {{0, 0, 0, 0, 0,
		  0, 0, 0, 0, 0,
		  0, 1, 3, 1, 0,
		  0, 0, 0, 0, 0,
		  0, 0, 0, 0, 0}}},

	{4, {{0, 0, 0, 0, 0,
		  0, 0, 1, 0, 0,
		  0, 1, 3, 1, 0,
		  0, 0, 0, 0, 0,
		  0, 0, 0, 0, 0},
		 {0, 0, 0, 0, 0,
		  0, 1, 1, 1, 0,
		  0, 0, 3, 0, 0,
		  0, 0, 0, 0, 0,
		  0, 0, 0, 0, 0}}},

	{5, {{0, 0, 0, 0, 0,
		  1, 0, 0, 0, 1,
		  0, 1, 3, 1, 0,
		  0, 0, 0, 0, 0,
		  0, 0, 0, 0, 0},
		 {0, 0, 1, 0, 0,
		  0, 0, 1, 0, 0,
		  0, 1, 3, 1, 0,
		  0, 0, 0, 0, 0,
		  0, 0, 0, 0, 0},
		 {0, 0, 1, 0, 0,
		  0, 1, 1, 1, 0,
		  0, 0, 3, 0, 0,
		  0, 0, 0, 0, 0,
		  0, 0, 0, 0, 0}}},

	{6, {{0, 0, 0, 0, 0,
		  1, 0, 1, 0, 1,
		  0, 1, 3, 1, 0,
		  0, 0, 0, 0, 0,
		  0, 0, 0, 0, 0},
		 {0, 1, 0, 1, 0,
		  0, 0, 1, 0, 0,
		  0, 1, 3, 1, 0,
		  0, 0, 0, 0, 0,
		  0, 0, 0, 0, 0},
		 {0, 0, 0, 0, 0,
		  0, 0, 1, 0, 0,
		  0, 1, 3, 1, 0,
		  0, 1, 0, 1, 0,
		  0, 0, 0, 0, 0},
		 {0, 0, 0, 0, 0,
		  0, 1, 1, 1, 0,
		  0, 1, 3, 1, 0,
		  0, 0, 0, 0, 0,
		  0, 0, 0, 0, 0}}}
};

gtl::flat_hash_map<int, RawAreaVec> deflectionDiagonalAreas = {
	{1,  {{0, 0, 0, 0, 0,
		  0, 0, 0, 0, 0,
		  0, 0, 3, 0, 0,
		  0, 0, 0, 0, 0,
		  0, 0, 0, 0, 0}}},
	{2, { // Double Diagonal
		{0, 0, 0, 0, 0,
		 0, 0, 0, 0, 0,
		 0, 0, 3, 0, 0,
		 0, 0, 1, 0, 0,
		 0, 0, 0, 0, 0},
		{0, 0, 0, 0, 0,
		 0, 0, 1, 0, 0,
		 0, 0, 3, 0, 0,
		 0, 0, 0, 0, 0,
		 0, 0, 0, 0, 0},
		{0, 0, 0, 0, 0,
		 0, 0, 0, 0, 0,
		 0, 0, 2, 1, 0,
		 0, 0, 1, 0, 0,
		 0, 0, 0, 0, 0}
	}},
	{3, { // Triple Diagonal
		{0, 0, 0, 0, 0,
		 0, 0, 0, 0, 0,
		 0, 0, 3, 0, 0,
		 0, 0, 0, 0, 0,
		 0, 0, 0, 0, 0},
		{0, 0, 0, 0, 0,
		 0, 0, 0, 0, 0,
		 0, 0, 3, 1, 0,
		 0, 0, 1, 1, 0,
		 0, 0, 0, 0, 0}
	}},
	{4, { // Quad Diagonal
		{0, 0, 0, 0, 0,
		 0, 1, 1, 0, 0,
		 0, 1, 3, 0, 0,
		 0, 0, 0, 0, 0,
		 0, 0, 0, 0, 0},
		{0, 0, 0, 0, 0,
		 0, 1, 0, 0, 0,
		 0, 0, 3, 1, 0,
		 0, 0, 1, 0, 0,
		 0, 0, 0, 0, 0}
	}},
	{5, { // Quint Diagonal
		{0, 0, 1, 0, 0,
		 0, 0, 1, 0, 0,
		 1, 1, 3, 0, 0,
		 0, 0, 0, 0, 0,
		 0, 0, 0, 0, 0},
		{0, 0, 0, 0, 0,
		 0, 0, 1, 1, 0,
		 0, 1, 3, 0, 0,
		 0, 1, 0, 0, 0,
		 0, 0, 0, 0, 0},
		{0, 0, 0, 0, 0,
		 0, 0, 0, 1, 0,
		 0, 0, 3, 1, 0,
		 0, 1, 1, 0, 0,
		 0, 0, 0, 0, 0}
	}},
	{6, { // Sext Diagonal
		{0, 0, 1, 0, 0,
		 0, 1, 1, 0, 0,
		 1, 1, 3, 0, 0,
		 0, 0, 0, 0, 0,
		 0, 0, 0, 0, 0},
		{0, 0, 0, 0, 0,
		 0, 0, 1, 1, 0,
		 0, 1, 3, 0, 0,
		 0, 1, 0, 0, 0,
		 0, 0, 0, 0, 0},
		{0, 0, 0, 0, 0,
		 0, 0, 0, 1, 0,
		 0, 0, 3, 1, 0,
		 0, 1, 1, 0, 0,
		 0, 0, 0, 0, 0}
	}}
};



// double Diagonal
RawAreaVec DeflectDiagonal2xAreas = {
	{
		0, 0, 0, 0, 0,
		0, 0, 0, 0, 0,
		0, 0, 3, 0, 0,
		0, 0, 1, 0, 0,
		0, 0, 0, 0, 0
	},

	{
		0, 0, 0, 0, 0,
		0, 0, 1, 0, 0,
		0, 0, 3, 0, 0,
		0, 0, 0, 0, 0,
		0, 0, 0, 0, 0
	},

	{
		0, 0, 0, 0, 0,
		0, 0, 0, 0, 0,
		0, 0, 2, 1, 0,
		0, 0, 1, 0, 0,
		0, 0, 0, 0, 0
	},
};

// triple Diagonal
RawAreaVec DeflectDiagonal3xAreas = {
	{
		0, 0, 0, 0, 0,
		0, 0, 1, 0, 0,
		0, 1, 3, 0, 0,
		0, 0, 0, 0, 0,
		0, 0, 0, 0, 0
	},

	{
		0, 0, 0, 0, 0,
		0, 0, 0, 0, 0,
		0, 0, 3, 1, 0,
		0, 0, 1, 0, 0,
		0, 0, 0, 0, 0
	}
};

// quad Diagonal
RawAreaVec DeflectDiagonal4xAreas = {
	{
		0, 0, 0, 0, 0,
		0, 1, 1, 0, 0,
		0, 1, 3, 0, 0,
		0, 0, 0, 0, 0,
		0, 0, 0, 0, 0
	},

	{
		0, 0, 0, 0, 0,
		0, 1, 0, 0, 0,
		0, 0, 3, 1, 0,
		0, 0, 1, 0, 0,
		0, 0, 0, 0, 0
	}
};

// quint Diagonal
RawAreaVec DeflectDiagonal5xAreas = {
	{
		0, 0, 1, 0, 0,
		0, 0, 1, 0, 0,
		1, 1, 3, 0, 0,
		0, 0, 0, 0, 0,
		0, 0, 0, 0, 0
	},

	{
		0, 0, 0, 0, 0,
		0, 0, 1, 1, 0,
		0, 1, 3, 0, 0,
		0, 1, 0, 0, 0,
		0, 0, 0, 0, 0
	},

	{
		0, 0, 0, 0, 0,
		0, 0, 0, 1, 0,
		0, 0, 3, 1, 0,
		0, 1, 1, 0, 0,
		0, 0, 0, 0, 0
	}
};

// sext Diagonal
RawAreaVec DeflectDiagonal6xAreas = {
	{
		0, 0, 1, 0, 0,
		0, 1, 1, 0, 0,
		1, 1, 3, 0, 0,
		0, 0, 0, 0, 0,
		0, 0, 0, 0, 0
	},

	{
		0, 0, 0, 0, 0,
		0, 0, 1, 1, 0,
		0, 1, 3, 0, 0,
		0, 1, 0, 0, 0,
		0, 0, 0, 0, 0
	},

	{
		0, 0, 0, 0, 0,
		0, 0, 0, 1, 0,
		0, 0, 3, 1, 0,
		0, 1, 1, 0, 0,
		0, 0, 0, 0, 0
	}
};


// single
RawArea Deflect1xArea = {
	0, 0, 0, 0, 0,
	0, 0, 0, 0, 0,
	0, 0, 3, 0, 0,
	0, 0, 0, 0, 0,
	0, 0, 0, 0, 0
};

// doubles
RawAreaVec Deflect2xAreas = {
	{
		0, 0, 0, 0, 0,
		0, 0, 0, 0, 0,
		0, 1, 3, 0, 0,
		0, 0, 0, 0, 0,
		0, 0, 0, 0, 0
	},

	{
		0, 0, 0, 0, 0,
		0, 0, 0, 0, 0,
		0, 0, 3, 1, 0,
		0, 0, 0, 0, 0,
		0, 0, 0, 0, 0
	},

	{
		0, 0, 0, 0, 0,
		0, 0, 0, 0, 0,
		0, 1, 2, 1, 0,
		0, 0, 0, 0, 0,
		0, 0, 0, 0, 0
	},
};


// triple
RawArea Deflect3xArea = {
	0, 0, 0, 0, 0,
	0, 0, 0, 0, 0,
	0, 1, 3, 1, 0,
	0, 0, 0, 0, 0,
	0, 0, 0, 0, 0
};



// quad
RawAreaVec Deflect4xAreas = {
	{
		0, 0, 0, 0, 0,
		0, 0, 1, 0, 0,
		0, 1, 3, 1, 0,
		0, 0, 0, 0, 0,
		0, 0, 0, 0, 0
	},

	{
		0, 0, 0, 0, 0,
		0, 1, 1, 1, 0,
		0, 0, 3, 0, 0,
		0, 0, 0, 0, 0,
		0, 0, 0, 0, 0
	}
};



// quint (5's)
RawAreaVec Deflect5xAreas = {
	{
		0, 0, 0, 0, 0,
		1, 0, 0, 0, 1,
		0, 1, 3, 1, 0,
		0, 0, 0, 0, 0,
		0, 0, 0, 0, 0
	},

	{
		0, 0, 1, 0, 0,
		0, 0, 1, 0, 0,
		0, 1, 3, 1, 0,
		0, 0, 0, 0, 0,
		0, 0, 0, 0, 0
	},

	{
		0, 0, 1, 0, 0,
		0, 1, 1, 1, 0,
		0, 0, 3, 0, 0,
		0, 0, 0, 0, 0,
		0, 0, 0, 0, 0
	}
};


// sext (6's)
RawAreaVec Deflect6xAreas = {
	{
		0, 0, 0, 0, 0,
		1, 0, 1, 0, 1,
		0, 1, 3, 1, 0,
		0, 0, 0, 0, 0,
		0, 0, 0, 0, 0
	},

	{
		0, 1, 0, 1, 0,
		0, 0, 1, 0, 0,
		0, 1, 3, 1, 0,
		0, 0, 0, 0, 0,
		0, 0, 0, 0, 0
	},

	{
		0, 0, 0, 0, 0,
		0, 0, 1, 0, 0,
		0, 1, 3, 1, 0,
		0, 1, 0, 1, 0,
		0, 0, 0, 0, 0
	},

	{
		0, 0, 0, 0, 0,
		0, 1, 1, 1, 0,
		0, 1, 3, 1, 0,
		0, 0, 0, 0, 0,
		0, 0, 0, 0, 0
	}
};



static std::vector<uint32_t> GetDeflectArea(uint32_t targets) {
	switch (targets) {
	case 1:
		return Deflect1xArea;
	case 2:
		return Deflect2xAreas[uniform_random(0, Deflect2xAreas.size() - 1)];
	case 3:
		return Deflect3xArea;
	case 4:
		return Deflect4xAreas[uniform_random(0, Deflect4xAreas.size() - 1)];
	case 5:
		return Deflect5xAreas[uniform_random(0, Deflect5xAreas.size() - 1)];
	default:
		return Deflect6xAreas[uniform_random(0, Deflect6xAreas.size() - 1)];
	}
}

static std::vector<uint32_t> GetDiaganolDeflectArea(uint32_t targets) {
	switch (targets) {
	case 1:
		return Deflect1xArea;
	case 2:
		return DeflectDiagonal2xAreas[uniform_random(0, DeflectDiagonal2xAreas.size() - 1)];
	case 3:
		return DeflectDiagonal3xAreas[uniform_random(0, DeflectDiagonal3xAreas.size() - 1)];
	case 4:
		return DeflectDiagonal4xAreas[uniform_random(0, DeflectDiagonal4xAreas.size() - 1)];
	case 5:
		return DeflectDiagonal5xAreas[uniform_random(0, DeflectDiagonal5xAreas.size() - 1)];
	default:
		return DeflectDiagonal6xAreas[uniform_random(0, DeflectDiagonal6xAreas.size() - 1)];
	}
}


Player::Player(ProtocolGame_ptr p) :
	lastPing(OTSYS_TIME()), lastPong(lastPing), client(std::move(p)), inbox(std::make_shared<Inbox>(ITEM_INBOX)), storeInbox(std::make_shared<StoreInbox>(ITEM_STORE_INBOX))
{

}

Player::~Player()
{
	for (const auto& item : inventory) {
		if (item) {
			item->clearParent();
		}
	}

	if (depotLocker) {
		depotLocker->removeInbox(inbox);
	}
	
	storeInbox->clearParent();

	setWriteItem(nullptr);
	setEditHouse(nullptr);
}

bool Player::setVocation(const uint16_t vocId)
{
	const auto& voc = g_vocations.getVocation(vocId);
	if (!voc) {
		return false;
	}
	vocation = voc;
	
	// todo, add custom skills to player if they don't have them already
	// if they do have them, compare multiplier, difficculty, and threshold
	// decide if that should be updated or not...

	updateRegeneration();
	setBaseSpeed(voc->getBaseSpeed());
	updateBaseSpeed();
	g_game.changeSpeed(this->getPlayer(), 0);
	return true;
}

bool Player::isPushable() const
{
	if (hasFlag(PlayerFlag_CannotBePushed)) {
		return false;
	}
	return Creature::isPushable();
}

std::string Player::getDescription(const int32_t lookDistance) const
{
	std::ostringstream s;

	if (lookDistance == -1) {
		s << "yourself.";

		if (group->access) {
			s << " You are " << group->name << '.';
		} else if (vocation->getId() != VOCATION_NONE) {
			s << " You are " << vocation->getVocDescription() << '.';
		} else {
			s << " You have no vocation.";
		}
	} else {
		s << name;
		if (!group->access) {
			s << " (Level " << level << ')';
		}
		s << '.';

		if (sex == PLAYERSEX_FEMALE) {
			s << " She";
		} else {
			s << " He";
		}

		if (group->access) {
			s << " is " << group->name << '.';
		} else if (vocation->getId() != VOCATION_NONE) {
			s << " is " << vocation->getVocDescription() << '.';
		} else {
			s << " has no vocation.";
		}
	}

	if (party) {
		if (lookDistance == -1) {
			s << " Your party has ";
		} else if (sex == PLAYERSEX_FEMALE) {
			s << " She is in a party with ";
		} else {
			s << " He is in a party with ";
		}

		size_t memberCount = party->getMemberCount() + 1;
		if (memberCount == 1) {
			s << "1 member and ";
		} else {
			s << memberCount << " members and ";
		}

		size_t invitationCount = party->getInvitationCount();
		if (invitationCount == 1) {
			s << "1 pending invitation.";
		} else {
			s << invitationCount << " pending invitations.";
		}
	}

	if (!guild || !guildRank) {
		return s.str();
	}

	if (lookDistance == -1) {
		s << " You are ";
	} else if (sex == PLAYERSEX_FEMALE) {
		s << " She is ";
	} else {
		s << " He is ";
	}

	s << guildRank->name << " of the " << guild->getName();
	if (!guildNick.empty()) {
		s << " (" << guildNick << ')';
	}

	size_t memberCount = guild->getMemberCount();
	if (memberCount == 1) {
		s << ", which has 1 member, " << guild->getMembersOnline().size() << " of them online.";
	} else {
		s << ", which has " << memberCount << " members, " << guild->getMembersOnline().size() << " of them online.";
	}
	return s.str();
}

ItemPtr Player::getInventoryItem(const slots_t slot) const
{
	if (slot < CONST_SLOT_FIRST || slot > CONST_SLOT_LAST) {
		return nullptr;
	}
	return inventory[slot];
}

ItemPtr Player::getInventoryItem(const uint32_t slot) const
{
	if (slot < CONST_SLOT_FIRST || slot > CONST_SLOT_LAST) {
		return nullptr;
	}
	return inventory[slot];
}

bool Player::isInventorySlot(const slots_t slot)
{
	return slot >= CONST_SLOT_FIRST && slot <= CONST_SLOT_LAST;
}	

void Player::addConditionSuppressions(const uint32_t conditions)
{
	conditionSuppressions |= conditions;
}

void Player::removeConditionSuppressions(const uint32_t conditions)
{
	conditionSuppressions &= ~conditions;
}

ItemPtr Player::getWeapon(const slots_t slot, const bool ignoreAmmo) const
{
	auto item = inventory[slot];
	if (!item) {
		return nullptr;
	}

	WeaponType_t weaponType = item->getWeaponType();
	if (weaponType == WEAPON_NONE || weaponType == WEAPON_SHIELD || weaponType == WEAPON_AMMO || weaponType == WEAPON_QUIVER) {
		return nullptr;
	}

	if (!ignoreAmmo && weaponType == WEAPON_DISTANCE) {
		const ItemType& itemType = Item::items[item->getID()];
		if (itemType.ammoType != AMMO_NONE) {
			const auto& ammoItem = inventory[CONST_SLOT_AMMO];
			if (!ammoItem || ammoItem->getAmmoType() != itemType.ammoType) {
				// no ammo item was found, search for quiver instead
				const auto& quiver = inventory[CONST_SLOT_RIGHT] ? inventory[CONST_SLOT_RIGHT]->getContainer() : nullptr;
				if (!quiver || quiver->getItem()->getWeaponType() != WEAPON_QUIVER) {
					// no quiver equipped
					return nullptr;
				}

				for (ContainerIterator containerItem = quiver->iterator(); containerItem.hasNext();
					containerItem.advance()) {
					if (itemType.ammoType == (*containerItem)->getAmmoType()) {
						const auto& weapon = g_weapons->getWeapon(*containerItem);
						if (weapon && weapon->ammoCheck(this->getPlayer())) {
							return *containerItem;
						}
					}
				}

				// no valid ammo was found in quiver
				return nullptr;
			}
			item = ammoItem;
		}
	}
	return item;
}

ItemPtr Player::getWeapon(bool ignoreAmmo/* = false*/) const
{
	if (const auto& leftHandItem = getWeapon(CONST_SLOT_LEFT, ignoreAmmo)) {
		return leftHandItem;
	}
	
	if (const auto& rightHandItem = getWeapon(CONST_SLOT_RIGHT, ignoreAmmo)) {
		return rightHandItem;
	}
	return nullptr;
}

WeaponType_t Player::getWeaponType() const
{
	const auto& item = getWeapon();
	if (!item) {
		return WEAPON_NONE;
	}
	return item->getWeaponType();
}

int32_t Player::getWeaponSkill(const ItemConstPtr& item) const
{
	if (!item) {
		return getSkillLevel(SKILL_FIST);
	}

	int32_t attackSkill;

	const WeaponType_t weaponType = item->getWeaponType();
	switch (weaponType) {
		case WEAPON_SWORD: {
			attackSkill = getSkillLevel(SKILL_SWORD);
			break;
		}

		case WEAPON_CLUB: {
			attackSkill = getSkillLevel(SKILL_CLUB);
			break;
		}

		case WEAPON_AXE: {
			attackSkill = getSkillLevel(SKILL_AXE);
			break;
		}

		case WEAPON_DISTANCE: {
			attackSkill = getSkillLevel(SKILL_DISTANCE);
			break;
		}

		default: {
			attackSkill = 0;
			break;
		}
	}
	return attackSkill;
}

int32_t Player::getArmor() const
{
	int32_t armor = 0;

	static constexpr slots_t armorSlots[] = {CONST_SLOT_HEAD, CONST_SLOT_NECKLACE, CONST_SLOT_ARMOR, CONST_SLOT_LEGS, CONST_SLOT_FEET, CONST_SLOT_RING};
	for (slots_t slot : armorSlots) {
		if (const auto& inventoryItem = inventory[slot]) {
			armor += inventoryItem->getArmor();
		}
	}
	return static_cast<int32_t>(armor * vocation->armorMultiplier);
}

int32_t Player::getDefense() const
{
	int32_t defenseSkill = getSkillLevel(SKILL_FIST);
	int32_t defenseValue = 7;

	ItemPtr leftHand = getInventoryItem(CONST_SLOT_LEFT);
	ItemPtr rightHand = getInventoryItem(CONST_SLOT_RIGHT);
	ItemPtr shield;
	ItemPtr weapon;

	// We aren't going to waste precious CPU cycles or memory trying to determine which item
	// has highest defense if you happen to have two shields, two quivers, or a shield
	// and a quiver in each hand... in this case we are just gonna end up using left hand
	// So if this is something that concerns you, you can go back to looping and using switches
	// and storing variables to keep track of highest defense and do the math and all that yourself.
	if (leftHand and (leftHand->getWeaponType() == WEAPON_SHIELD or leftHand->getWeaponType() == WEAPON_QUIVER))
	{
		shield = leftHand;
		if (rightHand)
		{
			weapon = rightHand;
		}
	}
	else if (rightHand and (rightHand->getWeaponType() == WEAPON_SHIELD or rightHand->getWeaponType() == WEAPON_QUIVER))
	{
		shield = rightHand;
		if (leftHand)
		{
			weapon = leftHand;
		}
	}
	else 
	{
		if (rightHand and rightHand->getWeaponType() != WEAPON_SHIELD and rightHand->getWeaponType() != WEAPON_QUIVER)
		{
			weapon = rightHand;
		} 
		else if (leftHand and leftHand->getWeaponType() != WEAPON_SHIELD and leftHand->getWeaponType() != WEAPON_QUIVER)
		{
			weapon = leftHand;
		}
	}
	
	if (weapon) {
		defenseValue = weapon->getDefense() + weapon->getExtraDefense();
		defenseSkill = getWeaponSkill(weapon);
	}

	if (shield) {
		defenseValue = weapon != nullptr ? shield->getDefense() + weapon->getExtraDefense() : shield->getDefense();
		defenseSkill = getSkillLevel(SKILL_SHIELD);
	}

	if (defenseSkill == 0) {
		switch (fightMode) {
			case FIGHTMODE_ATTACK:
			case FIGHTMODE_BALANCED:
				return 1;

			case FIGHTMODE_DEFENSE:
				return 2;
		}
	}

	return (defenseSkill / 4. + 2.23) * defenseValue * 0.15 * getDefenseFactor() * vocation->defenseMultiplier;
}

uint32_t Player::getAttackSpeed() const
{
	const auto& weapon = getWeapon(true);
	if (!weapon || weapon->getAttackSpeed() == 0) {
		return vocation->getAttackSpeed();
	}

	return weapon->getAttackSpeed();
}

float Player::getAttackFactor() const
{
	switch (fightMode) {
		case FIGHTMODE_ATTACK: return 1.0f;
		case FIGHTMODE_BALANCED: return 1.2f;
		case FIGHTMODE_DEFENSE: return 2.0f;
		default: return 1.0f;
	}
}

float Player::getDefenseFactor() const
{
	switch (fightMode) {
		case FIGHTMODE_ATTACK: return (OTSYS_TIME() - lastAttack) < getAttackSpeed() ? 0.5f : 1.0f;
		case FIGHTMODE_BALANCED: return (OTSYS_TIME() - lastAttack) < getAttackSpeed() ? 0.75f : 1.0f;
		case FIGHTMODE_DEFENSE: return 1.0f;
		default: return 1.0f;
	}
}

uint16_t Player::getClientIcons() const
{
	uint16_t icons = 0;
	for (const auto& condition : conditions) {
		if (!isSuppress(condition->getType())) {
			icons |= condition->getIcons();
		}
	}

	if (pzLocked) {
		icons |= ICON_REDSWORDS;
	}

	if (tile.lock() && tile.lock()->hasFlag(TILESTATE_PROTECTIONZONE)) {
		icons |= ICON_PIGEON;

		// Don't show ICON_SWORDS if player is in protection zone.
		icons &= ~ICON_SWORDS;
	}

	// Game client debugs with 10 or more icons
	// so let's prevent that from happening.
	std::bitset<20> icon_bitset(static_cast<uint64_t>(icons));
	for (size_t pos = 0, bits_set = icon_bitset.count(); bits_set >= 10; ++pos) {
		if (icon_bitset[pos]) {
			icon_bitset.reset(pos);
			--bits_set;
		}
	}
	return icon_bitset.to_ulong();
}

void Player::updateInventoryWeight()
{
	if (hasFlag(PlayerFlag_HasInfiniteCapacity)) {
		return;
	}

	inventoryWeight = 0;
	for (int i = CONST_SLOT_FIRST; i <= CONST_SLOT_LAST; ++i) {
		if (const auto& item = inventory[i]) {
			inventoryWeight += item->getWeight();
		}
	}

	if (const auto& storeInbox = getStoreInbox()) {
		inventoryWeight += storeInbox->getWeight();
	}
}

void Player::addSkillAdvance(skills_t skill, uint64_t count)
{
	uint64_t currReqTries = vocation->getReqSkillTries(skill, skills[skill].level);
	uint64_t nextReqTries = vocation->getReqSkillTries(skill, skills[skill].level + 1);
	if (currReqTries >= nextReqTries) {
		//player has reached max skill
		return;
	}

	g_events->eventPlayerOnGainSkillTries(this->getPlayer(), skill, count);
	if (count == 0) {
		return;
	}

	bool sendUpdateSkills = false;
	while ((skills[skill].tries + count) >= nextReqTries) {
		count -= nextReqTries - skills[skill].tries;
		skills[skill].level++;
		skills[skill].tries = 0;
		skills[skill].percent = 0;

		sendTextMessage(MESSAGE_EVENT_ADVANCE, fmt::format("You advanced to {:s} level {:d}.", getSkillName(skill), skills[skill].level));

		g_creatureEvents->playerAdvance(this->getPlayer(), skill, (skills[skill].level - 1), skills[skill].level);

		sendUpdateSkills = true;
		currReqTries = nextReqTries;
		nextReqTries = vocation->getReqSkillTries(skill, skills[skill].level + 1);
		if (currReqTries >= nextReqTries) {
			count = 0;
			break;
		}
	}

	skills[skill].tries += count;

	uint32_t newPercent;
	if (nextReqTries > currReqTries) {
		newPercent = Player::getPercentLevel(skills[skill].tries, nextReqTries);
	} else {
		newPercent = 0;
	}

	if (skills[skill].percent != newPercent) {
		skills[skill].percent = newPercent;
		sendUpdateSkills = true;
	}

	if (sendUpdateSkills) {
		sendSkills();
	}
}

void Player::removeSkillTries(const skills_t skill, uint64_t count, const bool notify/* = false*/)
{
	uint16_t oldLevel = skills[skill].level;
	uint8_t oldPercent = skills[skill].percent;

	while (count > skills[skill].tries) {
		count -= skills[skill].tries;

		if (skills[skill].level <= MINIMUM_SKILL_LEVEL) {
			skills[skill].level = MINIMUM_SKILL_LEVEL;
			skills[skill].tries = 0;
			count = 0;
			break;
		}

		skills[skill].tries = vocation->getReqSkillTries(skill, skills[skill].level);
		skills[skill].level--;
	}

	skills[skill].tries = std::max<int32_t>(0, skills[skill].tries - count);
	skills[skill].percent = Player::getPercentLevel(skills[skill].tries, vocation->getReqSkillTries(skill, skills[skill].level));

	if (notify) {
		bool sendUpdateSkills = false;
		if (oldLevel != skills[skill].level) {
			sendTextMessage(MESSAGE_EVENT_ADVANCE, fmt::format("You were downgraded to {:s} level {:d}.", getSkillName(skill), skills[skill].level));
			sendUpdateSkills = true;
		}

		if (sendUpdateSkills || oldPercent != skills[skill].percent) {
			sendSkills();
		}
	}
}

void Player::setVarStats(const stats_t stat, const int32_t modifier)
{
	varStats[stat] += modifier;

	switch (stat) {
		case STAT_MAXHITPOINTS: {
			if (getHealth() > getMaxHealth()) {
				Creature::changeHealth(getMaxHealth() - getHealth());
			} else {
				g_game.addCreatureHealth(this->getPlayer());
			}
			break;
		}

		case STAT_MAXMANAPOINTS: {
			if (getMana() > getMaxMana()) {
				changeMana(getMaxMana() - getMana());
			}
			break;
		}

		default: {
			break;
		}
	}
}

int32_t Player::getDefaultStats(const stats_t stat) const
{
	switch (stat) {
		case STAT_MAXHITPOINTS: return healthMax;
		case STAT_MAXMANAPOINTS: return manaMax;
		case STAT_MAGICPOINTS: return getBaseMagicLevel();
		default: return 0;
	}
}

void Player::addContainer(const uint8_t cid, const ContainerPtr& container)
{
	if (cid > 0xF) {
		return;
	}

	if (const auto& it = openContainers.find(cid); it != openContainers.end()) {
		OpenContainer& openContainer = it->second;
		openContainer.container = container;
		openContainer.index = 0;
	} else {
		OpenContainer openContainer;
		openContainer.container = container;
		openContainer.index = 0;
		openContainers[cid] = openContainer;
	}
}

void Player::closeContainer(const uint8_t cid)
{
	const auto& it = openContainers.find(cid);
	if (it == openContainers.end()) {
		return;
	}

	OpenContainer openContainer = it->second;
	ContainerPtr container = openContainer.container;
	openContainers.erase(it);
}

void Player::setContainerIndex(const uint8_t cid, const uint16_t index)
{
	const auto& it = openContainers.find(cid);
	if (it == openContainers.end()) {
		return;
	}
	it->second.index = index;
}

ContainerPtr Player::getContainerByID(const uint8_t cid)
{
	const auto& it = openContainers.find(cid);
	if (it == openContainers.end()) {
		return nullptr;
	}
	return it->second.container;
}

int8_t Player::getContainerID(const ContainerConstPtr& container) const
{
	for (const auto& it : openContainers) {
		if (it.second.container == container) {
			return it.first;
		}
	}
	return -1;
}

uint16_t Player::getContainerIndex(const uint8_t cid) const
{
	const auto& it = openContainers.find(cid);
	if (it == openContainers.end()) {
		return 0;
	}
	return it->second.index;
}

bool Player::canOpenCorpse(const uint32_t ownerId) const
{
	return getID() == ownerId || (party && party->canOpenCorpse(ownerId));
}

uint16_t Player::getLookCorpse() const
{
	if (sex == PLAYERSEX_FEMALE) {
		return ITEM_FEMALE_CORPSE;
	} else {
		return ITEM_MALE_CORPSE;
	}
}

void Player::addStorageValue(const uint32_t key, const int32_t value, const bool isLogin/* = false*/)
{
	if (IS_IN_KEYRANGE(key, RESERVED_RANGE)) {
		if (IS_IN_KEYRANGE(key, OUTFITS_RANGE)) {
			outfits.emplace_back(
				value >> 16,
				value & 0xFF
			);
			return;
		} else if (IS_IN_KEYRANGE(key, MOUNTS_RANGE)) {
			// do nothing
		} else {
			std::cout << "Warning: unknown reserved key: " << key << " player: " << getName() << std::endl;
			return;
		}
	}

	if (value != -1) {
		int32_t oldValue;
		getStorageValue(key, oldValue);

		storageMap[key] = value;

		if (!isLogin) {
			auto currentFrameTime = g_dispatcher.getDispatcherCycle();
			if (lastQuestlogUpdate != currentFrameTime && g_game.quests.isQuestStorage(key, value, oldValue)) {
				lastQuestlogUpdate = currentFrameTime;
				sendTextMessage(MESSAGE_EVENT_ADVANCE, "Your questlog has been updated.");
			}
		}
	} else {
		storageMap.erase(key);
	}
}

bool Player::getStorageValue(const uint32_t key, int32_t& value) const
{
	const auto it = storageMap.find(key);
	if (it == storageMap.end()) {
		value = -1;
		return false;
	}

	value = it->second;
	return true;
}

bool Player::canSee(const Position& pos) const
{
	if (!client) {
		return false;
	}
	return client->canSee(pos);
}

bool Player::canSeeCreature(const CreatureConstPtr& creature) const
{
	if (creature == getCreature()) {
		return true;
	}

	if (creature->isInGhostMode() && !canSeeGhostMode(creature)) {
		return false;
	}

	if (!creature->getPlayer() && !canSeeInvisibility() && creature->isInvisible()) {
		return false;
	}
	return true;
}

bool Player::canSeeGhostMode(const CreatureConstPtr&) const
{
	return group->access;
}

bool Player::canWalkthrough(const CreatureConstPtr& creature) const
{
	if (group->access || creature->isInGhostMode() || (g_config.getBoolean(ConfigManager::ALLOW_WALKTHROUGH) && creature->getPlayer() && creature->getPlayer()->isAccessPlayer())) {
		return true;
	}

	const auto& player = creature->getPlayer();
	if (!player || !g_config.getBoolean(ConfigManager::ALLOW_WALKTHROUGH)) {
		return false;
	}

	const auto& playerTile = player->getTile();
	if (!playerTile || (!playerTile->hasFlag(TILESTATE_PROTECTIONZONE) && player->getLevel() > static_cast<uint32_t>(g_config.getNumber(ConfigManager::PROTECTION_LEVEL)))) {
		return false;
	}

	const auto& playerTileGround = playerTile->getGround();
	if (!playerTileGround || !playerTileGround->hasWalkStack()) {
		return false;
	}

	auto thisPlayer = std::static_pointer_cast<Player>(std::const_pointer_cast<SharedObject>(shared_from_this()));
	if ((OTSYS_TIME() - lastWalkthroughAttempt) > 2000) {
		thisPlayer->setLastWalkthroughAttempt(OTSYS_TIME());
		return false;
	}

	if (creature->getPosition() != lastWalkthroughPosition) {
		thisPlayer->setLastWalkthroughPosition(creature->getPosition());
		return false;
	}

	thisPlayer->setLastWalkthroughPosition(creature->getPosition());
	return true;
}

bool Player::canWalkthroughEx(const CreatureConstPtr& creature) const
{
	if (group->access) {
		return true;
	}

	const auto& player = creature->getPlayer();
	if (!player || !g_config.getBoolean(ConfigManager::ALLOW_WALKTHROUGH)) {
		return false;
	}

	const auto& playerTile = player->getTile();
	return playerTile && (playerTile->hasFlag(TILESTATE_PROTECTIONZONE) || player->getLevel() <= static_cast<uint32_t>(g_config.getNumber(ConfigManager::PROTECTION_LEVEL)));
}

void Player::onReceiveMail() const
{
	if (isNearDepotBox()) {
		sendTextMessage(MESSAGE_EVENT_ADVANCE, "New mail has arrived.");
	}
}

bool Player::isNearDepotBox() const
{
	const Position& pos = getPosition();
	for (int32_t cx = -NOTIFY_DEPOT_BOX_RANGE; cx <= NOTIFY_DEPOT_BOX_RANGE; ++cx) {
		for (int32_t cy = -NOTIFY_DEPOT_BOX_RANGE; cy <= NOTIFY_DEPOT_BOX_RANGE; ++cy) {
			const auto& tile = g_game.map.getTile(pos.x + cx, pos.y + cy, pos.z);
			if (!tile) {
				continue;
			}

			if (tile->hasFlag(TILESTATE_DEPOT)) {
				return true;
			}
		}
	}
	return false;
}

DepotChestPtr Player::getDepotChest(uint32_t depotId, const bool autoCreate)
{
	auto it = depotChests.find(depotId);
	if (it != depotChests.end()) {
		return it->second;
	}

	if (!autoCreate) {
		return nullptr;
	}

	uint16_t depotItemId = getDepotBoxId(depotId);
	if (depotItemId == 0) {
		return nullptr;
	}

	it = depotChests.emplace(depotId, std::make_shared<DepotChest>(depotItemId)).first;
	it->second->setMaxDepotItems(getMaxDepotItems());
	return it->second;
}

DepotLockerPtr& Player::getDepotLocker()
{
	if (!depotLocker) {
		depotLocker = std::make_shared<DepotLocker>(ITEM_LOCKER1);
		depotLocker->internalAddThing(Item::CreateItem(ITEM_MARKET));
		depotLocker->internalAddThing(inbox);
		if (const DepotChestPtr depotChest = std::make_shared<DepotChest>(ITEM_DEPOT, false)) {
			// adding in reverse to align them from first to last
			for (int16_t depotId = depotChest->capacity(); depotId >= 0; --depotId) {
				if (DepotChestPtr box = getDepotChest(depotId, true)) {
					depotChest->internalAddThing(box);
				}
			}

			depotLocker->internalAddThing(depotChest);
		}
	}
	return depotLocker;
}

uint32_t Player::getDepotItemCount()
{
	uint32_t counter = 0;

	for (const auto item : getDepotLocker()->getItems(true)) {
		
		++counter;

		std::string itemName = item->getName();

		if (itemName.find("depot") != std::string::npos) {
			--counter;
		}
		if (itemName.find("inbox") != std::string::npos) {
			--counter;
		}
		if (itemName.find("market") != std::string::npos) {
			--counter;
		}
	}
	return counter;
}

RewardChestPtr& Player::getRewardChest()
{
	if (!rewardChest) {
		rewardChest = std::make_shared<RewardChest>(ITEM_REWARD_CHEST);
	}
	return rewardChest;
}

void Player::sendCancelMessage(ReturnValue message) const
{
	sendCancelMessage(getReturnMessage(message));
}

void Player::sendStats()
{
	if (client) {
		client->sendStats();
		lastStatsTrainingTime = getOfflineTrainingTime() / 60 / 1000;
	}
}

void Player::sendPing()
{
	int64_t timeNow = OTSYS_TIME();

	bool hasLostConnection = false;
	if ((timeNow - lastPing) >= 5000) {
		lastPing = timeNow;
		if (client) {
			client->sendPing();
		} else {
			hasLostConnection = true;
		}
	}

	int64_t noPongTime = timeNow - lastPong;
	if ((hasLostConnection || noPongTime >= 7000) && getAttackedCreature() && getAttackedCreature()->getPlayer()) {
		setAttackedCreature(nullptr);
	}

	int32_t noPongKickTime = vocation->getNoPongKickTime();
	if (pzLocked && noPongKickTime < 60000) {
		noPongKickTime = 60000;
	}

	if (noPongTime >= noPongKickTime) {
		if (isConnecting || getTile()->hasFlag(TILESTATE_NOLOGOUT)) {
			return;
		}

		if (!g_creatureEvents->playerLogout(this->getPlayer())) {
			return;
		}

		if (client) {
			client->logout(true, true);
		}
		g_game.removeCreature(this->getPlayer(), true);
	}
}

ItemPtr Player::getWriteItem(uint32_t& windowTextId, uint16_t& maxWriteLen)
{
	windowTextId = this->windowTextId;
	maxWriteLen = this->maxWriteLen;
	return writeItem;
}

void Player::setWriteItem(const ItemPtr& item, uint16_t maxWriteLen /*= 0*/)
{
	windowTextId++;
	if (item) {
		writeItem = item;
		this->maxWriteLen = maxWriteLen;
	} else {
		writeItem = nullptr;
		this->maxWriteLen = 0;
	}
}

House* Player::getEditHouse(uint32_t& windowTextId, uint32_t& listId) const
{
	windowTextId = this->windowTextId;
	listId = this->editListId;
	return editHouse;
}

void Player::setEditHouse(House* house, uint32_t listId /*= 0*/)
{
	windowTextId++;
	editHouse = house;
	editListId = listId;
}

void Player::sendHouseWindow(House* house, uint32_t listId) const
{
	if (!client) {
		return;
	}

	std::string text;
	if (house->getAccessList(listId, text)) {
		client->sendHouseWindow(windowTextId, text);
	}
}

//container
void Player::sendAddContainerItem(const ContainerConstPtr& container, ItemPtr& item) const
{
	if (!client) {
		return;
	}

	for (const auto& it : openContainers) {
		const OpenContainer& openContainer = it.second;
		if (openContainer.container != container) {
			continue;
		}

		uint16_t slot = openContainer.index;
		if (container->getItem()->getID() == ITEM_BROWSEFIELD) {
			uint16_t containerSize = container->size() - 1;
			uint16_t pageEnd = openContainer.index + container->capacity() - 1;
			if (containerSize > pageEnd) {
				slot = pageEnd;
				item = container->getItemByIndex(pageEnd);
			} else {
				slot = containerSize;
			}
		} else if (openContainer.index >= container->capacity()) {
			item = container->getItemByIndex(openContainer.index);
		}

		if (item) {
			client->sendAddContainerItem(it.first, slot, item);
		}
	}
}

void Player::sendUpdateContainerItem(const ContainerConstPtr& container, uint16_t slot, const ItemConstPtr& newItem) const
{
	if (!client) {
		return;
	}

	for (const auto& it : openContainers) {
		const OpenContainer& openContainer = it.second;
		if (openContainer.container != container) {
			continue;
		}

		if (slot < openContainer.index) {
			continue;
		}

		if (const uint16_t pageEnd = openContainer.index + container->capacity(); slot >= pageEnd) {
			continue;
		}

		client->sendUpdateContainerItem(it.first, slot, newItem);
	}
}

void Player::sendRemoveContainerItem(const ContainerConstPtr& container, const uint16_t slot)
{
	if (!client) {
		return;
	}

	for (auto& it : openContainers) {
		OpenContainer& openContainer = it.second;
		if (openContainer.container != container) {
			continue;
		}

		uint16_t& firstIndex = openContainer.index;
		if (firstIndex > 0 && firstIndex >= container->size() - 1) {
			firstIndex -= container->capacity();
			sendContainer(it.first, container, false, firstIndex);
		}

		client->sendRemoveContainerItem(it.first, std::max<uint16_t>(slot, firstIndex), container->getItemByIndex(container->capacity() + firstIndex));
	}
}

void Player::onUpdateTileItem(const TilePtr& tile, const Position& pos, const ItemPtr& oldItem,
                              const ItemType& oldType, const ItemPtr& newItem, const ItemType& newType)
{
	Creature::onUpdateTileItem(tile, pos, oldItem, oldType, newItem, newType);

	if (oldItem != newItem) {
		onRemoveTileItem(tile, pos, oldType, oldItem);
	}

	if (tradeState != TRADE_TRANSFER) {
		if (tradeItem && oldItem == tradeItem) {
			g_game.internalCloseTrade(this->getPlayer());
		}
	}
}

void Player::onRemoveTileItem(const TilePtr& tile, const Position& pos, const ItemType& iType,
                              const ItemPtr& item)
{
	Creature::onRemoveTileItem(tile, pos, iType, item);

	if (tradeState != TRADE_TRANSFER) {
		checkTradeState(item);

		if (tradeItem) {
			const auto& container = item->getContainer();
			if (container && container->isHoldingItem(tradeItem)) {
				g_game.internalCloseTrade(this->getPlayer());
			}
		}
	}
}

void Player::onCreatureAppear(const CreaturePtr& creature, bool isLogin)
{
	Creature::onCreatureAppear(creature, isLogin);

	if (isLogin and creature == getCreature()) {
		if (not isAccountManager()) {

			sendItems();
			for (int32_t slot = CONST_SLOT_FIRST; slot <= CONST_SLOT_LAST; ++slot) {
				if (const auto& item = inventory[slot]) {
					item->startDecaying();
					g_moveEvents->onPlayerEquip(this->getPlayer(), item, static_cast<slots_t>(slot), false);
				}
			}

			for (const auto& condition : storedConditionList) {
				addCondition(condition);
			}
			storedConditionList.clear();

			updateRegeneration();
			if (const auto& bed = g_game.getBedBySleeper(guid)) {
				bed->wakeUp(this->getPlayer());
			}

			if (guild) {
				guild->addMember(this->getPlayer());
			}

			int32_t offlineTime;
			if (getLastLogout() != 0) {
				// Not counting more than 21 days to prevent overflow when multiplying with 1000 (for milliseconds).
				offlineTime = std::min<int32_t>(time(nullptr) - getLastLogout(), 86400 * 21);
			} else {
				offlineTime = 0;
			}

			for (const auto& condition : getMuteConditions()) {
				condition->setTicks(condition->getTicks() - (offlineTime * 1000));
				if (condition->getTicks() <= 0) {
					removeCondition(condition);
				}
			}
			g_game.checkPlayersRecord();
			IOLoginData::updateOnlineStatus(guid, true);
		}
		else {
			storedConditionList.clear();
			const auto& acc_manager = getPlayer();
			g_game.doAccountManagerLogin(acc_manager);
		}

		if (g_config.getBoolean(ConfigManager::PLAYER_CONSOLE_LOGS)) {
			std::cout << name << " has logged in." << std::endl;
		}
	}
}

void Player::onAttackedCreatureDisappear(const bool isLogout)
{
	sendCancelTarget();

	if (!isLogout) {
		sendTextMessage(MESSAGE_STATUS_SMALL, "Target lost.");
	}
}

void Player::onFollowCreatureDisappear(const bool isLogout)
{
	sendCancelTarget();

	if (!isLogout) {
		sendTextMessage(MESSAGE_STATUS_SMALL, "Target lost.");
	}
}

void Player::onChangeZone(const ZoneType_t zone)
{
	if (zone == ZONE_PROTECTION) {
		if (getAttackedCreature() && !hasFlag(PlayerFlag_IgnoreProtectionZone)) {
			setAttackedCreature(nullptr);
			onAttackedCreatureDisappear(false);
		}

		if (!group->access && isMounted()) {
			dismount();
			g_game.internalCreatureChangeOutfit(this->getPlayer(), defaultOutfit);
			wasMounted = true;
		}
	} else {
		if (wasMounted) {
			toggleMount(true);
			wasMounted = false;
		}
	}

	g_game.updateCreatureWalkthrough(this->getPlayer());
	sendIcons();
}

void Player::onAttackedCreatureChangeZone(const ZoneType_t zone)
{
	if (zone == ZONE_PROTECTION) {
		if (!hasFlag(PlayerFlag_IgnoreProtectionZone)) {
			setAttackedCreature(nullptr);
			onAttackedCreatureDisappear(false);
		}
	} else if (zone == ZONE_NOPVP) {
		if (getAttackedCreature()->getPlayer()) {
			if (!hasFlag(PlayerFlag_IgnoreProtectionZone)) {
				setAttackedCreature(nullptr);
				onAttackedCreatureDisappear(false);
			}
		}
	} else if (zone == ZONE_NORMAL) {
		//attackedCreature can leave a pvp zone if not pzlocked
		if (g_game.getWorldType() == WORLD_TYPE_NO_PVP) {
			if (getAttackedCreature()->getPlayer()) {
				setAttackedCreature(nullptr);
				onAttackedCreatureDisappear(false);
			}
		}
	}
}

void Player::onRemoveCreature(const CreaturePtr& creature, bool isLogout)
{
	Creature::onRemoveCreature(creature, isLogout);

	if (creature == getCreature()) {
		for (int32_t slot = CONST_SLOT_FIRST; slot <= CONST_SLOT_LAST; ++slot) {
			if (const auto& item = inventory[slot]) {
				g_moveEvents->onPlayerDeEquip(this->getPlayer(), item, static_cast<slots_t>(slot));
			}
		}
		if (isLogout) {
			loginPosition = getPosition();
		}

		lastLogout = time(nullptr);

		if (eventWalk != 0) {
			setFollowCreature(nullptr);
		}

		if (tradePartner) {
			g_game.internalCloseTrade(this->getPlayer());
		}

		closeShopWindow();

		clearPartyInvitations();

		if (party) {
			party->leaveParty(this->getPlayer(), true);
		}

		g_chat->removeUserFromAllChannels(this->getPlayer());

		if (g_config.getBoolean(ConfigManager::PLAYER_CONSOLE_LOGS)) {
			std::cout << getName() << " has logged out." << std::endl;
		}

		if (guild) {
			guild->removeMember(this->getPlayer());
		}

		if (not isAccountManager()) {
			IOLoginData::updateOnlineStatus(guid, false);
		}

		bool saved = false;
		for (uint32_t tries = 0; tries < 3; ++tries) {
			if (IOLoginData::savePlayer(this->getPlayer())) {
				saved = true;
				break;
			}
		}

		if (!saved) {
			std::cout << "Error while saving player: " << getName() << std::endl;
		}
	}
}

void Player::openShopWindow(const NpcPtr& npc, const std::list<ShopInfo>& shop)
{
	shopItemList = shop;
	sendShop(npc);
	sendSaleItemList();
}

bool Player::closeShopWindow(bool sendCloseShopWindow /*= true*/)
{
	//unreference callbacks
	int32_t onBuy;
	int32_t onSell;

	const auto& npc = getShopOwner(onBuy, onSell);
	if (!npc) {
		shopItemList.clear();
		return false;
	}

	setShopOwner(nullptr, -1, -1);
	npc->onPlayerEndTrade(this->getPlayer(), onBuy, onSell);

	if (sendCloseShopWindow) {
		sendCloseShop();
	}

	shopItemList.clear();
	return true;
}

void Player::onWalk(Direction& dir)
{
	Creature::onWalk(dir);
	setNextActionTask(nullptr);
	setNextAction(OTSYS_TIME() + getStepDuration(dir));
}

void Player::onCreatureMove(const CreaturePtr& creature, const TilePtr& newTile, const Position& newPos,
                            const TilePtr& oldTile, const Position& oldPos, bool teleport)
{
	Creature::onCreatureMove(creature, newTile, newPos, oldTile, oldPos, teleport);

	if (hasFollowPath && (creature == getFollowCreature() || (creature == this->getPlayer() && getFollowCreature()))) {
		isUpdatingPath = false;
		g_dispatcher.addTask(createTask([id = getID()]() { g_game.updateCreatureWalk(id); }));
	}

	if (creature != this->getPlayer()) {
		return;
	}

	if (tradeState != TRADE_TRANSFER) {
		//check if we should close trade
		if (tradeItem && !Position::areInRange<1, 1, 0>(tradeItem->getPosition(), getPosition())) {
			g_game.internalCloseTrade(this->getPlayer());
		}

		if (tradePartner && !Position::areInRange<2, 2, 0>(tradePartner->getPosition(), getPosition())) {
			g_game.internalCloseTrade(this->getPlayer());
		}
	}

	// close modal windows
	if (!modalWindows.empty()) {
		// TODO: This shouldn't be hard-coded
		for (uint32_t modalWindowId : modalWindows) {
			if (modalWindowId == std::numeric_limits<uint32_t>::max()) {
				sendTextMessage(MESSAGE_EVENT_ADVANCE, "Offline training aborted.");
				break;
			}
		}
		modalWindows.clear();
	}

	// leave market
	if (inMarket) {
		inMarket = false;
	}

	if (party) {
		party->updateSharedExperience();
	}

	if (teleport || oldPos.z != newPos.z) {
		int32_t ticks = g_config.getNumber(ConfigManager::STAIRHOP_DELAY);
		if (ticks > 0) {
			if (const auto& condition = Condition::createCondition(CONDITIONID_DEFAULT, CONDITION_PACIFIED, ticks, 0)) {
				addCondition(condition);
			}
		}
	}
}

//container
void Player::onAddContainerItem(ItemPtr item)
{
	checkTradeState(item);
}

void Player::onUpdateContainerItem(ContainerPtr container, ItemPtr oldItem, ItemPtr newItem)
{
	if (oldItem != newItem) {
		onRemoveContainerItem(container, oldItem);
	}

	if (tradeState != TRADE_TRANSFER) {
		checkTradeState(oldItem);
	}
}

void Player::onRemoveContainerItem(ContainerPtr container, ItemPtr item)
{
	if (tradeState != TRADE_TRANSFER) {
		checkTradeState(item);

		if (tradeItem) {
			if (tradeItem->getParent() != container && container->isHoldingItem(tradeItem)) {
				g_game.internalCloseTrade(this->getPlayer());
			}
		}
	}
}

void Player::onCloseContainer(ContainerPtr container)
{
	if (!client) {
		return;
	}

	for (auto it : openContainers) {
		if (it.second.container == container) {
			client->sendCloseContainer(it.first);
		}
	}
}

void Player::onSendContainer(ContainerPtr container)
{
	if (!client) {
		return;
	}

	const bool hasParent = container->hasParent();
	for (auto it : openContainers) {
		OpenContainer openContainer = it.second;
		if (openContainer.container == container) {
			client->sendContainer(it.first, container, hasParent, openContainer.index);
		}
	}
}

//inventory
void Player::onUpdateInventoryItem(ItemPtr oldItem, ItemPtr newItem)
{
	if (oldItem != newItem) {
		onRemoveInventoryItem(oldItem);
	}

	if (tradeState != TRADE_TRANSFER) {
		checkTradeState(oldItem);
	}
}

void Player::onRemoveInventoryItem(ItemPtr item)
{
	if (tradeState != TRADE_TRANSFER) {
		checkTradeState(item);

		if (tradeItem) {
			const auto& container = item->getContainer();
			if (container && container->isHoldingItem(tradeItem)) {
				g_game.internalCloseTrade(this->getPlayer());
			}
		}
	}
}

void Player::checkTradeState(const ItemPtr& item)
{
	if (!tradeItem || tradeState == TRADE_TRANSFER) {
		return;
	}

	if (tradeItem == item) {
		g_game.internalCloseTrade(this->getPlayer());
	} else {
		auto container = std::dynamic_pointer_cast<Container>(item->getParent());
		while (container) {
			if (container == tradeItem) {
				g_game.internalCloseTrade(this->getPlayer());
				break;
			}

			container = std::dynamic_pointer_cast<Container>(container->getParent());
		}
	}
}

void Player::setNextWalkActionTask(SchedulerTask* task)
{
	if (walkTaskEvent != 0) {
		g_scheduler.stopEvent(walkTaskEvent);
		walkTaskEvent = 0;
	}

	delete walkTask;
	walkTask = task;
}

void Player::setNextActionTask(SchedulerTask* task, bool resetIdleTime /*= true */)
{
	if (actionTaskEvent != 0) {
		g_scheduler.stopEvent(actionTaskEvent);
		actionTaskEvent = 0;
	}

	if (task) {
		actionTaskEvent = g_scheduler.addEvent(task);
		if (resetIdleTime) {
			this->resetIdleTime();
		}
	}
}

uint32_t Player::getNextActionTime() const
{
	return std::max<int64_t>(SCHEDULER_MINTICKS, nextAction - OTSYS_TIME());
}

void Player::onThink(const uint32_t interval)
{
	Creature::onThink(interval);

	sendPing();

	MessageBufferTicks += interval;
	if (MessageBufferTicks >= 1500) {
		MessageBufferTicks = 0;
		addMessageBuffer();
	}

	if (!getTile()->hasFlag(TILESTATE_NOLOGOUT) && !isAccessPlayer()) {
		idleTime += interval;
		const int32_t kickAfterMinutes = g_config.getNumber(ConfigManager::KICK_AFTER_MINUTES);
		if (idleTime > (kickAfterMinutes * 60000) + 60000) {
			kickPlayer(true);
		} else if (client && idleTime == 60000 * kickAfterMinutes) {
			client->sendTextMessage(TextMessage(MESSAGE_STATUS_WARNING, fmt::format("There was no variation in your behaviour for {:d} minutes. You will be disconnected in one minute if there is no change in your actions until then.", kickAfterMinutes)));
		}
	}

	// if (isImbued()) { // TODO: Reimplement a check like this to first see if player has any items, then items with imbuements before decaying.
		for (int32_t slot = CONST_SLOT_FIRST; slot <= CONST_SLOT_LAST; ++slot) {
			const auto& item = inventory[slot];
			if (item && item->hasImbuements()) {
				item->decayImbuements(hasCondition(CONDITION_INFIGHT));
				sendSkills();
				sendStats();
			}
		}
	// } // part of the above TODO:

	if (g_game.getWorldType() != WORLD_TYPE_PVP_ENFORCED) {
		checkSkullTicks(interval / 1000);
	}

	addOfflineTrainingTime(interval);
	if (lastStatsTrainingTime != getOfflineTrainingTime() / 60 / 1000) {
		sendStats();
	}
}

uint32_t Player::isMuted() const
{
	if (hasFlag(PlayerFlag_CannotBeMuted)) {
		return 0;
	}

	int32_t muteTicks = 0;
	for (const auto& condition : conditions) {
		if (condition->getType() == CONDITION_MUTED && condition->getTicks() > muteTicks) {
			muteTicks = condition->getTicks();
		}
	}
	return static_cast<uint32_t>(muteTicks) / 1000;
}

void Player::addMessageBuffer()
{
	if (MessageBufferCount > 0 && g_config.getNumber(ConfigManager::MAX_MESSAGEBUFFER) != 0 && !hasFlag(PlayerFlag_CannotBeMuted)) {
		--MessageBufferCount;
	}
}

void Player::removeMessageBuffer()
{
	if (hasFlag(PlayerFlag_CannotBeMuted)) {
		return;
	}

	const int32_t maxMessageBuffer = g_config.getNumber(ConfigManager::MAX_MESSAGEBUFFER);
	if (maxMessageBuffer != 0 && MessageBufferCount <= maxMessageBuffer + 1) {
		if (++MessageBufferCount > maxMessageBuffer) {
			uint32_t muteCount = 1;
			if (const auto& it = muteCountMap.find(guid); it != muteCountMap.end()) {
				muteCount = it->second;
			}

			uint32_t muteTime = 5 * muteCount * muteCount;
			muteCountMap[guid] = muteCount + 1;
			const auto& condition = Condition::createCondition(CONDITIONID_DEFAULT, CONDITION_MUTED, muteTime * 1000, 0);
			addCondition(condition);

			sendTextMessage(MESSAGE_STATUS_SMALL, fmt::format("You are muted for {:d} seconds.", muteTime));
		}
	}
}

void Player::drainHealth(const CreaturePtr& attacker, const int32_t damage)
{
	Creature::drainHealth(attacker, damage);
	sendStats();
}

void Player::drainMana(const CreaturePtr& attacker, const int32_t manaLoss)
{
	onAttacked();
	changeMana(-manaLoss);

	if (attacker) {
		addDamagePoints(attacker, manaLoss);
	}

	sendStats();
}

void Player::addManaSpent(uint64_t amount)
{
	if (hasFlag(PlayerFlag_NotGainMana)) {
		return;
	}

	uint64_t currReqMana = vocation->getReqMana(magLevel);
	uint64_t nextReqMana = vocation->getReqMana(magLevel + 1);
	if (currReqMana >= nextReqMana) {
		//player has reached max magic level
		return;
	}

	g_events->eventPlayerOnGainSkillTries(this->getPlayer(), SKILL_MAGLEVEL, amount);
	if (amount == 0) {
		return;
	}

	bool sendUpdateStats = false;
	while ((manaSpent + amount) >= nextReqMana) {
		amount -= nextReqMana - manaSpent;

		magLevel++;
		manaSpent = 0;

		sendTextMessage(MESSAGE_EVENT_ADVANCE, fmt::format("You advanced to magic level {:d}.", magLevel));

		g_creatureEvents->playerAdvance(this->getPlayer(), SKILL_MAGLEVEL, magLevel - 1, magLevel);

		sendUpdateStats = true;
		currReqMana = nextReqMana;
		nextReqMana = vocation->getReqMana(magLevel + 1);
		if (currReqMana >= nextReqMana) {
			return;
		}
	}

	manaSpent += amount;

	uint8_t oldPercent = magLevelPercent;
	if (nextReqMana > currReqMana) {
		magLevelPercent = Player::getPercentLevel(manaSpent, nextReqMana);
	} else {
		magLevelPercent = 0;
	}

	if (oldPercent != magLevelPercent) {
		sendUpdateStats = true;
	}

	if (sendUpdateStats) {
		sendStats();
	}
}

void Player::removeManaSpent(uint64_t amount, const bool notify/* = false*/)
{
	if (amount == 0) {
		return;
	}

	const uint32_t oldLevel = magLevel;
	const uint8_t oldPercent = magLevelPercent;

	while (amount > manaSpent && magLevel > 0) {
		amount -= manaSpent;
		manaSpent = vocation->getReqMana(magLevel);
		magLevel--;
	}

	manaSpent -= amount;

	uint64_t nextReqMana = vocation->getReqMana(magLevel + 1);
	if (nextReqMana > vocation->getReqMana(magLevel)) {
		magLevelPercent = Player::getPercentLevel(manaSpent, nextReqMana);
	} else {
		magLevelPercent = 0;
	}

	if (notify) {
		bool sendUpdateStats = false;
		if (oldLevel != magLevel) {
			sendTextMessage(MESSAGE_EVENT_ADVANCE, fmt::format("You were downgraded to magic level {:d}.", magLevel));
			sendUpdateStats = true;
		}

		if (sendUpdateStats || oldPercent != magLevelPercent) {
			sendStats();
		}
	}
}

void Player::addExperience(const CreaturePtr& source, uint64_t exp, bool sendText/* = false*/)
{
	uint64_t currLevelExp = Player::getExpForLevel(level);
	uint64_t nextLevelExp = Player::getExpForLevel(level + 1);
	uint64_t rawExp = exp;
	if (currLevelExp >= nextLevelExp) {
		//player has reached max level
		levelPercent = 0;
		sendStats();
		return;
	}

	g_events->eventPlayerOnGainExperience(this->getPlayer(), source, exp, rawExp);
	if (exp == 0) {
		return;
	}

	experience += exp;

	if (sendText) {
		const std::string expString = std::to_string(exp) + (exp != 1 ? " experience points." : " experience point.");

		TextMessage message(MESSAGE_EXPERIENCE, "You gained " + expString);
		message.position = position;
		message.primary.value = exp;
		message.primary.color = TEXTCOLOR_WHITE_EXP;
		sendTextMessage(message);

		SpectatorVec spectators;
		g_game.map.getSpectators(spectators, position, false, true);
		spectators.erase(this->getPlayer());
		if (!spectators.empty()) {
			message.type = MESSAGE_EXPERIENCE_OTHERS;
			message.text = getName() + " gained " + expString;
			for (const auto& spectator : spectators) {
				assert(std::dynamic_pointer_cast<Player>(spectator) != nullptr);
				std::static_pointer_cast<Player>(spectator)->sendTextMessage(message);
			}
		}
	}

	uint32_t prevLevel = level;
	while (experience >= nextLevelExp) {
		++level;
		healthMax += vocation->getHPGain();
		health += vocation->getHPGain();
		manaMax += vocation->getManaGain();
		mana += vocation->getManaGain();
		capacity += vocation->getCapGain();

		currLevelExp = nextLevelExp;
		nextLevelExp = Player::getExpForLevel(level + 1);
		if (currLevelExp >= nextLevelExp) {
			//player has reached max level
			break;
		}
	}

	if (prevLevel != level) {
		health = getMaxHealth();
		mana = getMaxMana();

		updateBaseSpeed();
		setBaseSpeed(getBaseSpeed());

		g_game.changeSpeed(this->getPlayer(), 0);
		g_game.addCreatureHealth(this->getPlayer());

		const uint32_t protectionLevel = static_cast<uint32_t>(g_config.getNumber(ConfigManager::PROTECTION_LEVEL));
		if (prevLevel < protectionLevel && level >= protectionLevel) {
			g_game.updateCreatureWalkthrough(this->getPlayer());
		}

		if (party) {
			party->updateSharedExperience();
		}

		g_creatureEvents->playerAdvance(this->getPlayer(), SKILL_LEVEL, prevLevel, level);

		sendTextMessage(MESSAGE_EVENT_ADVANCE, fmt::format("You advanced from Level {:d} to Level {:d}.", prevLevel, level));
	}

	if (nextLevelExp > currLevelExp) {
		levelPercent = Player::getPercentLevel(experience - currLevelExp, nextLevelExp - currLevelExp);
	} else {
		levelPercent = 0;
	}
	sendStats();
}

void Player::removeExperience(uint64_t exp, const bool sendText/* = false*/)
{
	if (experience == 0 || exp == 0) {
		return;
	}

	g_events->eventPlayerOnLoseExperience(this->getPlayer(), exp);
	if (exp == 0) {
		return;
	}

	uint64_t lostExp = experience;
	experience = std::max<int64_t>(0, experience - exp);

	if (sendText) {
		lostExp -= experience;

		const std::string expString = std::to_string(lostExp) + (lostExp != 1 ? " experience points." : " experience point.");

		TextMessage message(MESSAGE_EXPERIENCE, "You lost " + expString);
		message.position = position;
		message.primary.value = lostExp;
		message.primary.color = TEXTCOLOR_RED;
		sendTextMessage(message);

		SpectatorVec spectators;
		g_game.map.getSpectators(spectators, position, false, true);
		spectators.erase(this->getPlayer());
		if (!spectators.empty()) {
			message.type = MESSAGE_EXPERIENCE_OTHERS;
			message.text = getName() + " lost " + expString;
			for (const auto& spectator : spectators) {
				assert(std::dynamic_pointer_cast<Player>(spectator) != nullptr);
				std::static_pointer_cast<Player>(spectator)->sendTextMessage(message);
			}
		}
	}

	uint32_t oldLevel = level;
	uint64_t currLevelExp = Player::getExpForLevel(level);

	while (level > 1 && experience < currLevelExp) {
		--level;
		healthMax = std::max<int32_t>(0, healthMax - vocation->getHPGain());
		manaMax = std::max<int32_t>(0, manaMax - vocation->getManaGain());
		capacity = std::max<int32_t>(0, capacity - vocation->getCapGain());
		currLevelExp = Player::getExpForLevel(level);
	}

	if (oldLevel != level) {
		health = getMaxHealth();
		mana = getMaxMana();

		updateBaseSpeed();
		setBaseSpeed(getBaseSpeed());

		g_game.changeSpeed(this->getPlayer(), 0);
		g_game.addCreatureHealth(this->getPlayer());

		const uint32_t protectionLevel = static_cast<uint32_t>(g_config.getNumber(ConfigManager::PROTECTION_LEVEL));
		if (oldLevel >= protectionLevel && level < protectionLevel) {
			g_game.updateCreatureWalkthrough(this->getPlayer());
		}

		if (party) {
			party->updateSharedExperience();
		}

		sendTextMessage(MESSAGE_EVENT_ADVANCE, fmt::format("You were downgraded from Level {:d} to Level {:d}.", oldLevel, level));
	}

	uint64_t nextLevelExp = Player::getExpForLevel(level + 1);
	if (nextLevelExp > currLevelExp) {
		levelPercent = Player::getPercentLevel(experience - currLevelExp, nextLevelExp - currLevelExp);
	} else {
		levelPercent = 0;
	}
	sendStats();
}

uint8_t Player::getPercentLevel(const uint64_t count, uint64_t nextLevelCount)
{
	if (nextLevelCount == 0) {
		return 0;
	}

	uint8_t result = (count * 100) / nextLevelCount;
	if (result > 100) {
		return 0;
	}
	return result;
}

void Player::onBlockHit()
{
	if (shieldBlockCount > 0) {
		--shieldBlockCount;

		if (hasShield()) {
			addSkillAdvance(SKILL_SHIELD, 1);
		}
	}
}

void Player::onAttackedCreatureBlockHit(BlockType_t blockType)
{
	lastAttackBlockType = blockType;

	switch (blockType) {
		case BLOCK_NONE: {
			addAttackSkillPoint = true;
			bloodHitCount = 30;
			shieldBlockCount = 30;
			break;
		}

		case BLOCK_DEFENSE:
		case BLOCK_ARMOR: {
			//need to draw blood every 30 hits
			if (bloodHitCount > 0) {
				addAttackSkillPoint = true;
				--bloodHitCount;
			} else {
				addAttackSkillPoint = false;
			}
			break;
		}

		default: {
			addAttackSkillPoint = false;
			break;
		}
	}
}

bool Player::hasShield() const
{
	auto item = inventory[CONST_SLOT_LEFT];
	if (item && item->getWeaponType() == WEAPON_SHIELD) {
		return true;
	}

	item = inventory[CONST_SLOT_RIGHT];
	if (item && item->getWeaponType() == WEAPON_SHIELD) {
		return true;
	}
	return false;
}

BlockType_t Player::blockHit(const CreaturePtr& attacker, CombatType_t combatType, int32_t& damage,
                             bool checkDefense /* = false*/, bool checkArmor /* = false*/, bool field /* = false*/, bool ignoreResistances /* = false*/)
{
	BlockType_t blockType = Creature::blockHit(attacker, combatType, damage, checkDefense, checkArmor, field, ignoreResistances);

	if (attacker && combatType != COMBAT_HEALING) {
		sendCreatureSquare(attacker, SQ_COLOR_BLACK);
	}

	if (damage <= 0) {
		damage = 0;
		blockType = BLOCK_ARMOR;
	}
	return blockType;
}

uint32_t Player::getIP() const
{
	if (client) {
		return client->getIP();
	}

	return 0;
}

void Player::death(const CreaturePtr& lastHitCreature)
{
	loginPosition = town->getTemplePosition();

	if (skillLoss) {
		uint8_t unfairFightReduction = 100;
		const bool lastHitPlayer = Player::lastHitIsPlayer(lastHitCreature);

		if (lastHitPlayer) {
			uint32_t sumLevels = 0;
			uint32_t inFightTicks = g_config.getNumber(ConfigManager::PZ_LOCKED);
			for (const auto& it : damageMap) {
				CountBlock_t cb = it.second;
				if ((OTSYS_TIME() - cb.ticks) <= inFightTicks) {
					if (const auto& damageDealer = g_game.getPlayerByID(it.first)) {
						sumLevels += damageDealer->getLevel();
					}
				}
			}

			if (sumLevels > level) {
				double reduce = level / static_cast<double>(sumLevels);
				unfairFightReduction = std::max<uint8_t>(20, std::floor((reduce * 100) + 0.5));
			}
		}

		//Magic level loss
		uint64_t sumMana = 0;
		for (uint32_t i = 1; i <= magLevel; ++i) {
			sumMana += vocation->getReqMana(i);
		}

		double deathLossPercent = getLostPercent() * (unfairFightReduction / 100.);
		removeManaSpent(static_cast<uint64_t>((sumMana + manaSpent) * deathLossPercent), false);

		//Skill loss
		for (uint8_t i = SKILL_FIRST; i <= SKILL_LAST; ++i) { //for each skill
			uint64_t sumSkillTries = 0;
			for (uint16_t c = MINIMUM_SKILL_LEVEL + 1; c <= skills[i].level; ++c) { //sum up all required tries for all skill levels
				sumSkillTries += vocation->getReqSkillTries(i, c);
			}

			sumSkillTries += skills[i].tries;

			removeSkillTries(static_cast<skills_t>(i), sumSkillTries * deathLossPercent, false);
		}

		//Level loss
		uint64_t expLoss = static_cast<uint64_t>(experience * deathLossPercent);
		g_events->eventPlayerOnLoseExperience(this->getPlayer(), expLoss);

		if (expLoss != 0) {
			uint32_t oldLevel = level;

			if (vocation->getId() == VOCATION_NONE || level > 7) {
				experience -= expLoss;
			}

			while (level > 1 && experience < Player::getExpForLevel(level)) {
				--level;
				healthMax = std::max<int32_t>(0, healthMax - vocation->getHPGain());
				manaMax = std::max<int32_t>(0, manaMax - vocation->getManaGain());
				capacity = std::max<int32_t>(0, capacity - vocation->getCapGain());
			}

			if (oldLevel != level) {
				sendTextMessage(MESSAGE_EVENT_ADVANCE, fmt::format("You were downgraded from Level {:d} to Level {:d}.", oldLevel, level));
			}

			uint64_t currLevelExp = Player::getExpForLevel(level);
			uint64_t nextLevelExp = Player::getExpForLevel(level + 1);
			if (nextLevelExp > currLevelExp) {
				levelPercent = Player::getPercentLevel(experience - currLevelExp, nextLevelExp - currLevelExp);
			} else {
				levelPercent = 0;
			}
		}

		if (blessings.test(5)) {
			if (lastHitPlayer) {
				blessings.reset(5);
			} else {
				blessings.reset();
				blessings.set(5);
			}
		} else {
			blessings.reset();
		}

		sendStats();
		sendSkills();
		sendReLoginWindow(unfairFightReduction);

		if (getSkull() == SKULL_BLACK) {
			health = 40;
			mana = 0;
		} else {
			health = healthMax;
			mana = manaMax;
		}

		auto it = conditions.begin(), end = conditions.end();
		while (it != end) {
			Condition* condition = *it;
			if (condition->isPersistent()) {
				it = conditions.erase(it);

				condition->endCondition(this->getPlayer());
				onEndCondition(condition->getType());
				delete condition;
			} else {
				++it;
			}
		}
	} else {
		setSkillLoss(true);

		auto it = conditions.begin(), end = conditions.end();
		while (it != end) {
			Condition* condition = *it;
			if (condition->isPersistent()) {
				it = conditions.erase(it);

				condition->endCondition(this->getPlayer());
				onEndCondition(condition->getType());
				delete condition;
			} else {
				++it;
			}
		}

		health = healthMax;
		g_game.internalTeleport(this->getPlayer(), getTemplePosition(), true);
		g_game.addCreatureHealth(this->getPlayer());
		onThink(EVENT_CREATURE_THINK_INTERVAL);
		onIdleStatus();
		sendStats();
	}
}

bool Player::dropCorpse(const CreaturePtr& lastHitCreature, const CreaturePtr& mostDamageCreature, bool lastHitUnjustified, bool mostDamageUnjustified)
{
	if (getZone() != ZONE_PVP || !Player::lastHitIsPlayer(lastHitCreature)) {
		return Creature::dropCorpse(lastHitCreature, mostDamageCreature, lastHitUnjustified, mostDamageUnjustified);
	}

	setDropLoot(true);
	return false;
}

ItemPtr Player::getCorpse(const CreaturePtr& lastHitCreature, const CreaturePtr& mostDamageCreature)
{
	const auto& corpse = Creature::getCorpse(lastHitCreature, mostDamageCreature);
	if (corpse && corpse->getContainer()) {
		size_t killersSize = getKillers().size();

		if (lastHitCreature) {
			if (!mostDamageCreature) {
				corpse->setSpecialDescription(fmt::format("You recognize {:s}. {:s} was killed by {:s}{:s}", getNameDescription(), getSex() == PLAYERSEX_FEMALE ? "She" : "He", lastHitCreature->getNameDescription(), killersSize > 1 ? " and others." : "."));
			} else if (lastHitCreature != mostDamageCreature) {
				corpse->setSpecialDescription(fmt::format("You recognize {:s}. {:s} was killed by {:s}, {:s}{:s}", getNameDescription(), getSex() == PLAYERSEX_FEMALE ? "She" : "He", mostDamageCreature->getNameDescription(), lastHitCreature->getNameDescription(), killersSize > 2 ? " and others." : "."));
			} else {
				corpse->setSpecialDescription(fmt::format("You recognize {:s}. {:s} was killed by {:s} and others.", getNameDescription(), getSex() == PLAYERSEX_FEMALE ? "She" : "He", mostDamageCreature->getNameDescription()));
			}
		} else if (mostDamageCreature) {
			if (killersSize > 1) {
				corpse->setSpecialDescription(fmt::format("You recognize {:s}. {:s} was killed by something evil, {:s}, and others", getNameDescription(), getSex() == PLAYERSEX_FEMALE ? "She" : "He", mostDamageCreature->getNameDescription()));
			} else {
				corpse->setSpecialDescription(fmt::format("You recognize {:s}. {:s} was killed by something evil and others", getNameDescription(), getSex() == PLAYERSEX_FEMALE ? "She" : "He", mostDamageCreature->getNameDescription()));
			}
		} else {
			corpse->setSpecialDescription(fmt::format("You recognize {:s}. {:s} was killed by something evil {:s}", getNameDescription(), getSex() == PLAYERSEX_FEMALE ? "She" : "He", killersSize ? " and others." : "."));
		}
	}
	return corpse;
}

void Player::addInFightTicks(const bool pzlock /*= false*/)
{
	if (hasFlag(PlayerFlag_NotGainInFight)) {
		return;
	}

	if (pzlock) {
		pzLocked = true;
	}

	const auto& condition = Condition::createCondition(CONDITIONID_DEFAULT, CONDITION_INFIGHT, g_config.getNumber(ConfigManager::PZ_LOCKED), 0);
	addCondition(condition);
}

void Player::removeList()
{
	g_game.removePlayer(this->getPlayer());

	for (const auto& it : g_game.getPlayers()) {
		it.second->notifyStatusChange(this->getPlayer(), VIPSTATUS_OFFLINE);
	}
}

void Player::addList()
{
	for (const auto& it : g_game.getPlayers()) {
		it.second->notifyStatusChange(this->getPlayer(), VIPSTATUS_ONLINE);
	}

	g_game.addPlayer(this->getPlayer());
}

void Player::kickPlayer(bool displayEffect)
{
	g_creatureEvents->playerLogout(this->getPlayer());
	if (client) {
		client->logout(displayEffect, true);
	} else {
		g_game.removeCreature(this->getPlayer());
	}
}

void Player::notifyStatusChange(const PlayerPtr& loginPlayer, VipStatus_t status)
{
	if (!client) {
		return;
	}

	if (const auto& it = VIPList.find(loginPlayer->guid); it == VIPList.end()) {
		return;
	}

	client->sendUpdatedVIPStatus(loginPlayer->guid, status);

	if (status == VIPSTATUS_ONLINE) {
		client->sendTextMessage(TextMessage(MESSAGE_STATUS_SMALL, loginPlayer->getName() + " has logged in."));
	} else if (status == VIPSTATUS_OFFLINE) {
		client->sendTextMessage(TextMessage(MESSAGE_STATUS_SMALL, loginPlayer->getName() + " has logged out."));
	}
}

bool Player::removeVIP(const uint32_t vipGuid)
{
	if (VIPList.erase(vipGuid) == 0) {
		return false;
	}

	IOLoginData::removeVIPEntry(accountNumber, vipGuid);
	return true;
}

bool Player::addVIP(const uint32_t vipGuid, const std::string& vipName, VipStatus_t status)
{
	if (VIPList.size() >= getMaxVIPEntries()) {
		sendTextMessage(MESSAGE_STATUS_SMALL, "You cannot add more buddies.");
		return false;
	}

	if (const auto result = VIPList.insert(vipGuid); !result.second) {
		sendTextMessage(MESSAGE_STATUS_SMALL, "This player is already in your list.");
		return false;
	}

	IOLoginData::addVIPEntry(accountNumber, vipGuid, "", 0, false);
	if (client) {
		client->sendVIP(vipGuid, vipName, "", 0, false, status);
	}
	return true;
}

bool Player::addVIPInternal(const uint32_t vipGuid)
{
	if (VIPList.size() >= getMaxVIPEntries()) {
		return false;
	}

	return VIPList.insert(vipGuid).second;
}

bool Player::editVIP(const uint32_t vipGuid, const std::string& description, const uint32_t icon, const bool notify)
{
	if (const auto& it = VIPList.find(vipGuid); it == VIPList.end()) {
		return false; // player is not in VIP
	}

	IOLoginData::editVIPEntry(accountNumber, vipGuid, description, icon, notify);
	return true;
}

//close container and its child containers
void Player::autoCloseContainers(ContainerPtr /*unused*/)
{
	std::vector<uint32_t> closeList;

	const Position& playerPos = getPosition();

	for (const auto& [cid, containerData] : openContainers) {
		const ContainerPtr& openContainer = containerData.container;

		const ThingPtr& top = openContainer->getTopParent();
		if (top) {
			if (const CreaturePtr creature = top->getCreature()) {
				if (creature.get() == this) {
					continue; 
				}
			}
		}

		const Position& containerPos = openContainer->getPosition();
		if (containerPos == Position() || !Position::areInRange<1, 1, 0>(playerPos, containerPos)) {
			closeList.push_back(cid);
		}
	}

	for (uint32_t cid : closeList) {
		closeContainer(cid);
		if (client) {
			client->sendCloseContainer(cid);
		}
	}
}

bool Player::hasCapacity(const ItemPtr& item, uint32_t count) const
{
	if (hasFlag(PlayerFlag_CannotPickupItem)) {
		return false;
	}

	if (hasFlag(PlayerFlag_HasInfiniteCapacity) || item->getTopParent() == this->getPlayer()) {
		return true;
	}

	uint32_t itemWeight = item->getContainer() != nullptr ? item->getWeight() : item->getBaseWeight();
	if (item->isStackable()) {
		itemWeight *= count;
	}
	return itemWeight <= getFreeCapacity();
}

ReturnValue Player::queryAdd(int32_t index, const ThingPtr& thing, uint32_t count, uint32_t flags, CreaturePtr)
{
	const auto& item = thing->getItem();
	if (item == nullptr) {
		return RETURNVALUE_NOTPOSSIBLE;
	}

	if (const bool childIsOwner = hasBitSet(FLAG_CHILDISOWNER, flags)) {
		//a child container is querying the player, just check if enough capacity
		bool skipLimit = hasBitSet(FLAG_NOLIMIT, flags);
		if (skipLimit || hasCapacity(item, count)) {
			return RETURNVALUE_NOERROR;
		}
		return RETURNVALUE_NOTENOUGHCAPACITY;
	}

	if (!item->isPickupable()) {
		return RETURNVALUE_CANNOTPICKUP;
	}

	if (item->isStoreItem()) {
		return RETURNVALUE_ITEMCANNOTBEMOVEDTHERE;
	}

	ReturnValue ret = RETURNVALUE_NOTPOSSIBLE;

	const int32_t& slotPosition = item->getSlotPosition();
	if ((slotPosition & SLOTP_HEAD) || (slotPosition & SLOTP_NECKLACE) ||
	        (slotPosition & SLOTP_BACKPACK) || (slotPosition & SLOTP_ARMOR) ||
	        (slotPosition & SLOTP_LEGS) || (slotPosition & SLOTP_FEET) ||
	        (slotPosition & SLOTP_RING)) {
		ret = RETURNVALUE_CANNOTBEDRESSED;
	} else if (slotPosition & SLOTP_TWO_HAND) {
		ret = RETURNVALUE_PUTTHISOBJECTINBOTHHANDS;
	} else if ((slotPosition & SLOTP_RIGHT) || (slotPosition & SLOTP_LEFT)) {
		if (!g_config.getBoolean(ConfigManager::CLASSIC_EQUIPMENT_SLOTS)) {
			ret = RETURNVALUE_CANNOTBEDRESSED;
		} else {
			ret = RETURNVALUE_PUTTHISOBJECTINYOURHAND;
		}
	}

	switch (index) {
		case CONST_SLOT_HEAD: {
			if (slotPosition & SLOTP_HEAD) {
				ret = RETURNVALUE_NOERROR;
			}
			break;
		}

		case CONST_SLOT_NECKLACE: {
			if (slotPosition & SLOTP_NECKLACE) {
				ret = RETURNVALUE_NOERROR;
			}
			break;
		}

		case CONST_SLOT_BACKPACK: {
			if (slotPosition & SLOTP_BACKPACK) {
				ret = RETURNVALUE_NOERROR;
			}
			break;
		}

		case CONST_SLOT_ARMOR: {
			if (slotPosition & SLOTP_ARMOR) {
				ret = RETURNVALUE_NOERROR;
			}
			break;
		}

		case CONST_SLOT_RIGHT: {
			if (slotPosition & SLOTP_RIGHT) {
				if (!g_config.getBoolean(ConfigManager::CLASSIC_EQUIPMENT_SLOTS)) {
					if (item->getWeaponType() != WEAPON_SHIELD && item->getWeaponType() != WEAPON_QUIVER) {
						ret = RETURNVALUE_CANNOTBEDRESSED;
					} else {
						const auto& leftItem = inventory[CONST_SLOT_LEFT];
						if (leftItem) {
							if ((leftItem->getSlotPosition() | slotPosition) & SLOTP_TWO_HAND) {
								if (leftItem->getWeaponType() != WEAPON_DISTANCE ||
									item->getWeaponType() != WEAPON_QUIVER) {
									ret = RETURNVALUE_BOTHHANDSNEEDTOBEFREE;
								} else {
									ret = RETURNVALUE_NOERROR;
								}
							} else {
								ret = RETURNVALUE_NOERROR;
							}
						} else {
							ret = RETURNVALUE_NOERROR;
						}
					}
				} else if (slotPosition & SLOTP_TWO_HAND) {
					const auto& leftItem = inventory[CONST_SLOT_LEFT];
					if (leftItem && leftItem != item) {
						ret = RETURNVALUE_BOTHHANDSNEEDTOBEFREE;
					} else {
						ret = RETURNVALUE_NOERROR;
					}
				} else if (inventory[CONST_SLOT_LEFT]) {
					const auto& leftItem = inventory[CONST_SLOT_LEFT];
					WeaponType_t type = item->getWeaponType(), leftType = leftItem->getWeaponType();

					if (leftItem->getSlotPosition() & SLOTP_TWO_HAND) {
						if (leftItem->getWeaponType() != WEAPON_DISTANCE || type != WEAPON_QUIVER) {
							ret = RETURNVALUE_DROPTWOHANDEDITEM;
						} else {
							ret = RETURNVALUE_NOERROR;
						}
					} else if (item == leftItem && count == item->getItemCount()) {
						ret = RETURNVALUE_NOERROR;
					} else if (leftType == WEAPON_SHIELD && type == WEAPON_SHIELD) {
						ret = RETURNVALUE_CANONLYUSEONESHIELD;
					} else if (leftType == WEAPON_NONE || type == WEAPON_NONE || leftType == WEAPON_SHIELD ||
						type == WEAPON_SHIELD || leftType == WEAPON_AMMO || type == WEAPON_AMMO ||
						leftType == WEAPON_QUIVER || type == WEAPON_QUIVER) {
						ret = RETURNVALUE_NOERROR;
					} else {
						ret = RETURNVALUE_CANONLYUSEONEWEAPON;
					}
				} else {
					ret = RETURNVALUE_NOERROR;
				}
			}
			break;
		}

		case CONST_SLOT_LEFT: {
			if (slotPosition & SLOTP_LEFT) {
				if (!g_config.getBoolean(ConfigManager::CLASSIC_EQUIPMENT_SLOTS)) {
					WeaponType_t type = item->getWeaponType();
					const auto& rightItem = inventory[CONST_SLOT_RIGHT];
					if (type == WEAPON_NONE || type == WEAPON_SHIELD || type == WEAPON_AMMO || type == WEAPON_QUIVER) {
						ret = RETURNVALUE_CANNOTBEDRESSED;
					} else if (rightItem && (slotPosition & SLOTP_TWO_HAND)) {
						if (type != WEAPON_DISTANCE || rightItem->getWeaponType() != WEAPON_QUIVER) {
							ret = RETURNVALUE_BOTHHANDSNEEDTOBEFREE;
						} else {
							ret = RETURNVALUE_NOERROR;
						}
					} else {
						ret = RETURNVALUE_NOERROR;
					}
				} else if (slotPosition & SLOTP_TWO_HAND) {
					const auto& rightItem = inventory[CONST_SLOT_RIGHT];
					if (rightItem && rightItem != item) {
						if (item->getWeaponType() != WEAPON_DISTANCE || rightItem->getWeaponType() != WEAPON_QUIVER) {
							ret = RETURNVALUE_BOTHHANDSNEEDTOBEFREE;
						} else {
							ret = RETURNVALUE_NOERROR;
						}
					} else {
						ret = RETURNVALUE_NOERROR;
					}
				} else if (inventory[CONST_SLOT_RIGHT]) {
					const auto& rightItem = inventory[CONST_SLOT_RIGHT];
					WeaponType_t type = item->getWeaponType(), rightType = rightItem->getWeaponType();

					if (rightItem->getSlotPosition() & SLOTP_TWO_HAND) {
						if (type != WEAPON_DISTANCE || rightItem->getWeaponType() != WEAPON_QUIVER) {
							ret = RETURNVALUE_DROPTWOHANDEDITEM;
						} else {
							ret = RETURNVALUE_NOERROR;
						}
					} else if (item == rightItem && count == item->getItemCount()) {
						ret = RETURNVALUE_NOERROR;
					} else if (rightType == WEAPON_SHIELD && type == WEAPON_SHIELD) {
						ret = RETURNVALUE_CANONLYUSEONESHIELD;
					} else if (rightType == WEAPON_NONE || type == WEAPON_NONE || rightType == WEAPON_SHIELD ||
					           type == WEAPON_SHIELD || rightType == WEAPON_AMMO || type == WEAPON_AMMO ||
					           rightType == WEAPON_QUIVER || type == WEAPON_QUIVER) {
						ret = RETURNVALUE_NOERROR;
					} else {
						ret = RETURNVALUE_CANONLYUSEONEWEAPON;
					}
				} else {
					ret = RETURNVALUE_NOERROR;
				}
			}
			break;
		}

		case CONST_SLOT_LEGS: {
			if (slotPosition & SLOTP_LEGS) {
				ret = RETURNVALUE_NOERROR;
			}
			break;
		}

		case CONST_SLOT_FEET: {
			if (slotPosition & SLOTP_FEET) {
				ret = RETURNVALUE_NOERROR;
			}
			break;
		}

		case CONST_SLOT_RING: {
			if (slotPosition & SLOTP_RING) {
				ret = RETURNVALUE_NOERROR;
			}
			break;
		}

		case CONST_SLOT_AMMO: {
			if ((slotPosition & SLOTP_AMMO) || g_config.getBoolean(ConfigManager::CLASSIC_EQUIPMENT_SLOTS)) {
				ret = RETURNVALUE_NOERROR;
			}
			break;
		}

		case CONST_SLOT_WHEREEVER:
		case -1:
			ret = RETURNVALUE_NOTENOUGHROOM;
			break;

		default:
			ret = RETURNVALUE_NOTPOSSIBLE;
			break;
	}

	if (ret != RETURNVALUE_NOERROR && ret != RETURNVALUE_NOTENOUGHROOM) {
		return ret;
	}

	//check if enough capacity
	if (!hasCapacity(item, count)) {
		return RETURNVALUE_NOTENOUGHCAPACITY;
	}

	if (index != CONST_SLOT_WHEREEVER && index != -1) { // we don't try to equip whereever call
		ret = g_moveEvents->onPlayerEquip(std::const_pointer_cast<Player>(this->getPlayer()), std::const_pointer_cast<Item>(item), static_cast<slots_t>(index), true);
		if (ret != RETURNVALUE_NOERROR) {
			return ret;
		}
	}

	//need an exchange with source? (destination item is swapped with currently moved item)
	const auto& inventoryItem = getInventoryItem(static_cast<slots_t>(index));
	if (inventoryItem && (!inventoryItem->isStackable() || inventoryItem->getID() != item->getID())) {
		if (!g_config.getBoolean(ConfigManager::CLASSIC_EQUIPMENT_SLOTS)) {
			const auto& cylinder = item->getTopParent();
			if (cylinder && (std::dynamic_pointer_cast<const DepotChest>(cylinder) || std::dynamic_pointer_cast<const Player>(cylinder))) {
				return RETURNVALUE_NEEDEXCHANGE;
			}
			return RETURNVALUE_NOTENOUGHROOM;	
		}
		return RETURNVALUE_NEEDEXCHANGE;
	}
	return ret;
}

ReturnValue Player::queryMaxCount(int32_t index, const ThingPtr& thing, uint32_t count, uint32_t& maxQueryCount,
		uint32_t flags)
{
	auto item = thing->getItem();
	if (item == nullptr) {
		maxQueryCount = 0;
		return RETURNVALUE_NOTPOSSIBLE;
	}

	if (index == INDEX_WHEREEVER) {
		uint32_t n = 0;
		for (int32_t slotIndex = CONST_SLOT_FIRST; slotIndex <= CONST_SLOT_LAST; ++slotIndex) {
			if (const auto& inventoryItem = inventory[slotIndex]) {
				if (auto subContainer = inventoryItem->getContainer()) {
					uint32_t queryCount = 0;
					subContainer->queryMaxCount(INDEX_WHEREEVER, item, item->getItemCount(), queryCount, flags);
					n += queryCount;

					//iterate through all items, including sub-containers (deep search)
					for (ContainerIterator it = subContainer->iterator(); it.hasNext(); it.advance()) {
						if (auto tmpContainer = (*it)->getContainer()) {
							queryCount = 0;
							tmpContainer->queryMaxCount(INDEX_WHEREEVER, item, item->getItemCount(), queryCount, flags);
							n += queryCount;
						}
					}
				} else if (inventoryItem->isStackable() && item->equals(inventoryItem) && inventoryItem->getItemCount() < 100) {
					const uint32_t remainder = (100 - inventoryItem->getItemCount());

					if (queryAdd(slotIndex, item, remainder, flags) == RETURNVALUE_NOERROR) {
						n += remainder;
					}
				}
			} else if (queryAdd(slotIndex, item, item->getItemCount(), flags) == RETURNVALUE_NOERROR) { //empty slot
				if (item->isStackable()) {
					n += 100;
				} else {
					++n;
				}
			}
		}

		maxQueryCount = n;
	} else {
		ItemPtr destItem = nullptr;

		if (const auto& destThing = getThing(index)) {
			destItem = destThing->getItem();
		}

		if (destItem) {
			if (destItem->isStackable() && item->equals(destItem) && destItem->getItemCount() < 100) {
				maxQueryCount = 100 - destItem->getItemCount();
			} else {
				maxQueryCount = 0;
			}
		} else if (queryAdd(index, item, count, flags) == RETURNVALUE_NOERROR) { //empty slot
			if (item->isStackable()) {
				maxQueryCount = 100;
			} else {
				maxQueryCount = 1;
			}

			return RETURNVALUE_NOERROR;
		}
	}

	if (maxQueryCount < count) {
		return RETURNVALUE_NOTENOUGHROOM;
	} else {
		return RETURNVALUE_NOERROR;
	}
}

ReturnValue Player::queryRemove(const ThingPtr& thing, uint32_t count, uint32_t flags, CreaturePtr /*= nullptr*/)
{
	int32_t index = getThingIndex(thing);
	if (index == -1) {
		return RETURNVALUE_NOTPOSSIBLE;
	}

	const auto& item = thing->getItem();
	if (item == nullptr) {
		return RETURNVALUE_NOTPOSSIBLE;
	}

	if (count == 0 || (item->isStackable() && count > item->getItemCount())) {
		return RETURNVALUE_NOTPOSSIBLE;
	}

	if (!item->isMoveable() && !hasBitSet(FLAG_IGNORENOTMOVEABLE, flags)) {
		return RETURNVALUE_NOTMOVEABLE;
	}

	return RETURNVALUE_NOERROR;
}

CylinderPtr Player::queryDestination(int32_t& index, const ThingPtr& thing, ItemPtr& destItem,
	uint32_t& flags)
{
	if (index == 0 /*drop to capacity window*/ || index == INDEX_WHEREEVER) {
		destItem.reset();

		ItemPtr item = thing ? thing->getItem() : nullptr;
		if (!item) {
			return this->getPlayer();
		}

		const bool autoStack = !(flags & FLAG_IGNOREAUTOSTACK);
		const bool isStackable = item->isStackable();

		std::vector<ContainerPtr> containers;

		for (uint32_t slotIndex = CONST_SLOT_FIRST; slotIndex <= CONST_SLOT_LAST; ++slotIndex) {
			if (auto inventoryItem = inventory[slotIndex]) {
				if (inventoryItem == tradeItem || inventoryItem == item) {
					continue;
				}

				if (autoStack && isStackable) {
					// Try to find an already existing item to stack with
					if (queryAdd(slotIndex, item, item->getItemCount(), 0) == RETURNVALUE_NOERROR) {
						if (inventoryItem->equals(item) && inventoryItem->getItemCount() < 100) {
							index = slotIndex;
							destItem = inventoryItem;
							return this->getPlayer();
						}
					}
				}

				if (auto subContainer = inventoryItem->getContainer()) {
					containers.push_back(subContainer);
				}
			}
			else if (queryAdd(slotIndex, item, item->getItemCount(), flags) == RETURNVALUE_NOERROR) {
				index = slotIndex;
				destItem.reset();
				return this->getPlayer();
			}
		}

		size_t i = 0;
		while (i < containers.size()) {
			auto tmpContainer = containers[i++];
			if (tmpContainer) {
				if (!autoStack || !isStackable) {
					uint32_t n = tmpContainer->capacity() - std::min(tmpContainer->capacity(), static_cast<uint32_t>(tmpContainer->size()));
					while (n) {
						if (tmpContainer->queryAdd(tmpContainer->capacity() - n, item, item->getItemCount(), flags) == RETURNVALUE_NOERROR) {
							index = tmpContainer->capacity() - n;
							destItem.reset();
							return tmpContainer;
						}
						--n;
					}
				}

				for (const auto& tmpContainerItem : tmpContainer->getItemList()) {
					if (tmpContainerItem == tradeItem || tmpContainerItem == item) {
						continue;
					}

					if (autoStack && isStackable && tmpContainerItem->equals(item) && tmpContainerItem->getItemCount() < 100) {
						index = tmpContainer->size();
						destItem = tmpContainerItem;
						return tmpContainer;
					}

					if (auto subContainer = tmpContainerItem->getContainer()) {
						containers.push_back(subContainer);
					}
				}

				if (tmpContainer->size() < tmpContainer->capacity() && tmpContainer->queryAdd(tmpContainer->size(), item, item->getItemCount(), flags) == RETURNVALUE_NOERROR) {
					index = tmpContainer->size();
					destItem.reset();
					return tmpContainer;
				}
			}
		}

		return this->getPlayer();
	}

	auto destThing = getThing(index);
	if (destThing) {
		destItem = destThing->getItem();
	}

	if (auto subCylinder = std::dynamic_pointer_cast<Cylinder>(destThing)) {
		index = INDEX_WHEREEVER;
		destItem.reset();
		return subCylinder;
	}

	return this->getPlayer();
}

void Player::addThing(int32_t index, ThingPtr thing)
{
	if (index < CONST_SLOT_FIRST || index > CONST_SLOT_LAST) {
		return /*RETURNVALUE_NOTPOSSIBLE*/;
	}

	const auto& item = thing->getItem();
	if (!item) {
		return /*RETURNVALUE_NOTPOSSIBLE*/;
	}

	item->setParent(getPlayer());
	inventory[index] = item;

	//send to client
	sendInventoryItem(static_cast<slots_t>(index), item);
}

void Player::updateThing(ThingPtr thing, uint16_t itemId, uint32_t count)
{
	int32_t index = getThingIndex(thing);
	if (index == -1) {
		return /*RETURNVALUE_NOTPOSSIBLE*/;
	}

	const auto& item = thing->getItem();
	if (!item) {
		return /*RETURNVALUE_NOTPOSSIBLE*/;
	}

	item->setID(itemId);
	item->setSubType(count);

	//send to client
	sendInventoryItem(static_cast<slots_t>(index), item);

	//event methods
	onUpdateInventoryItem(item, item);
}

void Player::replaceThing(uint32_t index, ThingPtr thing)
{
	if (index > CONST_SLOT_LAST) {
		return /*RETURNVALUE_NOTPOSSIBLE*/;
	}

	const auto& oldItem = getInventoryItem(static_cast<slots_t>(index));
	if (!oldItem) {
		return /*RETURNVALUE_NOTPOSSIBLE*/;
	}

	const auto& item = thing->getItem();
	if (!item) {
		return /*RETURNVALUE_NOTPOSSIBLE*/;
	}

	//send to client
	sendInventoryItem(static_cast<slots_t>(index), item);

	//event methods
	onUpdateInventoryItem(oldItem, item);
	item->setParent(getPlayer());

	inventory[index] = item;
}

void Player::removeThing(ThingPtr thing, uint32_t count)
{
	const auto& item = thing->getItem();
	if (!item) {
		return /*RETURNVALUE_NOTPOSSIBLE*/;
	}

	int32_t index = getThingIndex(thing);
	if (index == -1) {
		return /*RETURNVALUE_NOTPOSSIBLE*/;
	}

	if (item->isStackable()) {
		if (count == item->getItemCount()) {
			//send change to client
			sendInventoryItem(static_cast<slots_t>(index), nullptr);

			//event methods
			onRemoveInventoryItem(item);

			item->clearParent();
			inventory[index] = nullptr;
		} else {
			uint8_t newCount = static_cast<uint8_t>(std::max<int32_t>(0, item->getItemCount() - count));
			item->setItemCount(newCount);

			//send change to client
			sendInventoryItem(static_cast<slots_t>(index), item);

			//event methods
			onUpdateInventoryItem(item, item);
		}
	} else {
		//send change to client
		sendInventoryItem(static_cast<slots_t>(index), nullptr);

		//event methods
		onRemoveInventoryItem(item);
		item->clearParent();
		inventory[index] = nullptr;
	}
}

int32_t Player::getThingIndex(ThingPtr thing)
{
	for (int i = CONST_SLOT_FIRST; i <= CONST_SLOT_LAST; ++i) {
		if (inventory[i] == thing) {
			return i;
		}
	}
	return -1;
}

size_t Player::getFirstIndex() const
{
	return CONST_SLOT_FIRST;
}

size_t Player::getLastIndex() const
{
	return CONST_SLOT_LAST + 1;
}

uint32_t Player::getItemTypeCount(const uint16_t itemId, int32_t subType /*= -1*/) const
{
	uint32_t count = 0;
	for (int32_t i = CONST_SLOT_FIRST; i <= CONST_SLOT_LAST; i++) {
		const auto& item = inventory[i];
		if (!item) {
			continue;
		}

		if (item->getID() == itemId) {
			count += Item::countByType(item, subType);
		}

		if (const auto& container = item->getContainer()) {
			for (ContainerIterator it = container->iterator(); it.hasNext(); it.advance()) {
				if ((*it)->getID() == itemId) {
					count += Item::countByType(*it, subType);
				}
			}
		}
	}
	return count;
}

bool Player::removeItemOfType(const uint16_t itemId, uint32_t amount, int32_t subType, bool ignoreEquipped/* = false*/) const
{
	if (amount == 0) {
		return true;
	}

	std::vector<ItemPtr> itemList;

	uint32_t count = 0;
	for (int32_t i = CONST_SLOT_FIRST; i <= CONST_SLOT_LAST; i++) {
		const auto& item = inventory[i];
		if (!item) {
			continue;
		}

		if (!ignoreEquipped && item->getID() == itemId) {
			uint32_t itemCount = Item::countByType(item, subType);
			if (itemCount == 0) {
				continue;
			}

			itemList.push_back(item);

			count += itemCount;
			if (count >= amount) {
				g_game.internalRemoveItems(std::move(itemList), amount, Item::items[itemId].stackable);
				return true;
			}
		} else if (const auto& container = item->getContainer()) {
			for (ContainerIterator it = container->iterator(); it.hasNext(); it.advance()) {
				const auto& containerItem = *it;
				if (containerItem->getID() == itemId) {
					const uint32_t itemCount = Item::countByType(containerItem, subType);
					if (itemCount == 0) {
						continue;
					}

					itemList.push_back(containerItem);

					count += itemCount;
					if (count >= amount) {
						g_game.internalRemoveItems(std::move(itemList), amount, Item::items[itemId].stackable);
						return true;
					}
				}
			}
		}
	}
	return false;
}

gtl::btree_map<uint32_t, uint32_t>& Player::getAllItemTypeCount(gtl::btree_map<uint32_t, uint32_t>& countMap) const
{
	for (int32_t i = CONST_SLOT_FIRST; i <= CONST_SLOT_LAST; i++) {
		const auto& item = inventory[i];
		if (!item) {
			continue;
		}

		countMap[item->getID()] += Item::countByType(item, -1);

		if (const auto& container = item->getContainer()) {
			for (ContainerIterator it = container->iterator(); it.hasNext(); it.advance()) {
				countMap[(*it)->getID()] += Item::countByType(*it, -1);
			}
		}
	}
	return countMap;
}

ThingPtr Player::getThing(size_t index)
{
	if (index >= CONST_SLOT_FIRST && index <= CONST_SLOT_LAST) {
		return inventory[index];
	}
	return nullptr;
}

void Player::postAddNotification(ThingPtr thing, CylinderPtr oldParent, int32_t index, cylinderlink_t link /*= LINK_OWNER*/)
{
	if (link == LINK_OWNER) {
		//calling movement scripts
		g_moveEvents->onPlayerEquip(this->getPlayer(), thing->getItem(), static_cast<slots_t>(index), false);
		g_events->eventPlayerOnInventoryUpdate(this->getPlayer(), thing->getItem(), static_cast<slots_t>(index), true);
		if (isInventorySlot(static_cast<slots_t>(index))) {
			const auto& item = thing->getItem();
			if (item && item->hasImbuements()) {
				addItemImbuements(thing->getItem());
			}
		}
	}

	bool requireListUpdate = false;

	if (link == LINK_OWNER || link == LINK_TOPPARENT) {
		const auto& i = (oldParent ? oldParent->getItem() : nullptr);

		// Check if we owned the old container too, so we don't need to do anything,
		// as the list was updated in postRemoveNotification
		assert(i ? i->getContainer() != nullptr : true);

		if (i) {
			requireListUpdate = std::static_pointer_cast<Container>(i)->getHoldingPlayer() != getPlayer();
		} else {
			requireListUpdate = oldParent != getPlayer();
		}

		updateInventoryWeight();
		updateItemsLight();
		sendStats();
	}

	if (auto item = thing->getItem()) {
		if (auto container = item->getContainer()) {
			onSendContainer(container);
		}

		if (shopOwner && requireListUpdate) {
			updateSaleShopList(item);
		}
	} else if (auto creature = thing->getCreature()) {
		if (creature == getCreature()) {
			//check containers
			std::vector<ContainerPtr> containers;

			for (auto& val : openContainers | std::views::values) {
				if (!Position::areInRange<1, 1, 0>(val.container->getPosition(), getPosition())) {
					containers.push_back(val.container);
				}
			}

			for (auto& container : containers) {
				autoCloseContainers(container);
			}
		}
	}
}

void Player::postRemoveNotification(ThingPtr thing, CylinderPtr newParent, int32_t index, cylinderlink_t link /*= LINK_OWNER*/)
{
	if (link == LINK_OWNER) {
		//calling movement scripts
		g_moveEvents->onPlayerDeEquip(this->getPlayer(), thing->getItem(), static_cast<slots_t>(index));
		g_events->eventPlayerOnInventoryUpdate(this->getPlayer(), thing->getItem(), static_cast<slots_t>(index), false);
		if (isInventorySlot(static_cast<slots_t>(index))) {
			auto item = thing->getItem();
			if (item && item->hasImbuements()) {
				removeItemImbuements(thing->getItem());
			}
		}
	}

	bool requireListUpdate = false;

	if (link == LINK_OWNER || link == LINK_TOPPARENT) {
		const auto& i = (newParent ? newParent->getItem() : nullptr);

		// Check if we owned the old container too, so we don't need to do anything,
		// as the list was updated in postRemoveNotification
		assert(i ? i->getContainer() != nullptr : true);

		if (i) {
			requireListUpdate = std::dynamic_pointer_cast<Container>(i)->getHoldingPlayer() != getPlayer();
		} else {
			requireListUpdate = newParent != getPlayer();
		} 
		
		updateInventoryWeight();
		updateItemsLight();
		sendStats();
	}

	if (auto item = thing->getItem()) {
		if (auto container = item->getContainer()) {
			if (container->isRemoved() || !Position::areInRange<1, 1, 0>(getPosition(), container->getPosition())) {
				autoCloseContainers(container);
			} else if (container->getItem()->getTopParent() == this->getPlayer()) {
				onSendContainer(container);
			} else if (auto topContainer = std::dynamic_pointer_cast<Container>(container->getItem()->getTopParent())) {
				if (auto depotChest = std::dynamic_pointer_cast<DepotChest>(topContainer)) {
					bool isOwner = false;

					for (const auto& it : depotChests) {
						if (it.second == depotChest) {
							isOwner = true;
							onSendContainer(container);
						}
					}

					if (!isOwner) {
						autoCloseContainers(container);
					}
				} else if (auto inboxContainer = std::dynamic_pointer_cast<Inbox>(topContainer)) {
					if (inboxContainer == inbox) {
						onSendContainer(container);
					} else {
						autoCloseContainers(container);
					}
				} else {
					onSendContainer(container);
				}
			} else {
				autoCloseContainers(container);
			}
		}

		if (shopOwner && requireListUpdate) {
			updateSaleShopList(item);
		}
	}
}

bool Player::updateSaleShopList(const ItemConstPtr& item)
{
	uint16_t itemId = item->getID();
	bool isCurrency = false;
	for (const auto& it : Item::items.currencyItems) {
		if (it.second == itemId) {
			isCurrency = true;
			break;
		}
	}

	if (!isCurrency) {
		const auto it = std::ranges::find_if(shopItemList, [itemId](const ShopInfo& shopInfo) { return shopInfo.itemId == itemId && shopInfo.sellPrice != 0; });
		if (it == shopItemList.end()) {
			const auto& container = item->getContainer();
			if (!container) {
				return false;
			}

			const auto& items = container->getItemList();
			return std::any_of(items.begin(), items.end(), [this](const ItemPtr& containerItem) {
				return updateSaleShopList(containerItem);
			});
		}
	}

	if (client) {
		client->sendSaleItemList(shopItemList);
	}
	return true;
}

bool Player::hasShopItemForSale(uint32_t itemId, uint8_t subType) const
{
	const ItemType& itemType = Item::items[itemId];
	return std::any_of(shopItemList.begin(), shopItemList.end(), [&](const ShopInfo& shopInfo) {
		return shopInfo.itemId == itemId && shopInfo.buyPrice != 0 && (!itemType.isFluidContainer() || shopInfo.subType == subType);
	});
}

void Player::internalAddThing(ThingPtr thing)
{
	internalAddThing(0, thing);
}

void Player::internalAddThing(uint32_t index, ThingPtr thing)
{
	const auto& item = thing->getItem();
	if (!item) {
		return;
	}

	//index == 0 means we should equip this item at the most appropriate slot (no action required here)
	if (index > CONST_SLOT_WHEREEVER && index <= CONST_SLOT_LAST) {
		if (inventory[index]) {
			return;
		}

		inventory[index] = item;
		item->setParent(getPlayer());
	}
}

bool Player::setFollowCreature(const CreaturePtr& creature)
{
	if (!Creature::setFollowCreature(creature)) {
		setFollowCreature(nullptr);
		setAttackedCreature(nullptr);

		sendCancelMessage(RETURNVALUE_THEREISNOWAY);
		sendCancelTarget();
		stopWalk();
		return false;
	}
	return true;
}

bool Player::setAttackedCreature(const CreaturePtr& creature)
{
	if (!Creature::setAttackedCreature(creature)) {
		sendCancelTarget();
		return false;
	}

	if (chaseMode && creature) {
		if (getFollowCreature() != creature) {
			//chase opponent
			setFollowCreature(creature);
		}
	} else if (getFollowCreature()) {
		setFollowCreature(nullptr);
	}

	if (creature) {
		g_dispatcher.addTask(createTask([id = getID()]() { g_game.checkCreatureAttack(id); }));
	}
	return true;
}

void Player::goToFollowCreature()
{
	if (!walkTask) {
		if ((OTSYS_TIME() - lastFailedFollow) < 2000) {
			return;
		}

		Creature::goToFollowCreature();

		if (getFollowCreature() && !hasFollowPath) {
			lastFailedFollow = OTSYS_TIME();
		}
	}
}

void Player::getPathSearchParams(const CreatureConstPtr& creature, FindPathParams& fpp) const
{
	Creature::getPathSearchParams(creature, fpp);
	fpp.fullPathSearch = true;
}

void Player::doAttacking(uint32_t)
{
	if (lastAttack == 0) {
		lastAttack = OTSYS_TIME() - getAttackSpeed() - 1;
	}

	if (hasCondition(CONDITION_PACIFIED)) {
		return;
	}

	if ((OTSYS_TIME() - lastAttack) >= getAttackSpeed()) {
		bool result = false;

		const auto& tool = getWeapon();
		const auto& weapon = g_weapons->getWeapon(tool);
		uint32_t delay = getAttackSpeed();
		bool classicSpeed = g_config.getBoolean(ConfigManager::CLASSIC_ATTACK_SPEED);

		if (weapon) {
			if (!weapon->interruptSwing()) {
				result = weapon->useWeapon(this->getPlayer(), tool, getAttackedCreature());
			} else if (!classicSpeed && !canDoAction()) {
				delay = getNextActionTime();
			} else {
				result = weapon->useWeapon(this->getPlayer(), tool, getAttackedCreature());
			}
		} else {
			result = Weapon::useFist(this->getPlayer(), getAttackedCreature());
		}

		SchedulerTask* task = createSchedulerTask(std::max<uint32_t>(SCHEDULER_MINTICKS, delay), [id = getID()]() { g_game.checkCreatureAttack(id); });
		if (!classicSpeed) {
			setNextActionTask(task, false);
		} else {
			g_scheduler.stopEvent(classicAttackEvent);
			classicAttackEvent = g_scheduler.addEvent(task);
		}

		if (result) {
			lastAttack = OTSYS_TIME();
		}
	}
}

uint64_t Player::getGainedExperience(const CreaturePtr& attacker) const
{
	if (g_config.getBoolean(ConfigManager::EXPERIENCE_FROM_PLAYERS)) {
		const auto attackerPlayer = attacker->getPlayer();
		if (attackerPlayer && attackerPlayer != this->getPlayer() && skillLoss && std::abs(static_cast<int32_t>(attackerPlayer->getLevel() - level)) <= g_config.getNumber(ConfigManager::EXP_FROM_PLAYERS_LEVEL_RANGE)) {
			return std::max<uint64_t>(0, std::floor(getLostExperience() * getDamageRatio(attacker) * 0.75));
		}
	}
	return 0;
}

void Player::onFollowCreature(const CreatureConstPtr& creature)
{
	if (!creature) {
		stopWalk();
	}
}

void Player::setChaseMode(const bool mode)
{
	bool prevChaseMode = chaseMode;
	chaseMode = mode;

	if (prevChaseMode != chaseMode) {
		if (chaseMode) {
			if (!getFollowCreature() && getAttackedCreature()) {
				//chase opponent
				setFollowCreature(getAttackedCreature());
			}
		} else if (getAttackedCreature()) {
			setFollowCreature(nullptr);
			cancelNextWalk = true;
		}
	}
}

void Player::onWalkAborted()
{
	setNextWalkActionTask(nullptr);
	sendCancelWalk();
}

void Player::onWalkComplete()
{
	if (walkTask) {
		walkTaskEvent = g_scheduler.addEvent(walkTask);
		walkTask = nullptr;
	}
}

void Player::stopWalk()
{
	cancelNextWalk = true;
}

LightInfo Player::getCreatureLight() const
{
	if (internalLight.level > itemsLight.level) {
		return internalLight;
	}
	return itemsLight;
}

void Player::updateItemsLight(bool internal /*=false*/)
{
	LightInfo maxLight;

	for (int32_t i = CONST_SLOT_FIRST; i <= CONST_SLOT_LAST; ++i) {
		if (const auto& item = inventory[i]) {
			LightInfo curLight = item->getLightInfo();

			if (curLight.level > maxLight.level) {
				maxLight = std::move(curLight);
			}
		}
	}

	if (itemsLight.level != maxLight.level || itemsLight.color != maxLight.color) {
		itemsLight = maxLight;

		if (!internal) {
			g_game.changeLight(this->getPlayer());
		}
	}
}

void Player::onAddCondition(const ConditionType_t type)
{
	Creature::onAddCondition(type);

	if (type == CONDITION_OUTFIT && isMounted()) {
		dismount();
	}

	sendIcons();
}

void Player::onAddCombatCondition(const ConditionType_t type)
{
	switch (type) {
		case CONDITION_POISON:
			sendTextMessage(MESSAGE_STATUS_DEFAULT, "You are poisoned.");
			break;

		case CONDITION_DROWN:
			sendTextMessage(MESSAGE_STATUS_DEFAULT, "You are drowning.");
			break;

		case CONDITION_PARALYZE:
			sendTextMessage(MESSAGE_STATUS_DEFAULT, "You are paralyzed.");
			break;

		case CONDITION_DRUNK:
			sendTextMessage(MESSAGE_STATUS_DEFAULT, "You are drunk.");
			break;

		case CONDITION_CURSED:
			sendTextMessage(MESSAGE_STATUS_DEFAULT, "You are cursed.");
			break;

		case CONDITION_FREEZING:
			sendTextMessage(MESSAGE_STATUS_DEFAULT, "You are freezing.");
			break;

		case CONDITION_DAZZLED:
			sendTextMessage(MESSAGE_STATUS_DEFAULT, "You are dazzled.");
			break;

		case CONDITION_BLEEDING:
			sendTextMessage(MESSAGE_STATUS_DEFAULT, "You are bleeding.");
			break;

		default:
			break;
	}
}

void Player::onEndCondition(const ConditionType_t type)
{
	Creature::onEndCondition(type);

	if (type == CONDITION_INFIGHT) {
		onIdleStatus();
		pzLocked = false;
		clearAttacked();

		if (getSkull() != SKULL_RED && getSkull() != SKULL_BLACK) {
			setSkull(SKULL_NONE);
		}
	}

	sendIcons();
}

void Player::onCombatRemoveCondition(Condition* condition)
{
	//Creature::onCombatRemoveCondition(condition);
	if (condition->getId() > 0) {
		//Means the condition is from an item, id == slot
		if (g_game.getWorldType() == WORLD_TYPE_PVP_ENFORCED) {
			if (const auto& item = getInventoryItem(static_cast<slots_t>(condition->getId()))) {
				//25% chance to destroy the item
				if (25 >= uniform_random(1, 100)) {
					g_game.internalRemoveItem(item);
				}
			}
		}
	} else {
		if (!canDoAction()) {
			const uint32_t delay = getNextActionTime();
			const int32_t ticks = delay - (delay % EVENT_CREATURE_THINK_INTERVAL);
			if (ticks < 0) {
				removeCondition(condition);
			} else {
				condition->setTicks(ticks);
			}
		} else {
			removeCondition(condition);
		}
	}
}

void Player::onAttackedCreature(const CreaturePtr& target, bool addFightTicks /* = true */)
{
	Creature::onAttackedCreature(target);

	if (target->getZone() == ZONE_PVP) {
		return;
	}

	if (target == getCreature()) {
		if (addFightTicks) {
			addInFightTicks();
		}
		return;
	}

	if (hasFlag(PlayerFlag_NotGainInFight)) {
		return;
	}

	const auto& targetPlayer = target->getPlayer();
	if (targetPlayer && !isPartner(targetPlayer) && !isGuildMate(targetPlayer)) {
		if (!pzLocked && g_game.getWorldType() == WORLD_TYPE_PVP_ENFORCED) {
			pzLocked = true;
			sendIcons();
		}

		targetPlayer->addInFightTicks();

		if (getSkull() == SKULL_NONE && getSkullClient(targetPlayer) == SKULL_YELLOW) {
			addAttacked(targetPlayer);
			targetPlayer->sendCreatureSkull(this->getPlayer());
		} else if (!targetPlayer->hasAttacked(this->getPlayer())) {
			if (!pzLocked) {
				pzLocked = true;
				sendIcons();
			}

			if (!Combat::isInPvpZone(this->getPlayer(), targetPlayer) && !isInWar(targetPlayer)) {
				addAttacked(targetPlayer);

				if (targetPlayer->getSkull() == SKULL_NONE && getSkull() == SKULL_NONE) {
					setSkull(SKULL_WHITE);
				}

				if (getSkull() == SKULL_NONE) {
					targetPlayer->sendCreatureSkull(this->getPlayer());
				}
			}
		}
	}

	if (addFightTicks) {
		addInFightTicks();
	}
}

void Player::onAttacked()
{
	Creature::onAttacked();

	addInFightTicks();
}

void Player::onIdleStatus()
{
	Creature::onIdleStatus();

	if (party) {
		party->clearPlayerPoints(this->getPlayer());
	}
}

void Player::onPlacedCreature()
{
	//scripting event - onLogin
	if (!g_creatureEvents->playerLogin(this->getPlayer())) {
		kickPlayer(true);
	}
}

void Player::onAttackedCreatureDrainHealth(const CreaturePtr& target, int32_t points)
{
	Creature::onAttackedCreatureDrainHealth(target, points);

	if (target) {
		if (party && !Combat::isPlayerCombat(target)) {
			const auto& tmpMonster = target->getMonster();
			if (tmpMonster && tmpMonster->isHostile()) {
				//We have fulfilled a requirement for shared experience
				party->updatePlayerTicks(this->getPlayer(), points);
			}
		}
	}
}

void Player::onTargetCreatureGainHealth(const CreaturePtr& target, int32_t points)
{
	if (target && party) {
		PlayerPtr tmpPlayer = nullptr;

		if (target->getPlayer()) {
			tmpPlayer = target->getPlayer();
		} else if (const auto& targetMaster = target->getMaster()) {
			if (const auto& targetMasterPlayer = targetMaster->getPlayer()) {
				tmpPlayer = targetMasterPlayer;
			}
		}

		if (isPartner(tmpPlayer)) {
			party->updatePlayerTicks(this->getPlayer(), points);
		}
	}
}

bool Player::onKilledCreature(const CreaturePtr& target, bool lastHit/* = true*/)
{
	bool unjustified = false;

	if (hasFlag(PlayerFlag_NotGenerateLoot)) {
		target->setDropLoot(false);
	}

	Creature::onKilledCreature(target, lastHit);

	PlayerPtr targetPlayer = target->getPlayer();
	if (!targetPlayer) {
		return false;
	}

	if (targetPlayer->getZone() == ZONE_PVP) {
		targetPlayer->setDropLoot(false);
		targetPlayer->setSkillLoss(false);
	} else if (!hasFlag(PlayerFlag_NotGainInFight) && !isPartner(targetPlayer)) {
		if (!Combat::isInPvpZone(this->getPlayer(), targetPlayer) && hasAttacked(targetPlayer) && !targetPlayer->hasAttacked(this->getPlayer()) && !isGuildMate(targetPlayer) && targetPlayer != this->getPlayer()) {
			if (targetPlayer->getSkull() == SKULL_NONE && !isInWar(targetPlayer)) {
				unjustified = true;
				addUnjustifiedDead(targetPlayer);
			}

			if (lastHit && hasCondition(CONDITION_INFIGHT)) {
				pzLocked = true;
				Condition* condition = Condition::createCondition(CONDITIONID_DEFAULT, CONDITION_INFIGHT, g_config.getNumber(ConfigManager::WHITE_SKULL_TIME) * 1000, 0);
				addCondition(condition);
			}
		}
	}

	return unjustified;
}

void Player::gainExperience(uint64_t gainExp, const CreaturePtr& source)
{
	if (hasFlag(PlayerFlag_NotGainExperience) || gainExp == 0 || staminaMinutes == 0) {
		return;
	}

	addExperience(source, gainExp, true);
}

void Player::onGainExperience(uint64_t gainExp, const CreaturePtr& target)
{
	if (hasFlag(PlayerFlag_NotGainExperience)) {
		return;
	}

	if (target && !target->getPlayer() && party && party->isSharedExperienceActive() && party->isSharedExperienceEnabled()) {
		party->shareExperience(gainExp, target);
		//We will get a share of the experience through the sharing mechanism
		return;
	}

	Creature::onGainExperience(gainExp, target);
	gainExperience(gainExp, target);
}

void Player::onGainSharedExperience(uint64_t gainExp, const CreaturePtr& source)
{
	gainExperience(gainExp, source);
}

bool Player::isImmune(CombatType_t type) const
{
	if (hasFlag(PlayerFlag_CannotBeAttacked)) {
		return true;
	}
	return Creature::isImmune(type);
}

bool Player::isImmune(ConditionType_t type) const
{
	if (hasFlag(PlayerFlag_CannotBeAttacked)) {
		return true;
	}
	return Creature::isImmune(type);
}

bool Player::isAttackable() const
{
	return !hasFlag(PlayerFlag_CannotBeAttacked);
}

bool Player::lastHitIsPlayer(const CreaturePtr& lastHitCreature)
{
	if (!lastHitCreature) {
		return false;
	}

	if (lastHitCreature->getPlayer()) {
		return true;
	}

	const auto& lastHitMaster = lastHitCreature->getMaster();
	return lastHitMaster && lastHitMaster->getPlayer();
}

void Player::changeHealth(int32_t healthChange, bool sendHealthChange/* = true*/)
{
	Creature::changeHealth(healthChange, sendHealthChange);
	sendStats();
}

void Player::changeMana(int32_t manaChange)
{
	if (!hasFlag(PlayerFlag_HasInfiniteMana)) {
		if (manaChange > 0) {
			mana += std::min<int32_t>(manaChange, getMaxMana() - mana);
		} else {
			mana = std::max<int32_t>(0, mana + manaChange);
		}
	}

	sendStats();
}

void Player::changeSoul(int32_t soulChange)
{
	if (soulChange > 0) {
		soul += std::min<int32_t>(soulChange, vocation->getSoulMax() - soul);
	} else {
		soul = std::max<int32_t>(0, soul + soulChange);
	}

	sendStats();
}

// to-do: add internal protection for usage of this method and the next one.
void Player::addSoul(uint8_t gain)
{
	if (gain > 0) {
		soul += gain;
	}
	sendStats();
}

void Player::addStamina(uint16_t gain)
{
	if (gain > 0) {
		staminaMinutes += gain;
	}
	sendStats();
}

void Player::changeStamina(int32_t amount)
{
	if (amount > 0) {
		staminaMinutes += std::min<int32_t>(amount, 2520 - staminaMinutes);
	} else {
		staminaMinutes = std::max<int32_t>(0, staminaMinutes + amount);
	}
}

bool Player::canWear(uint32_t lookType, uint8_t addons) const
{
	if (group->access) {
		return true;
	}

	const Outfit* outfit = Outfits::getInstance().getOutfitByLookType(sex, lookType);
	if (!outfit) {
		return false;
	}

	if (outfit->premium && !isPremium()) {
		return false;
	}

	if (!outfit->locked && addons == 0) {
		return true;
	}

	for (const OutfitEntry& outfitEntry : outfits) {
		if (outfitEntry.lookType == lookType) {
			if (outfitEntry.addons == addons || outfitEntry.addons == 3 || addons == 0) {
				return true;
			}
			return false; //have lookType on list and addons don't match
		}
	}
	return false;
}

bool Player::hasOutfit(uint32_t lookType, uint8_t addons) const
{
	const Outfit* outfit = Outfits::getInstance().getOutfitByLookType(sex, lookType);
	if (!outfit) {
		return false;
	}

	if (!outfit->locked && addons == 0) {
		return true;
	}

	for (const OutfitEntry& outfitEntry : outfits) {
		if (outfitEntry.lookType == lookType) {
			if (outfitEntry.addons == addons || outfitEntry.addons == 3 || addons == 0){
				return true;
			}
			return false; //have lookType on list and addons don't match
		}
	}
	return false;
}

void Player::genReservedStorageRange()
{
	//generate outfits range
	uint32_t base_key = PSTRG_OUTFITS_RANGE_START;
	for (const OutfitEntry& entry : outfits) {
		storageMap[++base_key] = (entry.lookType << 16) | entry.addons;
	}
}

void Player::addOutfit(uint16_t lookType, uint8_t addons)
{
	for (OutfitEntry& outfitEntry : outfits) {
		if (outfitEntry.lookType == lookType) {
			outfitEntry.addons |= addons;
			return;
		}
	}
	outfits.emplace_back(lookType, addons);
}

bool Player::removeOutfit(uint16_t lookType)
{
	for (auto it = outfits.begin(), end = outfits.end(); it != end; ++it) {
		OutfitEntry& entry = *it;
		if (entry.lookType == lookType) {
			outfits.erase(it);
			return true;
		}
	}
	return false;
}

bool Player::removeOutfitAddon(uint16_t lookType, uint8_t addons)
{
	for (OutfitEntry& outfitEntry : outfits) {
		if (outfitEntry.lookType == lookType) {
			outfitEntry.addons &= ~addons;
			return true;
		}
	}
	return false;
}

bool Player::getOutfitAddons(const Outfit& outfit, uint8_t& addons) const
{
	if (group->access) {
		addons = 3;
		return true;
	}

	if (outfit.premium && !isPremium()) {
		return false;
	}

	for (const OutfitEntry& outfitEntry : outfits) {
		if (outfitEntry.lookType != outfit.lookType) {
			continue;
		}

		addons = outfitEntry.addons;
		return true;
	}

	if (outfit.locked) {
		return false;
	}

	addons = 0;
	return true;
}

void Player::setSex(const PlayerSex_t newSex)
{
	sex = newSex;
}

Skulls_t Player::getSkull() const
{
	if (hasFlag(PlayerFlag_NotGainInFight)) {
		return SKULL_NONE;
	}
	return skull;
}

Skulls_t Player::getSkullClient(const CreatureConstPtr& creature) const
{
	if (!creature || g_game.getWorldType() != WORLD_TYPE_PVP) {
		return SKULL_NONE;
	}

	const auto& player = creature->getPlayer();
	if (!player || player->getSkull() != SKULL_NONE) {
		return Creature::getSkullClient(creature);
	}

	if (player->hasAttacked(this->getPlayer())) {
		return SKULL_YELLOW;
	}

	if (party && party == player->party) {
		return SKULL_GREEN;
	}
	return Creature::getSkullClient(creature);
}

bool Player::hasAttacked(const PlayerConstPtr& attacked) const
{
	if (hasFlag(PlayerFlag_NotGainInFight) || !attacked) {
		return false;
	}

	return attackedSet.contains(attacked->guid);
}

void Player::addAttacked(const PlayerConstPtr& attacked)
{
	if (hasFlag(PlayerFlag_NotGainInFight) || !attacked || attacked == this->getPlayer()) {
		return;
	}

	attackedSet.insert(attacked->guid);
}

void Player::removeAttacked(const PlayerConstPtr& attacked)
{
	if (!attacked || attacked == this->getPlayer()) {
		return;
	}

	auto it = attackedSet.find(attacked->guid);
	if (it != attackedSet.end()) {
		attackedSet.erase(it);
	}
}

void Player::clearAttacked()
{
	attackedSet.clear();
}

void Player::addUnjustifiedDead(const PlayerConstPtr& attacked)
{
	if (hasFlag(PlayerFlag_NotGainInFight) || attacked == this->getPlayer() || g_game.getWorldType() == WORLD_TYPE_PVP_ENFORCED) {
		return;
	}

	sendTextMessage(MESSAGE_EVENT_ADVANCE, "Warning! The murder of " + attacked->getName() + " was not justified.");

	skullTicks += g_config.getNumber(ConfigManager::FRAG_TIME);

	if (getSkull() != SKULL_BLACK) {
		if (g_config.getNumber(ConfigManager::KILLS_TO_BLACK) != 0 && skullTicks > (g_config.getNumber(ConfigManager::KILLS_TO_BLACK) - 1) * static_cast<int64_t>(g_config.getNumber(ConfigManager::FRAG_TIME))) {
			setSkull(SKULL_BLACK);
		} else if (getSkull() != SKULL_RED && g_config.getNumber(ConfigManager::KILLS_TO_RED) != 0 && skullTicks > (g_config.getNumber(ConfigManager::KILLS_TO_RED) - 1) * static_cast<int64_t>(g_config.getNumber(ConfigManager::FRAG_TIME))) {
			setSkull(SKULL_RED);
		}
	}
}

void Player::checkSkullTicks(const int64_t ticks)
{
	if (const int64_t newTicks = skullTicks - ticks; newTicks < 0) {
		skullTicks = 0;
	} else {
		skullTicks = newTicks;
	}

	if ((skull == SKULL_RED || skull == SKULL_BLACK) && skullTicks < 1 && !hasCondition(CONDITION_INFIGHT)) {
		setSkull(SKULL_NONE);
	}
}

bool Player::isPromoted() const
{
	uint16_t promotedVocation = g_vocations.getPromotedVocation(vocation->getId());
	return promotedVocation == VOCATION_NONE && vocation->getId() != promotedVocation;
}

double Player::getLostPercent() const
{
	int32_t deathLosePercent = g_config.getNumber(ConfigManager::DEATH_LOSE_PERCENT);
	if (deathLosePercent != -1) {
		if (isPromoted()) {
			deathLosePercent -= 3;
		}

		deathLosePercent -= blessings.count();
		return std::max<int32_t>(0, deathLosePercent) / 100.;
	}

	double lossPercent;
	if (level >= 25) {
		const double tmpLevel = level + (levelPercent / 100.);
		lossPercent = static_cast<double>((tmpLevel + 50) * 50 * ((tmpLevel * tmpLevel) - (5 * tmpLevel) + 8)) / experience;
	} else {
		lossPercent = 10;
	}

	double percentReduction = 0;
	if (isPromoted()) {
		percentReduction += 30;
	}
	percentReduction += blessings.count() * 8;
	return lossPercent * (1 - (percentReduction / 100.)) / 100.;
}

void Player::learnInstantSpell(const std::string& spellName)
{
	if (!hasLearnedInstantSpell(spellName)) {
		learnedInstantSpellList.push_front(spellName);
	}
}

void Player::forgetInstantSpell(const std::string& spellName)
{
	learnedInstantSpellList.remove(spellName);
}

bool Player::hasLearnedInstantSpell(const std::string& spellName) const
{
	if (hasFlag(PlayerFlag_CannotUseSpells)) {
		return false;
	}

	if (hasFlag(PlayerFlag_IgnoreSpellCheck)) {
		return true;
	}

	for (const auto& learnedSpellName : learnedInstantSpellList) {
		if (caseInsensitiveEqual(learnedSpellName, spellName)) {
			return true;
		}
	}
	return false;
}

bool Player::isPremium() const
{
	if (g_config.getBoolean(ConfigManager::FREE_PREMIUM) || hasFlag(PlayerFlag_IsAlwaysPremium)) {
		return true;
	}

	return premiumEndsAt > time(nullptr);
}

void Player::setPremiumTime(const time_t premiumEndsAt)
{
	this->premiumEndsAt = premiumEndsAt;
	sendBasicData();
}

PartyShields_t Player::getPartyShield(const PlayerConstPtr& player) const
{
	if (!player) {
		return SHIELD_NONE;
	}

	if (party) {
		if (party->getLeader() == player) {
			if (party->isSharedExperienceActive()) {
				if (party->isSharedExperienceEnabled()) {
					return SHIELD_YELLOW_SHAREDEXP;
				}

				if (party->canUseSharedExperience(player)) {
					return SHIELD_YELLOW_NOSHAREDEXP;
				}

				return SHIELD_YELLOW_NOSHAREDEXP_BLINK;
			}

			return SHIELD_YELLOW;
		}

		if (player->party == party) {
			if (party->isSharedExperienceActive()) {
				if (party->isSharedExperienceEnabled()) {
					return SHIELD_BLUE_SHAREDEXP;
				}

				if (party->canUseSharedExperience(player)) {
					return SHIELD_BLUE_NOSHAREDEXP;
				}

				return SHIELD_BLUE_NOSHAREDEXP_BLINK;
			}

			return SHIELD_BLUE;
		}

		if (isInviting(player)) {
			return SHIELD_WHITEBLUE;
		}
	}

	if (player->isInviting(this->getPlayer())) {
		return SHIELD_WHITEYELLOW;
	}

	if (player->party) {
		return SHIELD_GRAY;
	}

	return SHIELD_NONE;
}

bool Player::isInviting(const PlayerConstPtr& player) const
{
	if (!player || !party || party->getLeader() != this->getPlayer()) {
		return false;
	}
	return party->isPlayerInvited(player);
}

bool Player::isPartner(const PlayerConstPtr& player) const
{
	if (!player || !party || player == this->getPlayer()) {
		return false;
	}
	return party == player->party;
}

void Player::sendPlayerPartyIcons(const PlayerPtr& player) const
{
	sendCreatureShield(player);
	sendCreatureSkull(player);
}

bool Player::addPartyInvitation(Party* party)
{
	if (const auto it = std::ranges::find(invitePartyList, party); it != invitePartyList.end()) {
		return false;
	}

	invitePartyList.push_front(party);
	return true;
}

void Player::removePartyInvitation(Party* party)
{
	invitePartyList.remove(party);
}

void Player::clearPartyInvitations()
{
	for (Party* invitingParty : invitePartyList) {
		invitingParty->removeInvite(this->getPlayer(), false);
	}
	invitePartyList.clear();
}

GuildEmblems_t Player::getGuildEmblem(const PlayerConstPtr& player) const
{
	if (!player) {
		return GUILDEMBLEM_NONE;
	}

	const Guild_ptr playerGuild = player->getGuild();
	if (!playerGuild) {
		return GUILDEMBLEM_NONE;
	}

	if (!playerGuild->isInAnyWar()) {
		if (guild == playerGuild) {
			return GUILDEMBLEM_MEMBER;
		}
		return GUILDEMBLEM_OTHER;
	} else if (guild == playerGuild) {
		return GUILDEMBLEM_ALLY;
	} else if (isInWar(player)) {
		return GUILDEMBLEM_ENEMY;
	}

	return GUILDEMBLEM_NEUTRAL;
}

bool Player::isGuildMate(const PlayerConstPtr player) const
{
	if (!player || !guild) {
		return false;
	}
	return guild == player->guild;
}

bool Player::isGuildWarEnemy(const PlayerConstPtr player, bool alliesAsEnemies) const
{
	if (!player || !guild) {
		return false;
	}

	const auto& playerGuild = player->getGuild();
	if (!playerGuild) {
		return false;
	}

	if (getGuild()->getId() == playerGuild->getId()) {
		return alliesAsEnemies;
	}

	return guild->isInWar(playerGuild->getId()) && playerGuild->isInWar(guild->getId());
}

bool Player::isInWar(const PlayerConstPtr player) const
{
	if (!player || !guild) {
		return false;
	}

	const auto& playerGuild = player->getGuild();
	if (!playerGuild) {
		return false;
	}

	return guild->isInWar(playerGuild->getId()) && playerGuild->isInWar(guild->getId());
}

uint8_t Player::getCurrentMount() const
{
	int32_t value;
	if (getStorageValue(PSTRG_MOUNTS_CURRENTMOUNT, value)) {
		return value;
	}
	return 0;
}

void Player::setCurrentMount(const uint8_t mountId)
{
	addStorageValue(PSTRG_MOUNTS_CURRENTMOUNT, mountId);
}

bool Player::toggleMount(const bool mount)
{
	if ((OTSYS_TIME() - lastToggleMount) < 3000 && !wasMounted) {
		sendCancelMessage(RETURNVALUE_YOUAREEXHAUSTED);
		return false;
	}

	if (mount) {
		if (isMounted()) {
			return false;
		}

		if (!group->access && tile.lock()->hasFlag(TILESTATE_PROTECTIONZONE)) {
			sendCancelMessage(RETURNVALUE_ACTIONNOTPERMITTEDINPROTECTIONZONE);
			return false;
		}

		const auto& playerOutfit = Outfits::getInstance().getOutfitByLookType(getSex(), defaultOutfit.lookType);
		if (!playerOutfit) {
			return false;
		}

		const uint8_t currentMountId = getCurrentMount();
		if (currentMountId == 0) {
			sendOutfitWindow();
			return false;
		}

		const auto& currentMount = g_game.mounts.getMountByID(currentMountId);
		if (!currentMount) {
			return false;
		}

		if (!hasMount(currentMount)) {
			setCurrentMount(0);
			sendOutfitWindow();
			return false;
		}

		if (currentMount->premium && !isPremium()) {
			sendCancelMessage(RETURNVALUE_YOUNEEDPREMIUMACCOUNT);
			return false;
		}

		if (hasCondition(CONDITION_OUTFIT)) {
			sendCancelMessage(RETURNVALUE_NOTPOSSIBLE);
			return false;
		}

		defaultOutfit.lookMount = currentMount->clientId;

		if (currentMount->speed != 0) {
			g_game.changeSpeed(this->getPlayer(), currentMount->speed);
		}
	} else {
		if (!isMounted()) {
			return false;
		}

		dismount();
	}

	g_game.internalCreatureChangeOutfit(this->getPlayer(), defaultOutfit);
	lastToggleMount = OTSYS_TIME();
	return true;
}

bool Player::tameMount(const uint8_t mountId)
{
	if (!g_game.mounts.getMountByID(mountId)) {
		return false;
	}

	const uint8_t tmpMountId = mountId - 1;
	const uint32_t key = PSTRG_MOUNTS_RANGE_START + (tmpMountId / 31);

	int32_t value;
	if (getStorageValue(key, value)) {
		value |= (1 << (tmpMountId % 31));
	} else {
		value = (1 << (tmpMountId % 31));
	}

	addStorageValue(key, value);
	return true;
}

bool Player::untameMount(const uint8_t mountId)
{
	if (!g_game.mounts.getMountByID(mountId)) {
		return false;
	}

	const uint8_t tmpMountId = mountId - 1;
	const uint32_t key = PSTRG_MOUNTS_RANGE_START + (tmpMountId / 31);

	int32_t value;
	if (!getStorageValue(key, value)) {
		return true;
	}

	value &= ~(1 << (tmpMountId % 31));
	addStorageValue(key, value);

	if (getCurrentMount() == mountId) {
		if (isMounted()) {
			dismount();
			g_game.internalCreatureChangeOutfit(this->getPlayer(), defaultOutfit);
		}

		setCurrentMount(0);
	}

	return true;
}

bool Player::hasMount(const Mount* mount) const
{
	if (isAccessPlayer()) {
		return true;
	}

	if (mount->premium && !isPremium()) {
		return false;
	}

	const uint8_t tmpMountId = mount->id - 1;

	int32_t value;
	if (!getStorageValue(PSTRG_MOUNTS_RANGE_START + (tmpMountId / 31), value)) {
		return false;
	}

	return ((1 << (tmpMountId % 31)) & value) != 0;
}

void Player::dismount()
{
	const auto& mount = g_game.mounts.getMountByID(getCurrentMount());
	if (mount && mount->speed > 0) {
		g_game.changeSpeed(this->getPlayer(), -mount->speed);
	}

	defaultOutfit.lookMount = 0;
}

bool Player::addOfflineTrainingTries(skills_t skill, uint64_t tries)
{
	if (tries == 0 || skill == SKILL_LEVEL) {
		return false;
	}

	bool sendUpdate = false;
	uint32_t oldSkillValue, newSkillValue;
	long double oldPercentToNextLevel, newPercentToNextLevel;

	if (skill == SKILL_MAGLEVEL) {
		uint64_t currReqMana = vocation->getReqMana(magLevel);
		uint64_t nextReqMana = vocation->getReqMana(magLevel + 1);

		if (currReqMana >= nextReqMana) {
			return false;
		}

		oldSkillValue = magLevel;
		oldPercentToNextLevel = static_cast<long double>(manaSpent * 100) / nextReqMana;

		g_events->eventPlayerOnGainSkillTries(this->getPlayer(), SKILL_MAGLEVEL, tries);
		uint32_t currMagLevel = magLevel;

		while ((manaSpent + tries) >= nextReqMana) {
			tries -= nextReqMana - manaSpent;

			magLevel++;
			manaSpent = 0;

			g_creatureEvents->playerAdvance(this->getPlayer(), SKILL_MAGLEVEL, magLevel - 1, magLevel);

			sendUpdate = true;
			currReqMana = nextReqMana;
			nextReqMana = vocation->getReqMana(magLevel + 1);

			if (currReqMana >= nextReqMana) {
				tries = 0;
				break;
			}
		}

		manaSpent += tries;

		if (magLevel != currMagLevel) {
			sendTextMessage(MESSAGE_EVENT_ADVANCE, fmt::format("You advanced to magic level {:d}.", magLevel));
		}

		uint8_t newPercent;
		if (nextReqMana > currReqMana) {
			newPercent = Player::getPercentLevel(manaSpent, nextReqMana);
			newPercentToNextLevel = static_cast<long double>(manaSpent * 100) / nextReqMana;
		} else {
			newPercent = 0;
			newPercentToNextLevel = 0;
		}

		if (newPercent != magLevelPercent) {
			magLevelPercent = newPercent;
			sendUpdate = true;
		}

		newSkillValue = magLevel;
	} else {
		uint64_t currReqTries = vocation->getReqSkillTries(skill, skills[skill].level);
		uint64_t nextReqTries = vocation->getReqSkillTries(skill, skills[skill].level + 1);
		if (currReqTries >= nextReqTries) {
			return false;
		}

		oldSkillValue = skills[skill].level;
		oldPercentToNextLevel = static_cast<long double>(skills[skill].tries * 100) / nextReqTries;

		g_events->eventPlayerOnGainSkillTries(this->getPlayer(), skill, tries);
		uint32_t currSkillLevel = skills[skill].level;

		while ((skills[skill].tries + tries) >= nextReqTries) {
			tries -= nextReqTries - skills[skill].tries;

			skills[skill].level++;
			skills[skill].tries = 0;
			skills[skill].percent = 0;

			g_creatureEvents->playerAdvance(this->getPlayer(), skill, (skills[skill].level - 1), skills[skill].level);

			sendUpdate = true;
			currReqTries = nextReqTries;
			nextReqTries = vocation->getReqSkillTries(skill, skills[skill].level + 1);

			if (currReqTries >= nextReqTries) {
				tries = 0;
				break;
			}
		}

		skills[skill].tries += tries;

		if (currSkillLevel != skills[skill].level) {
			sendTextMessage(MESSAGE_EVENT_ADVANCE, fmt::format("You advanced to {:s} level {:d}.", getSkillName(skill), skills[skill].level));
		}

		uint8_t newPercent;
		if (nextReqTries > currReqTries) {
			newPercent = Player::getPercentLevel(skills[skill].tries, nextReqTries);
			newPercentToNextLevel = static_cast<long double>(skills[skill].tries * 100) / nextReqTries;
		} else {
			newPercent = 0;
			newPercentToNextLevel = 0;
		}

		if (skills[skill].percent != newPercent) {
			skills[skill].percent = newPercent;
			sendUpdate = true;
		}

		newSkillValue = skills[skill].level;
	}

	if (sendUpdate) {
		sendSkills();
	}

	sendTextMessage(MESSAGE_EVENT_ADVANCE, fmt::format("Your {:s} skill changed from level {:d} (with {:.2f}% progress towards level {:d}) to level {:d} (with {:.2f}% progress towards level {:d})", ucwords(getSkillName(skill)), oldSkillValue, oldPercentToNextLevel, (oldSkillValue + 1), newSkillValue, newPercentToNextLevel, (newSkillValue + 1)));
	return sendUpdate;
}

bool Player::hasModalWindowOpen(const uint32_t modalWindowId) const
{
	return std::ranges::find(modalWindows, modalWindowId) != modalWindows.end();
}

void Player::onModalWindowHandled(const uint32_t modalWindowId)
{
	modalWindows.remove(modalWindowId);
}

void Player::sendModalWindow(const ModalWindow& modalWindow)
{
	if (!client) {
		return;
	}

	modalWindows.push_front(modalWindow.id);
	client->sendModalWindow(modalWindow);
}

void Player::clearModalWindows()
{
	modalWindows.clear();
}

uint16_t Player::getHelpers() const
{
	uint16_t helpers;

	if (guild && party) {
		std::unordered_set<PlayerPtr> helperSet;

		auto guildMembers = guild->getMembersOnline();
		helperSet.insert(guildMembers.begin(), guildMembers.end());

		auto partyMembers = party->getMembers();
		helperSet.insert(partyMembers.begin(), partyMembers.end());

		auto partyInvitees = party->getInvitees();
		helperSet.insert(partyInvitees.begin(), partyInvitees.end());

		helperSet.insert(party->getLeader());

		helpers = helperSet.size();
	} else if (guild) {
		helpers = guild->getMembersOnline().size();
	} else if (party) {
		helpers = party->getMemberCount() + party->getInvitationCount() + 1;
	} else {
		helpers = 0;
	}

	return helpers;
}

void Player::sendClosePrivate(const uint16_t channelId)
{
	if (channelId == CHANNEL_GUILD || channelId == CHANNEL_PARTY) {
		g_chat->removeUserFromChannel(this->getPlayer(), channelId);
	}

	if (client) {
		client->sendClosePrivate(channelId);
	}
}

uint64_t Player::getMoney() const
{
	std::vector<ContainerPtr> containers;
	uint64_t moneyCount = 0;

	for (int32_t i = CONST_SLOT_FIRST; i <= CONST_SLOT_LAST; ++i) {
		const auto& item = inventory[i];
		if (!item) {
			continue;
		}

		if (const auto& container = item->getContainer()) {
			containers.push_back(container);
		} else {
			moneyCount += item->getWorth();
		}
	}

	size_t i = 0;
	while (i < containers.size()) {
		const auto& container = containers[i++];
		for (const auto& item : container->getItemList()) {
			if (auto tmpContainer = item->getContainer()) {
				containers.push_back(tmpContainer);
			} else {
				moneyCount += item->getWorth();
			}
		}
	}
	return moneyCount;
}

size_t Player::getMaxVIPEntries() const
{
	if (group->maxVipEntries != 0) {
		return group->maxVipEntries;
	}

	return g_config.getNumber(isPremium() ? ConfigManager::VIP_PREMIUM_LIMIT : ConfigManager::VIP_FREE_LIMIT);
}

size_t Player::getMaxDepotItems() const
{
	if (group->maxDepotItems != 0) {
		return group->maxDepotItems;
	}

	return g_config.getNumber(isPremium() ? ConfigManager::DEPOT_PREMIUM_LIMIT : ConfigManager::DEPOT_FREE_LIMIT);
}

const bool Player::addAugment(const std::shared_ptr<Augment>& augment) {
	if (std::ranges::find(augments, augment) == augments.end()) {
		augments.push_back(augment);
		g_events->eventPlayerOnAugment(this->getPlayer(), augment);
		return true;
	}
	return false;
}

const bool Player::addAugment(const std::string_view augmentName) {

	if (auto augment = Augments::GetAugment(augmentName)) {
		augments.emplace_back(augment);
		g_events->eventPlayerOnAugment(this->getPlayer(), augment);
		return true;
	}
	return false;
}

const bool Player::removeAugment(const std::shared_ptr<Augment>& augment) {
	if (const auto it = std::ranges::find(augments, augment); it != augments.end()) {
		g_events->eventPlayerOnRemoveAugment(this->getPlayer(), augment);
		augments.erase(it);
		return true;
	}
	return false;
}

const bool Player::isAugmented() const
{
	return augments.size() > 0;
}

const bool Player::hasAugment(const std::string_view augmentName, bool checkItems)
{
	for (const auto& augment : augments) {
		if (augment->getName() == augmentName) {
			return true;
		}
	}

	if (checkItems) {
		for (int32_t slot = CONST_SLOT_FIRST; slot <= CONST_SLOT_LAST; ++slot) {
			const auto& item = inventory[slot];
			for (const auto& aug : item->getAugments()) {
				if (aug->getName() == augmentName) {
					return true;
				}
			}
		}
	}

	return false;
}

const bool Player::hasAugment(const std::shared_ptr<Augment>& augment, bool checkItems)
{
	for (const auto& aug : augments) {
		if (aug == augment) {
			return true;
		}
	}

	if (checkItems) {
		for (int32_t slot = CONST_SLOT_FIRST; slot <= CONST_SLOT_LAST; ++slot) {
			const auto& item = inventory[slot];
			for (const auto& aug : item->getAugments()) {
				if (aug == augment) {
					return true;
				}
			}
		}
	}

	return false;
}

const std::vector<std::shared_ptr<Augment>> Player::getPlayerAugments() const {
	return augments;
}

const bool Player::removeAugment(std::string_view augmentName) {
	const auto& originalSize = augments.size();
	
	std::erase_if(augments,
	              [&](const std::shared_ptr<Augment>& augment) {
		              if (const auto match = augment->getName() == augmentName) {
			              g_events->eventPlayerOnRemoveAugment(this->getPlayer(), augment);
		              }
		              return augment->getName() == augmentName;
	              });
	
	return augments.size() > originalSize;
}


std::forward_list<Condition*> Player::getMuteConditions() const
{
	std::forward_list<Condition*> muteConditions;
	for (Condition* condition : conditions) {
		if (condition->getTicks() <= 0) {
			continue;
		}

		ConditionType_t type = condition->getType();
		if (type != CONDITION_MUTED && type != CONDITION_CHANNELMUTEDTICKS && type != CONDITION_YELLTICKS) {
			continue;
		}

		muteConditions.push_front(condition);
	}
	return muteConditions;
}

void Player::setGuild(Guild_ptr guild)
{
	if (guild == this->guild) {
		return;
	}

	Guild_ptr oldGuild = this->guild;

	this->guildNick.clear();
	this->guild = nullptr;
	this->guildRank = nullptr;

	if (guild) {
		GuildRank_ptr rank = guild->getRankByLevel(1);
		if (!rank) {
			return;
		}

		this->guild = guild;
		this->guildRank = rank;
		guild->addMember(this->getPlayer());
	}

	if (oldGuild) {
		oldGuild->removeMember(this->getPlayer());
	}
}

void Player::updateRegeneration() const
{
	if (!vocation) {
		return;
	}

	Condition* condition = getCondition(CONDITION_REGENERATION, CONDITIONID_DEFAULT);
	if (condition) {
		condition->setParam(CONDITION_PARAM_HEALTHGAIN, vocation->getHealthGainAmount());
		condition->setParam(CONDITION_PARAM_HEALTHTICKS, vocation->getHealthGainTicks() * 1000);
		condition->setParam(CONDITION_PARAM_MANAGAIN, vocation->getManaGainAmount());
		condition->setParam(CONDITION_PARAM_MANATICKS, vocation->getManaGainTicks() * 1000);
		condition->setParam(CONDITION_PARAM_SOULGAIN, vocation->getSoulGainAmount());
		condition->setParam(CONDITION_PARAM_SOULTICKS, vocation->getSoulGainTicks() * 1000);
	}
}

void Player::addItemImbuements(const ItemPtr& item) {
	if (item->hasImbuements()) {
		const std::vector<std::shared_ptr<Imbuement>>& imbuementList = item->getImbuements();
		for (auto& imbue : imbuementList) {
			if (imbue->isSkill()) {
				switch (imbue->imbuetype) {
					case ImbuementType::IMBUEMENT_TYPE_FIST_SKILL:
						setVarSkill(SKILL_FIST, static_cast<int32_t>(imbue->value));
						break;
					case ImbuementType::IMBUEMENT_TYPE_CLUB_SKILL:
						setVarSkill(SKILL_CLUB, static_cast<int32_t>(imbue->value));
						break;
					case ImbuementType::IMBUEMENT_TYPE_SWORD_SKILL:
						setVarSkill(SKILL_SWORD, static_cast<int32_t>(imbue->value));
						break;
					case ImbuementType::IMBUEMENT_TYPE_AXE_SKILL:
						setVarSkill(SKILL_AXE, static_cast<int32_t>(imbue->value));
						break;
					case ImbuementType::IMBUEMENT_TYPE_DISTANCE_SKILL:
						setVarSkill(SKILL_DISTANCE, static_cast<int32_t>(imbue->value));
						break;
					case ImbuementType::IMBUEMENT_TYPE_SHIELD_SKILL:
						setVarSkill(SKILL_SHIELD, static_cast<int32_t>(imbue->value));
						break;
					case ImbuementType::IMBUEMENT_TYPE_FISHING_SKILL:
						setVarSkill(SKILL_FISHING, static_cast<int32_t>(imbue->value));
						break;
					case ImbuementType::IMBUEMENT_TYPE_MAGIC_LEVEL:
						setVarSkill(SKILL_MAGLEVEL, static_cast<int32_t>(imbue->value));
						break;
				}
			}

			if (imbue->isSpecialSkill()) {
				switch (imbue->imbuetype) {
					case ImbuementType::IMBUEMENT_TYPE_MANA_LEECH:
						setVarSpecialSkill(SPECIALSKILL_MANALEECHAMOUNT, static_cast<int32_t>(imbue->value));
						break;
					case ImbuementType::IMBUEMENT_TYPE_LIFE_LEECH:
						setVarSpecialSkill(SPECIALSKILL_LIFELEECHAMOUNT, static_cast<int32_t>(imbue->value));
						break;
					case ImbuementType::IMBUEMENT_TYPE_CRITICAL_CHANCE:
						setVarSpecialSkill(SPECIALSKILL_CRITICALHITCHANCE, static_cast<int32_t>(imbue->value));
						break;
					case ImbuementType::IMBUEMENT_TYPE_CRITICAL_AMOUNT:
						setVarSpecialSkill(SPECIALSKILL_CRITICALHITAMOUNT, static_cast<int32_t>(imbue->value));
						break;
				}
			}

			if (imbue->isStat()) {
				switch (imbue->imbuetype) {
					case ImbuementType::IMBUEMENT_TYPE_CAPACITY_BOOST:
						capacity += imbue->value;
						break;
					case ImbuementType::IMBUEMENT_TYPE_SPEED_BOOST:
						g_game.changeSpeed(this->getPlayer(), static_cast<int32_t>(imbue->value));
						break;
				}
			}
		}
	}
	checkForImbuedEquipment();
	sendSkills();
	sendStats();
}

void Player::removeItemImbuements(const ItemPtr& item) {
	if (item->hasImbuements()) {
		const std::vector<std::shared_ptr<Imbuement>>& imbuementList = item->getImbuements();
		for (auto& imbue : imbuementList) {
			if (imbue->isSkill()) {
				switch (imbue->imbuetype) {
					case ImbuementType::IMBUEMENT_TYPE_FIST_SKILL:
						setVarSkill(SKILL_FIST, -static_cast<int32_t>(imbue->value));
						break;
					case ImbuementType::IMBUEMENT_TYPE_CLUB_SKILL:
						setVarSkill(SKILL_CLUB, -static_cast<int32_t>(imbue->value));
						break;
					case ImbuementType::IMBUEMENT_TYPE_SWORD_SKILL:
						setVarSkill(SKILL_SWORD, -static_cast<int32_t>(imbue->value));
						break;
					case ImbuementType::IMBUEMENT_TYPE_AXE_SKILL:
						setVarSkill(SKILL_AXE, -static_cast<int32_t>(imbue->value));
						break;
					case ImbuementType::IMBUEMENT_TYPE_DISTANCE_SKILL:
						setVarSkill(SKILL_DISTANCE, -static_cast<int32_t>(imbue->value));
						break;
					case ImbuementType::IMBUEMENT_TYPE_SHIELD_SKILL:
						setVarSkill(SKILL_SHIELD, -static_cast<int32_t>(imbue->value));
						break;
					case ImbuementType::IMBUEMENT_TYPE_FISHING_SKILL:
						setVarSkill(SKILL_FISHING, -static_cast<int32_t>(imbue->value));
						break;
					case ImbuementType::IMBUEMENT_TYPE_MAGIC_LEVEL:
						setVarSkill(SKILL_MAGLEVEL, -static_cast<int32_t>(imbue->value));
						break;
				}
			}

			if (imbue->isSpecialSkill()) {
				switch (imbue->imbuetype) {
					case ImbuementType::IMBUEMENT_TYPE_MANA_LEECH:
						setVarSpecialSkill(SPECIALSKILL_MANALEECHAMOUNT, -static_cast<int32_t>(imbue->value));
						break;
					case ImbuementType::IMBUEMENT_TYPE_LIFE_LEECH:
						setVarSpecialSkill(SPECIALSKILL_LIFELEECHAMOUNT, -static_cast<int32_t>(imbue->value));
						break;
					case ImbuementType::IMBUEMENT_TYPE_CRITICAL_CHANCE:
						setVarSpecialSkill(SPECIALSKILL_CRITICALHITCHANCE, -static_cast<int32_t>(imbue->value));
						break;
					case ImbuementType::IMBUEMENT_TYPE_CRITICAL_AMOUNT:
						setVarSpecialSkill(SPECIALSKILL_CRITICALHITAMOUNT, -static_cast<int32_t>(imbue->value));
						break;
				}
			}

			if (imbue->isStat()) {
				switch (imbue->imbuetype) {
				case ImbuementType::IMBUEMENT_TYPE_CAPACITY_BOOST:
					capacity -= imbue->value;
					break;
				case ImbuementType::IMBUEMENT_TYPE_SPEED_BOOST:
					g_game.changeSpeed(this->getPlayer(), -static_cast<int32_t>(imbue->value));
					break;
				}
			}
		}
	}
	checkForImbuedEquipment();
	sendSkills();
	sendStats();
}


void Player::removeImbuementEffect(const std::shared_ptr<Imbuement>& imbue) {
	
	if (imbue->isSkill()) {
		switch (imbue->imbuetype) {
		case ImbuementType::IMBUEMENT_TYPE_FIST_SKILL:
			setVarSkill(SKILL_FIST, -static_cast<int32_t>(imbue->value));
			break;
		case ImbuementType::IMBUEMENT_TYPE_CLUB_SKILL:
			setVarSkill(SKILL_CLUB, -static_cast<int32_t>(imbue->value));
			break;
		case ImbuementType::IMBUEMENT_TYPE_SWORD_SKILL:
			setVarSkill(SKILL_SWORD, -static_cast<int32_t>(imbue->value));
			break;
		case ImbuementType::IMBUEMENT_TYPE_AXE_SKILL:
			setVarSkill(SKILL_AXE, -static_cast<int32_t>(imbue->value));
			break;
		case ImbuementType::IMBUEMENT_TYPE_DISTANCE_SKILL:
			setVarSkill(SKILL_DISTANCE, -static_cast<int32_t>(imbue->value));
			break;
		case ImbuementType::IMBUEMENT_TYPE_SHIELD_SKILL:
			setVarSkill(SKILL_SHIELD, -static_cast<int32_t>(imbue->value));
			break;
		case ImbuementType::IMBUEMENT_TYPE_FISHING_SKILL:
			setVarSkill(SKILL_FISHING, -static_cast<int32_t>(imbue->value));
			break;
		case ImbuementType::IMBUEMENT_TYPE_MAGIC_LEVEL:
			setVarSkill(SKILL_MAGLEVEL, -static_cast<int32_t>(imbue->value));
			break;
		}
	}

	if (imbue->isSpecialSkill()) {
		switch (imbue->imbuetype) {
		case ImbuementType::IMBUEMENT_TYPE_MANA_LEECH:
			setVarSpecialSkill(SPECIALSKILL_MANALEECHAMOUNT, -static_cast<int32_t>(imbue->value));
			break;
		case ImbuementType::IMBUEMENT_TYPE_LIFE_LEECH:
			setVarSpecialSkill(SPECIALSKILL_LIFELEECHAMOUNT, -static_cast<int32_t>(imbue->value));
			break;
		case ImbuementType::IMBUEMENT_TYPE_CRITICAL_CHANCE:
			setVarSpecialSkill(SPECIALSKILL_CRITICALHITCHANCE, -static_cast<int32_t>(imbue->value));
			break;
		case ImbuementType::IMBUEMENT_TYPE_CRITICAL_AMOUNT:
			setVarSpecialSkill(SPECIALSKILL_CRITICALHITAMOUNT, -static_cast<int32_t>(imbue->value));
			break;
		}
	}

	if (imbue->isStat()) {
		switch (imbue->imbuetype) {
		case ImbuementType::IMBUEMENT_TYPE_CAPACITY_BOOST:
			capacity -= imbue->value;
			break;
		case ImbuementType::IMBUEMENT_TYPE_SPEED_BOOST:
			g_game.changeSpeed(this->getPlayer(), -static_cast<int32_t>(imbue->value));
			break;
		}
	}
	sendSkills();
	sendStats();
}

void Player::addImbuementEffect(const std::shared_ptr<Imbuement>& imbue) {

	if (imbue->isSkill()) {
		switch (imbue->imbuetype) {
		case ImbuementType::IMBUEMENT_TYPE_FIST_SKILL:
			setVarSkill(SKILL_FIST, static_cast<int32_t>(imbue->value));
			break;
		case ImbuementType::IMBUEMENT_TYPE_CLUB_SKILL:
			setVarSkill(SKILL_CLUB, static_cast<int32_t>(imbue->value));
			break;
		case ImbuementType::IMBUEMENT_TYPE_SWORD_SKILL:
			setVarSkill(SKILL_SWORD, static_cast<int32_t>(imbue->value));
			break;
		case ImbuementType::IMBUEMENT_TYPE_AXE_SKILL:
			setVarSkill(SKILL_AXE, static_cast<int32_t>(imbue->value));
			break;
		case ImbuementType::IMBUEMENT_TYPE_DISTANCE_SKILL:
			setVarSkill(SKILL_DISTANCE, static_cast<int32_t>(imbue->value));
			break;
		case ImbuementType::IMBUEMENT_TYPE_SHIELD_SKILL:
			setVarSkill(SKILL_SHIELD, static_cast<int32_t>(imbue->value));
			break;
		case ImbuementType::IMBUEMENT_TYPE_FISHING_SKILL:
			setVarSkill(SKILL_FISHING, static_cast<int32_t>(imbue->value));
			break;
		case ImbuementType::IMBUEMENT_TYPE_MAGIC_LEVEL:
			setVarSkill(SKILL_MAGLEVEL, static_cast<int32_t>(imbue->value));
			break;
		}
	}

	if (imbue->isSpecialSkill()) {
		switch (imbue->imbuetype) {
		case ImbuementType::IMBUEMENT_TYPE_MANA_LEECH:
			setVarSpecialSkill(SPECIALSKILL_MANALEECHAMOUNT, static_cast<int32_t>(imbue->value));
			break;
		case ImbuementType::IMBUEMENT_TYPE_LIFE_LEECH:
			setVarSpecialSkill(SPECIALSKILL_LIFELEECHAMOUNT, static_cast<int32_t>(imbue->value));
			break;
		case ImbuementType::IMBUEMENT_TYPE_CRITICAL_CHANCE:
			setVarSpecialSkill(SPECIALSKILL_CRITICALHITCHANCE, static_cast<int32_t>(imbue->value));
			break;
		case ImbuementType::IMBUEMENT_TYPE_CRITICAL_AMOUNT:
			setVarSpecialSkill(SPECIALSKILL_CRITICALHITAMOUNT, static_cast<int32_t>(imbue->value));
			break;
		}
	}

	if (imbue->isStat()) {
		switch (imbue->imbuetype) {
		case ImbuementType::IMBUEMENT_TYPE_CAPACITY_BOOST:
			capacity += imbue->value;
			break;
		case ImbuementType::IMBUEMENT_TYPE_SPEED_BOOST:
			g_game.changeSpeed(this->getPlayer(), static_cast<int32_t>(imbue->value));
			break;
		}
	}
	sendSkills();
	sendStats();
}

CreatureType_t Player::getCreatureType(const MonsterPtr& monster) const
{
	auto creatureType = CREATURETYPE_MONSTER;
	if (monster->getFriendList().size() > 0) {
		for (auto monsterFriend : monster->getFriendList()) {
			if (const auto& ally = monsterFriend->getPlayer()) {

				if (ally->getGuild() && this->getGuild() && ally->getGuild() == this->getGuild()) {
					creatureType = CREATURETYPE_SUMMON_GUILD;
				}

				if (ally->getParty() && this->getParty() && ally->getParty() == this->getParty()) {
					creatureType = CREATURETYPE_SUMMON_PARTY;
				}
			}
		}
	}

	if (monster->getMaster() && monster->getMaster()->getID() == this->getID()) {
		creatureType = CREATURETYPE_SUMMON_OWN;
	}
	return creatureType;
}

static ModifierTotals getValidatedTotals(const std::vector<std::shared_ptr<DamageModifier>>& modifierList, const CombatType_t damageType, const CombatOrigin originType, const CreatureType_t creatureType, const RaceType_t race, const std::string_view creatureName) {
	uint16_t percent = 0;
	uint16_t flat = 0;
	// to-do: const and auto&
	for (auto& modifier : modifierList) {

		if (modifier->appliesToDamage(damageType) && modifier->appliesToOrigin(originType) && modifier->appliesToTarget(creatureType, race, creatureName)) {
			if (modifier->isFlatValue() && modifier->getChance() == 0 || modifier->isFlatValue() && modifier->getChance() == 100) {
					flat += modifier->getValue();
					continue;
			} else if (modifier->isFlatValue()) {
				if (modifier->getChance() >= uniform_random(1, 100)) {
					flat += modifier->getValue();
					continue;
				}
			}

			if (modifier->isPercent() && modifier->getChance() == 0 || modifier->isPercent() && modifier->getChance() == 100) {
				percent += modifier->getValue();
				continue;
			} else if (modifier->isPercent()) {
				if (modifier->getChance() >= uniform_random(1, 100)) {
					percent += modifier->getValue();
					continue;
				}
			}
		}
	}
	percent = std::clamp<uint16_t>(percent, 0, 100);
	return ModifierTotals(flat, percent);
}

gtl::node_hash_map <uint8_t, std::vector<std::shared_ptr<DamageModifier>>> Player::getAttackModifiers() const
{
	gtl::node_hash_map<uint8_t, std::vector<std::shared_ptr<DamageModifier>>> modifierMap;

	if (!augments.empty()) {
		for (const auto& aug : augments) {
			for (const auto& mod : aug->getAttackModifiers()) {
				modifierMap[mod->getType()].emplace_back(mod);
			}
		}
	}

	for (uint8_t slot = CONST_SLOT_FIRST; slot <= CONST_SLOT_RING; ++slot) {
		if (const auto& item = inventory[slot]; item && !item->getAugments().empty()) {
			for (const auto& aug : item->getAugments()) {
				if (!g_config.getBoolean(ConfigManager::AUGMENT_SLOT_PROTECTION) || (item->getEquipSlot() == getPositionForSlot(static_cast<slots_t>(slot)))) {
					for (const auto& mod : aug->getAttackModifiers()) {
						modifierMap[mod->getType()].emplace_back(mod);
					}
				} else if ( g_config.getBoolean(ConfigManager::AUGMENT_SLOT_PROTECTION) && (slot == CONST_SLOT_RIGHT || slot == CONST_SLOT_LEFT) && (item->getWeaponType() != WEAPON_NONE && item->getWeaponType() != WEAPON_AMMO)) {
					for (const auto& mod : aug->getAttackModifiers()) {
						modifierMap[mod->getType()].emplace_back(mod);
					}
				}
			}
		}
	}

	return modifierMap;
}

gtl::node_hash_map <uint8_t, std::vector<std::shared_ptr<DamageModifier>>> Player::getDefenseModifiers() const
{
	gtl::node_hash_map<uint8_t, std::vector<std::shared_ptr<DamageModifier>>> modifierMap;

	if (!augments.empty()) {
		for (const auto& aug : augments) {
			for (const auto& mod : aug->getDefenseModifiers()) {
				modifierMap[mod->getType()].emplace_back(mod);
			}
		}
	}

	for (uint8_t slot = CONST_SLOT_FIRST; slot <= CONST_SLOT_RING; ++slot) {
		if (const auto& item = inventory[slot]; item && !item->getAugments().empty()) {
			for (const auto& aug : item->getAugments()) {
				if (!g_config.getBoolean(ConfigManager::AUGMENT_SLOT_PROTECTION) || (item->getEquipSlot() == getPositionForSlot(static_cast<slots_t>(slot)))) {
					for (const auto& mod : aug->getDefenseModifiers()) {
						modifierMap[mod->getType()].emplace_back(mod);
					}
				} else if (g_config.getBoolean(ConfigManager::AUGMENT_SLOT_PROTECTION) && (slot == CONST_SLOT_RIGHT || slot == CONST_SLOT_LEFT) && (item->getWeaponType() != WEAPON_NONE && item->getWeaponType() != WEAPON_AMMO)) {
					for (const auto& mod : aug->getDefenseModifiers()) {
						modifierMap[mod->getType()].emplace_back(mod);
					}
				}
			}
		}
	}
	return modifierMap;
}

gtl::node_hash_map<uint8_t, ModifierTotals> Player::getConvertedTotals(const uint8_t modType, const CombatType_t damageType, const CombatOrigin originType, const CreatureType_t creatureType, const RaceType_t race, const std::string_view creatureName)
{
	gtl::node_hash_map<uint8_t, ModifierTotals> playerList;
	playerList.reserve(COMBAT_COUNT);

	gtl::node_hash_map<uint8_t, ModifierTotals> itemList;
	itemList.reserve(COMBAT_COUNT);
	
	[[unlikely]]
	if ((modType != ATTACK_MODIFIER_CONVERSION) && (modType != DEFENSE_MODIFIER_REFORM)) {
		std::cout << "::: WARNING Player::getConvertedTotals called with invalid Mod Type! \n";
		return playerList;
	}

	if (!augments.empty()) {
		for (const auto& aug : augments) {
			const auto& modifiers = modType == ATTACK_MODIFIER_CONVERSION ? aug->getAttackModifiers(modType) : aug->getDefenseModifiers(modType);
			for (const auto& modifier : modifiers) {
				if (modifier->appliesToDamage(damageType) && modifier->appliesToOrigin(originType) && modifier->appliesToTarget(creatureType, race, creatureName)) {

					uint16_t flat = 0;
					uint16_t percent = 0;

					if (modifier->isFlatValue() && modifier->getChance() == 0 || modifier->isFlatValue() && modifier->getChance() == 100) {
						flat += modifier->getValue();
					} else if (modifier->isFlatValue()) {
						if (modifier->getChance() >= uniform_random(1, 100)) {
							flat += modifier->getValue();
						}
					}

					if (modifier->isPercent() && modifier->getChance() == 0 || modifier->isPercent() && modifier->getChance() == 100) {
						percent += modifier->getValue();
					} else if (modifier->isPercent()) {
						if (modifier->getChance() >= uniform_random(1, 100)) {
							percent += modifier->getValue();
						}
					}

					percent = std::min<uint16_t>(percent, 100);
					const auto& index = combatTypeToIndex(modifier->getConversionType());
					if (auto [it, inserted] = playerList.try_emplace(index, ModifierTotals{flat, percent}); !inserted) {
						it->second += ModifierTotals{flat, percent};
					}
				}
			}
		}
	}

	for (int32_t slot = CONST_SLOT_FIRST; slot <= CONST_SLOT_LAST; ++slot) {
		if (const auto& item = inventory[slot]; item && !item->getAugments().empty()) {
			for (const auto& aug : item->getAugments()) {
				const auto& modifiers = modType == ATTACK_MODIFIER_CONVERSION ? aug->getAttackModifiers(modType) : aug->getDefenseModifiers(modType);
				for (const auto& modifier : modifiers) {
					if (modifier->appliesToDamage(damageType) && modifier->appliesToOrigin(originType) && modifier->appliesToTarget(creatureType, race, creatureName)) {

						uint16_t flat = 0;
						uint16_t percent = 0;

						if (modifier->isFlatValue() && modifier->getChance() == 0 || modifier->isFlatValue() && modifier->getChance() == 100) {
							flat += modifier->getValue();
						} else if (modifier->isFlatValue()) {
							if (modifier->getChance() >= uniform_random(1, 100)) {
								flat += modifier->getValue();
							}
						}

						if (modifier->isPercent() && modifier->getChance() == 0 || modifier->isPercent() && modifier->getChance() == 100) {
							percent += modifier->getValue();
						} else if (modifier->isPercent()) {
							if (modifier->getChance() >= uniform_random(1, 100)) {
								percent += modifier->getValue();
							}
						}
						
						percent = std::min<uint16_t>(percent, 100);
						const auto index = combatTypeToIndex(modifier->getConversionType());
						if (auto [it, inserted] = playerList.try_emplace(index, ModifierTotals{flat, percent}); !inserted) {
							it->second += ModifierTotals{flat, percent};
						}
					}
				}
			}
		}
	}

	return playerList;
}

gtl::node_hash_map<uint8_t, ModifierTotals> Player::getAttackModifierTotals(const CombatType_t damageType, const CombatOrigin originType, const CreatureType_t creatureType, const RaceType_t race, const std::string_view creatureName) const
{
	
	gtl::node_hash_map<uint8_t, ModifierTotals> modMap;
	modMap.reserve(ATTACK_MODIFIER_LAST);
	
	auto attackMods = getAttackModifiers();
	for (uint8_t i = ATTACK_MODIFIER_NONE; i < ATTACK_MODIFIER_LAST; ++i) {
		auto modTotals = getValidatedTotals(attackMods[i], damageType, originType, creatureType, race, creatureName);
		modMap.try_emplace(i, modTotals);
	}
	return modMap;
}

gtl::node_hash_map<uint8_t, ModifierTotals> Player::getDefenseModifierTotals(const CombatType_t damageType, const CombatOrigin originType, const CreatureType_t creatureType, const RaceType_t race, std::string_view creatureName) const
{
	
	gtl::node_hash_map<uint8_t, ModifierTotals> modMap;
	modMap.reserve(DEFENSE_MODIFIER_LAST);
	
	auto defenseMods = getDefenseModifiers();
	// todo: skip reform in this loop
	for (uint8_t i = DEFENSE_MODIFIER_FIRST; i <= DEFENSE_MODIFIER_LAST; ++i) {
		auto modTotals = getValidatedTotals(defenseMods[i], damageType, originType, creatureType, race, creatureName);
		modMap.try_emplace(i, modTotals);
	}
	return modMap;
}

std::vector<Position> Player::getOpenPositionsInRadius(int radius) const {
	std::vector<Position> openPositions;
	const auto& center = getPosition();
	for (int x = -radius; x <= radius; ++x) {
		for (int y = -radius; y <= radius; ++y) {
			Position pos(center.x + x, center.y + y, center.z);
			if (pos.z != center.z) {
				continue; // make sure its same floor, maybe in future we remove this check XD
			}

			auto tile = g_game.map.getTile(pos);
			const bool isValid = tile
			&& g_game.canThrowObjectTo(center, pos)
			&& !tile->getZone() == ZONE_PROTECTION
			&& !tile->hasFlag(TILESTATE_PROTECTIONZONE
				| TILESTATE_FLOORCHANGE
				| TILESTATE_TELEPORT
				| TILESTATE_IMMOVABLEBLOCKSOLID
				| TILESTATE_NOPVPZONE
				| TILESTATE_IMMOVABLEBLOCKPATH
				| TILESTATE_IMMOVABLENOFIELDBLOCKPATH);

			if (isValid) {
				openPositions.push_back(pos);
			}
		}
	}

	return openPositions;
}

void Player::absorbDamage(const std::optional<CreaturePtr> attacker,
							CombatDamage& originalDamage,
							int32_t percent,
							int32_t flat) {
	int32_t absorbDamage = 0;
	const int32_t originalDamageValue = std::abs(originalDamage.primary.value);
	if (percent) {
		absorbDamage += originalDamageValue  * percent / 100;
	}
	if (flat) {
		absorbDamage += flat;
	}

	if (absorbDamage != 0) {
		absorbDamage = std::min<int32_t>(absorbDamage, originalDamageValue);
		originalDamage.primary.value += absorbDamage;

		auto absorb = CombatDamage{};
		absorb.leeched = true;
		absorb.origin = ORIGIN_AUGMENT;
		absorb.primary.type = COMBAT_HEALING;
		absorb.primary.value = absorbDamage;
		absorb.augmented = true;

		auto absorbParams = CombatParams{};
		absorbParams.origin = ORIGIN_AUGMENT;
		absorbParams.combatType = COMBAT_HEALING;
		absorbParams.impactEffect = CONST_ME_MAGIC_RED;
		absorbParams.distanceEffect = CONST_ANI_NONE;

		if (!attacker.has_value()) {
			Combat::doTargetCombat(nullptr, this->getPlayer(), absorb, absorbParams);
			return;
		}

		Combat::doTargetCombat(attacker.value(), this->getPlayer(), absorb, absorbParams);
	}
}

void Player::restoreManaFromDamage(std::optional<CreaturePtr> attacker,
									CombatDamage& originalDamage,
									int32_t percent,
									int32_t flat) {
	int32_t restoreDamage = 0;
	const int32_t originalDamageValue = std::abs(originalDamage.primary.value);
	if (percent) {
		restoreDamage += originalDamageValue  * percent / 100;
	}
	if (flat) {
		restoreDamage += flat;
	}

	if (restoreDamage != 0) {
		restoreDamage = std::min<int32_t>(restoreDamage, originalDamageValue);
		originalDamage.primary.value += restoreDamage;

		auto restore = CombatDamage{};
		restore.leeched = true;
		restore.origin = ORIGIN_AUGMENT;
		restore.primary.type = COMBAT_MANADRAIN;
		restore.primary.value = restoreDamage;
		restore.augmented = true;

		auto restoreParams = CombatParams{};
		restoreParams.origin = ORIGIN_AUGMENT;
		restoreParams.combatType = COMBAT_MANADRAIN;
		restoreParams.impactEffect = CONST_ME_ENERGYHIT;
		restoreParams.distanceEffect = CONST_ANI_NONE;

		if (!attacker.has_value()) {
			Combat::doTargetCombat(nullptr, this->getPlayer(), restore, restoreParams);
			return;
		}

		Combat::doTargetCombat(attacker.value(), this->getPlayer(), restore, restoreParams);
	}
}

void Player::reviveSoulFromDamage(std::optional<CreaturePtr> attacker,
									CombatDamage& originalDamage,
									int32_t percent,
									int32_t flat) {
	int32_t reviveDamage = 0;
	const int32_t originalDamageValue = std::abs(originalDamage.primary.value);
	if (percent) {
		reviveDamage += originalDamageValue  * percent / 100;
	}
	if (flat) {
		reviveDamage += flat;
	}

	if (reviveDamage != 0) {
		reviveDamage = std::min<int32_t>(reviveDamage,  originalDamageValue);
		originalDamage.primary.value += reviveDamage;

		auto message = (attacker.has_value()) ?
			"You gained " + std::to_string(reviveDamage) + " soul from " + attacker.value()->getName() + "'s attack." :
			"You gained " + std::to_string(reviveDamage) + " soul from revival.";
		
		sendTextMessage(MESSAGE_HEALED, message);
		changeSoul(reviveDamage);
	}
}

void Player::replenishStaminaFromDamage(std::optional<CreaturePtr> attacker,
										CombatDamage& originalDamage,
										int32_t percent,
										int32_t flat) {
	int32_t replenishDamage = 0;
	const int32_t originalDamageValue = std::abs(originalDamage.primary.value);
	if (percent) {
		replenishDamage += originalDamageValue  * percent / 100;
	}
	if (flat) {
		replenishDamage += flat;
	}

	if (replenishDamage != 0) {
		replenishDamage = std::min<int32_t>(replenishDamage,  originalDamageValue);
		originalDamage.primary.value += replenishDamage;

		if (!g_config.getBoolean(ConfigManager::AUGMENT_STAMINA_RULE)) {
			replenishDamage = replenishDamage / 60;
		}

		auto message = (attacker.has_value()) ?
			"You gained " + std::to_string(replenishDamage) + " stamina from " + attacker.value()->getName() + "'s attack." :
			"You gained " + std::to_string(replenishDamage) + " stamina from replenishment.";

		sendTextMessage(MESSAGE_HEALED,  message);
		addStamina(static_cast<uint16_t>(replenishDamage));
	}
}

void Player::resistDamage(std::optional<CreaturePtr> attacker,
							CombatDamage& originalDamage,
							int32_t percent,
							int32_t flat) const
{
	int32_t resistDamage = 0;
	const int32_t originalDamageValue = std::abs(originalDamage.primary.value);
	if (percent) {
		resistDamage += originalDamageValue  * percent / 100;
	}
	if (flat) {
		resistDamage += flat;
	}

	if (resistDamage != 0) {
		resistDamage = std::min<int32_t>(resistDamage, originalDamageValue);
		originalDamage.primary.value += resistDamage;
		
		auto message = (attacker.has_value()) ?
			"You resisted " + std::to_string(resistDamage) + " damage from " + attacker.value()->getName() + "'s attack." :
			"You resisted " + std::to_string(resistDamage) + " damage.";

		sendTextMessage(MESSAGE_HEALED, message);
	}
}

void Player::reflectDamage(std::optional<CreaturePtr> attacker,
							CombatDamage& originalDamage,
							int32_t percent,
							int32_t flat,
							uint8_t areaEffect,
							uint8_t distanceEffect) {
	
	if (!attacker.has_value()) {
		return;
	}

	int32_t reflectDamage = 0;
	const int32_t originalDamageValue = std::abs(originalDamage.primary.value);
	if (percent) {
		reflectDamage += originalDamageValue  * percent / 100;
	}
	if (flat) {
		reflectDamage += flat;
	}

	if (reflectDamage != 0)	{
		const auto& target = attacker.value();
		reflectDamage = std::min<int32_t>(reflectDamage, originalDamageValue);
		originalDamage.primary.value += reflectDamage;

		auto reflect = CombatDamage{};
		reflect.primary.type = originalDamage.primary.type;
		reflect.primary.value = (0 - reflectDamage);
		reflect.origin = ORIGIN_AUGMENT;
		reflect.augmented = true;

		auto params = CombatParams{};
		params.distanceEffect = distanceEffect;
		params.impactEffect = areaEffect;
		params.origin = ORIGIN_AUGMENT;
		params.combatType = originalDamage.primary.type;

		sendTextMessage(
		MESSAGE_DAMAGE_DEALT,
		 "You reflected " + std::to_string(reflectDamage) + " damage from " + target->getName() + "'s attack back at them."
		 );
	
		Combat::doTargetCombat(this->getPlayer(), target, reflect, params);
	}
}

void Player::deflectDamage(std::optional<CreaturePtr> attackerOpt, 
                          CombatDamage& originalDamage, 
                          int32_t percent, 
                          int32_t flat, 
                          CombatOrigin paramOrigin, 
                          uint8_t areaEffect, 
                          uint8_t distanceEffect) {
	
    int32_t deflectDamage = 0;
    const int32_t originalDamageValue = std::abs(originalDamage.primary.value);

	if (percent) {
		deflectDamage += originalDamageValue  * percent / 100;
	}
	if (flat) {
		deflectDamage += flat;
	}
    
    if (deflectDamage > 0) {
    	deflectDamage = std::min(deflectDamage, originalDamageValue);
    	originalDamage.primary.value += deflectDamage;
        constexpr int32_t DAMAGE_DIVIDER = 50.0; // Should be moved to global config
        constexpr int32_t MAX_TARGETS = 6.0;
        const int32_t calculatedTargets = std::min<int32_t>(
            std::round<int32_t>((deflectDamage) / DAMAGE_DIVIDER) + 1, 
            MAX_TARGETS
        );
    	
        auto defensePos = getPosition();
        const auto attackPos = generateAttackPosition(attackerOpt, defensePos, paramOrigin);
        const auto damageArea = generateDeflectArea(attackerOpt, calculatedTargets);
    	
        auto deflect = CombatDamage{};
        deflect.primary.type = originalDamage.primary.type;
        deflect.origin = ORIGIN_AUGMENT;
        deflect.primary.value = -1 * std::round<int32_t>(deflectDamage / calculatedTargets);
		deflect.augmented = true;
    	
        auto params = CombatParams();
        params.origin = ORIGIN_AUGMENT;
        params.combatType = originalDamage.primary.type;
        params.distanceEffect = distanceEffect;
        params.targetCasterOrTopMost = true;
        params.impactEffect = (areaEffect == CONST_ME_NONE) 
            ? CombatTypeToAreaEffect(originalDamage.primary.type) 
            : areaEffect;
    	
        sendTextMessage(
            MESSAGE_EVENT_DEFAULT,
            "You deflected " + std::to_string(deflectDamage) + " total damage."
        );
    	
        Combat::doAreaCombat(this->getPlayer(), attackPos, damageArea.get(), deflect, params);
    }
}

void Player::ricochetDamage(CombatDamage& originalDamage,
							int32_t percent,
							int32_t flat,
							uint8_t areaEffect,
							uint8_t distanceEffect) {

	int32_t ricochetDamage = 0;
	const int32_t originalDamageValue = std::abs(originalDamage.primary.value);

	if (percent) {
		ricochetDamage += originalDamageValue  * percent / 100;
	}
	if (flat) {
		ricochetDamage += flat;
	}

	auto targetList = getOpenPositionsInRadius(3);

	if (ricochetDamage != 0 && targetList.size() > 0) {
		const auto& targetPos = targetList[uniform_random(0, targetList.size() - 1)];
		ricochetDamage = std::min(ricochetDamage, originalDamageValue);
		originalDamage.primary.value += ricochetDamage;

		auto message = "An attack on you ricocheted " + std::to_string(ricochetDamage) + " damage.";
		sendTextMessage(MESSAGE_EVENT_ADVANCE, message);

		auto ricochet = CombatDamage{};
		ricochet.primary.type = originalDamage.primary.type;
		ricochet.primary.value = (0 - ricochetDamage);
		ricochet.origin = ORIGIN_AUGMENT;
		ricochet.augmented = true;

		auto params = CombatParams();
		params.origin = ORIGIN_AUGMENT;
		params.combatType = originalDamage.primary.type;
		params.distanceEffect = distanceEffect;
		params.targetCasterOrTopMost = true;
		params.impactEffect = (areaEffect == CONST_ME_NONE) ? CombatTypeToAreaEffect(originalDamage.primary.type) : areaEffect;

		const auto& damageArea = std::make_unique<AreaCombat>();
		damageArea->setupArea(Deflect1xArea, 5);
		Combat::doAreaCombat(this->getPlayer(), targetPos, damageArea.get(), ricochet, params);
	}
}

void Player::convertDamage(const CreaturePtr& target, CombatDamage& originalDamage, gtl::node_hash_map<uint8_t, ModifierTotals> conversionList) {
	auto iter = conversionList.begin();

	while (originalDamage.primary.value < 0 && iter != conversionList.end()) {

		const CombatType_t combatType = indexToCombatType(iter->first);
		const ModifierTotals& totals = iter->second;

		int32_t convertedDamage = 0;
		const int32_t percent = static_cast<int32_t>(totals.percentTotal);
		const int32_t flat = static_cast<int32_t>(totals.flatTotal);
		const int32_t originalDamageValue = std::abs(originalDamage.primary.value);
		if (percent) {
			convertedDamage += originalDamageValue  * percent / 100;
		}
		if (flat) {
			convertedDamage += flat;
		}

		if (convertedDamage != 0 && target) {
			convertedDamage = std::min<int32_t>(convertedDamage, originalDamageValue);
			originalDamage.primary.value += convertedDamage;
			
			auto converted = CombatDamage{};
			converted.primary.type = combatType;
			converted.primary.value = (0 - convertedDamage);
			converted.origin = ORIGIN_AUGMENT;
			converted.augmented = true;

			auto params = CombatParams{};
			params.combatType = combatType;
			params.origin = ORIGIN_AUGMENT;
			
			auto message = "You converted " + std::to_string(convertedDamage) + " " + getCombatName(originalDamage.primary.type) + " damage to " + getCombatName(combatType) + " during an attack on " + target->getName() + ".";
			sendTextMessage(MESSAGE_DAMAGE_DEALT, message);
			Combat::doTargetCombat(this->getPlayer(), target, converted, params);
		}
		++iter;
	}
}

void Player::reformDamage(std::optional<CreaturePtr> attacker, CombatDamage& originalDamage, gtl::node_hash_map<uint8_t, ModifierTotals> conversionList) {
	auto iter = conversionList.begin();

	while (originalDamage.primary.value < 0 && iter != conversionList.end()) {

		CombatType_t combatType = indexToCombatType(iter->first);
		const ModifierTotals& totals = iter->second;

		int32_t reformedDamage = 0;
		int32_t percent = static_cast<int32_t>(totals.percentTotal);
		int32_t flat = static_cast<int32_t>(totals.flatTotal);
		const int32_t originalDamageValue = std::abs(originalDamage.primary.value);
		if (percent) {
			reformedDamage += originalDamageValue  * percent / 100;
		}
		if (flat) {
			reformedDamage += flat;
		}

		if (reformedDamage) {
			reformedDamage = std::min<int32_t>(reformedDamage, originalDamageValue);
			originalDamage.primary.value += reformedDamage;

			auto reform = CombatDamage{};
			reform.primary.type = combatType;
			reform.primary.value = (0 - reformedDamage);
			reform.origin = ORIGIN_AUGMENT;
			reform.augmented = true;

			auto params = CombatParams{};
			params.combatType = combatType;
			params.origin = ORIGIN_AUGMENT;
			
			auto message = (attacker.has_value()) ?
				"You reformed " + std::to_string(reformedDamage) + " " + getCombatName(originalDamage.primary.type) + " damage from " + getCombatName(combatType) + " during an attack on you by " + attacker.value()->getName() + "." :
				"You reformed " + std::to_string(reformedDamage) + " " + getCombatName(originalDamage.primary.type) + " damage from " + getCombatName(combatType) + ".";
			
			sendTextMessage(MESSAGE_DAMAGE_DEALT, message);
			auto target = (attacker.has_value()) ? attacker.value() : nullptr;
			Combat::doTargetCombat(target, this->getPlayer(), reform, params);
		}
		++iter;
	}
}

std::vector<ItemPtr> Player::getEquipment(bool validateSlot) const
{
	std::vector<ItemPtr> equipment;
	for (uint8_t slot = CONST_SLOT_FIRST; slot <= CONST_SLOT_LAST; ++slot)
	{
		if (const auto& item = inventory[slot])
		{
			if (validateSlot) 
			{
				if (item->getEquipSlot() == getPositionForSlot(static_cast<slots_t>(slot)))
				{
					if (g_config.getBoolean(ConfigManager::CLASSIC_EQUIPMENT_SLOTS)
						and ((slot == CONST_SLOT_RIGHT or slot == CONST_SLOT_LEFT) and (item->getWeaponType() != WEAPON_NONE and item->getWeaponType() != WEAPON_AMMO))
						or (slot == CONST_SLOT_AMMO) and (item->getWeaponType() == WEAPON_AMMO or item->getLightInfo().level > 0))
					{
						equipment.push_back(item);
					}
					else if (!g_config.getBoolean(ConfigManager::CLASSIC_EQUIPMENT_SLOTS))
					{
						equipment.push_back(item);
					}
				}
			}
			else
			{
				equipment.push_back(item);
			}
		}
	}
	return equipment;
}

Position Player::generateAttackPosition(std::optional<CreaturePtr> attacker, Position& defensePosition, CombatOrigin origin) {

	const Direction attackDirection = (attacker.has_value())
		? getDirectionTo(defensePosition, attacker.value()->getPosition())
		: getOppositeDirection(this->getDirection());
	
	// Offsets
	static constexpr std::array<std::array<int, 4>, 8> DIRECTION_PATTERNS = { {
			// x_start, x_end, y_start, y_end
			{-1, 1, -2, -1},	// NORTH:
			{1, 2, -1, 1},	// EAST:
			{-1, 1, 1, 2},	// SOUTH:
			{-2, -1, -1, 1},	// WEST:
			{-2, -1, 1, 2},	// SOUTHWEST:
			{1, 2, 1, 2},		// SOUTHEAST:
			{-2, -1, -2, -1},	// NORTHWEST:
			{1, 2, -2, -1}	// NORTHEAST:
		} };

	std::vector<Position> possibleTargets;
	possibleTargets.reserve(9);

	const auto& pattern = DIRECTION_PATTERNS[attackDirection & 0x7]; // Mask to handle diagonal directions

	auto addLocationInline = [&](int x, int y) {
		Position targetLocation{
			static_cast<uint16_t>(defensePosition.x + x),
			static_cast<uint16_t>(defensePosition.y + y),
			defensePosition.z
		};

		const auto& tile = g_game.map.getTile(targetLocation);
		const bool isValid = tile
			&& g_game.canThrowObjectTo(defensePosition, targetLocation)
			&& !tile->getZone() == ZONE_PROTECTION
			&& !tile->hasFlag(TILESTATE_PROTECTIONZONE
				| TILESTATE_FLOORCHANGE
				| TILESTATE_TELEPORT
				| TILESTATE_IMMOVABLEBLOCKSOLID
				| TILESTATE_NOPVPZONE
				| TILESTATE_IMMOVABLEBLOCKPATH
				| TILESTATE_IMMOVABLENOFIELDBLOCKPATH);

		if (isValid) {
			possibleTargets.emplace_back(targetLocation);
		}
	};
	
	for (int x = pattern[0]; x <= pattern[1]; ++x) {
		for (int y = pattern[2]; y <= pattern[3]; ++y) {
			addLocationInline(x, y);
		}
	}

	const size_t vectorSize = possibleTargets.size();
	const size_t index = vectorSize ? (std::rand() % vectorSize) : 0;
	return vectorSize ? possibleTargets[index] : Spells::getCasterPosition(this->getPlayer(), getOppositeDirection(this->getDirection()));
} 

std::unique_ptr<AreaCombat> Player::generateDeflectArea(std::optional<CreaturePtr> attacker, int32_t targetCount) const
{
	auto combatArea = std::make_unique<AreaCombat>();
	const auto& defendersPosition = this->getPosition();

	switch (const auto direction = (attacker.has_value()) ? getDirectionTo(defendersPosition, attacker.value()->getPosition()) : getOppositeDirection(this->getDirection())) {
	case DIRECTION_NORTH:
	case DIRECTION_EAST:
	case DIRECTION_SOUTH:
	case DIRECTION_WEST: {
		const auto targetAreas = _StandardDeflectionMap.find(targetCount)->second;
			if (!targetAreas.empty()) {
				const auto index = std::rand() % targetAreas.size();
				const auto area = targetAreas[index];
				combatArea->setupArea(area, 5);
			}
		break;
	}
	case DIRECTION_SOUTHWEST:
	case DIRECTION_SOUTHEAST:
	case DIRECTION_NORTHWEST:
	case DIRECTION_NORTHEAST: {
		if (const auto targetAreas = _DiagonalDeflectionMap.find(targetCount)->second; !targetAreas.empty()) {
			const auto index = std::rand() % targetAreas.size();
			const auto area = targetAreas[index];
			combatArea->setupExtArea(area, 5);
		}
		break;
	}
	[[unlikely]]
	default:
		std::cerr << "Deflection area attempted to be generated from unknown direction!" << std::endl;
		break;
	}

	return combatArea;
}

void Player::increaseDamage(	std::optional<CreaturePtr> attacker,
								CombatDamage& originalDamage,
								int32_t percent,
								int32_t flat) const
{
	int32_t increasedDamage = 0;
	const int32_t originalDamageValue = std::abs(originalDamage.primary.value);
	if (percent) {
		increasedDamage += originalDamageValue * percent / 100;
	}

	if (flat) {
		increasedDamage += flat;
	}

	if (increasedDamage != 0) {
		increasedDamage = std::min<int32_t>(increasedDamage, originalDamageValue);
		originalDamage.primary.value -= increasedDamage;

		auto message = (attacker.has_value()) ?
			"You took an additional " + std::to_string(increasedDamage) + " damage from " + attacker.value()->getName() + "'s attack." :
			"You took an additional " + std::to_string(increasedDamage) + " damage.";

		sendTextMessage(MESSAGE_DAMAGE_RECEIVED, message);
	}
}