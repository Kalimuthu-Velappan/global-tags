/*
 * Copyright (c) 2014, 2015
 *	Tama Communications Corporation
 *
 * This file is part of GNU GLOBAL.
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
#ifndef _OUTPUT_H_
#define _OUTPUT_H_

#include "convert.h"
#include "gtagsop.h"

void start_output(void);
void end_output(void);
//KALS
int output_with_formatting(void*, CONVERT *, GTP *, const char *, int);

#endif /* ! _OUTPUT_H_ */

