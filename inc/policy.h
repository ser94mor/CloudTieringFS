/**
 * Copyright (C) 2017  Sergey Morozov <sergey@morozov.ch>
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

#include "queue.h"

int scan_fs(queue_t *download_queue, queue_t *upload_queue);

#endif    /* CLOUDTIERING_POLICY_H */
