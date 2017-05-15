/**
 * Copyright (C) 2017  Sergey Morozov <sergey94morozov@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef CLOUDTIERING_POLICY_H
#define CLOUDTIERING_POLICY_H

/*******************************************************************************
* POLICY                                                                       *
* ------                                                                       *
* TODO: write description                                                      *
*******************************************************************************/

#include <sys/types.h>

#include "defs.h"

#define MAX_POLICY_RULE_COUNT  32
#define MAX_CONDITION_COUNT    32
#define MAX_ACTION_COUNT       32

#define POLICY_RULES(action,   sep)           \
        action( file_demotion_policy  )  sep  \
        action( file_promotion_policy )

enum policy_rule_enum {
        POLICY_RULES(ENUMERIZE,COMMA),
};

typedef struct {
        int (*condition[MAX_CONDITION_COUNT])();
        int (*action[MAX_ACTION_COUNT])();
        size_t cond_num;
        size_t action_num;
        enum policy_rule_enum policy_rule_id;
} policy_rule_t;

typedef struct {
        policy_rule_t policy_rule[MAX_POLICY_RULE_COUNT];
        size_t policy_rule_num;
} policy_t;

policy_t  *get_policy();

#endif    /* CLOUDTIERING_POLICY_H */
