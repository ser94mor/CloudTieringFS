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

#ifndef CLOUDTIERING_POLICY_PROCESSOR_H
#define CLOUDTIERING_POLICY_PROCESSOR_H

#include "queue.h"

void policy_processor(queue_t *raw_input_queue,
                      queue_t *demotion_primary_queue,
                      queue_t *demotion_secondary_queue,
                      queue_t *promotion_primary_queue,
                      queue_t *promotion_secondary_queue);

int assess_then_process_file(const char *path);

#endif    /* CLOUDTIERING_POLICY_PROCESSOR_H */
