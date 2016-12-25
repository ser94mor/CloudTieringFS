################################################################################
#  Copyright (C) 2016  Sergey Morozov <sergey94morozov@gmail.com>              #
#                                                                              #
#  This program is free software: you can redistribute it and/or modify        #
#  it under the terms of the GNU General Public License as published by        #
#  the Free Software Foundation, either version 3 of the License, or           #
#  (at your option) any later version.                                         #
#                                                                              #
#  This program is distributed in the hope that it will be useful,             #
#  but WITHOUT ANY WARRANTY; without even the implied warranty of              #
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the               #
#  GNU General Public License for more details.                                #
#                                                                              #
#  You should have received a copy of the GNU General Public License           #
#  along with this program.  If not, see <http://www.gnu.org/licenses/>.       #
################################################################################

#
# Single makefile to build all components of cloudtiering.
#


### versions
MAJOR_VER ::= 0
MINOR_VER ::= 2
PATCH_VER ::= 0
VERSION   ::= ${MAJOR_VER}.${MINOR_VER}.${PATCH_VER}


### names
NAME       ::= cloudtiering
APP_NAME   ::= ${NAME}-daemon
LIB_SONAME ::= lib${NAME}.so
LIB_NAME   ::= ${LIB_SONAME}.${VERSION}
TST_NAME   ::= ${NAME}-test


### directories
INC_DIR    ::= inc
BIN_DIR    ::= bin
SRC_DIR    ::= src
APP_SUBDIR ::= app
LIB_SUBDIR ::= lib
TST_SUBDIR ::= tst


### lists of source files
src_func  = $(wildcard ${SRC_DIR}/$($(1)_SUBDIR)/*.c)
LIB_SRC ::= $(call src_func,LIB)
APP_SRC ::= $(call src_func,APP)
TST_SRC ::= $(call src_func,TST)


### lists of produced objects
obj_func  = $(subst ${SRC_DIR},${BIN_DIR},$($(1)_SRC:.c=.o))
LIB_OBJ ::= $(call obj_func,LIB)
APP_OBJ ::= $(call obj_func,APP)
TST_OBJ ::= $(call obj_func,TST)


### targets
all: lib app tst validate


lib: mkdir-${BIN_DIR}/${LIB_SUBDIR} ${LIB_OBJ}
	gcc -pthread -shared -Wl,-soname,${LIB_SONAME} ${LIB_OBJ} -o ${BIN_DIR}/${LIB_NAME} -ldotconf -ls3 -ldl
	ln --symbolic --force ${LIB_NAME} ${BIN_DIR}/${LIB_SONAME}


app: mkdir-${BIN_DIR}/${APP_SUBDIR} lib ${APP_OBJ}
	gcc -pthread ${APP_OBJ} -o ${BIN_DIR}/${APP_NAME} -L${BIN_DIR} -l:${LIB_NAME}


tst: mkdir-${BIN_DIR}/${TST_SUBDIR} lib ${TST_OBJ}
	gcc -pthread ${TST_OBJ} -o ${BIN_DIR}/${TST_NAME} -L${BIN_DIR} -l:${LIB_NAME}


validate: lib app tst mkdir-${BIN_DIR}/validate
	@pushd ${BIN_DIR}/validate 1>/dev/null && \
	LD_LIBRARY_PATH=../ ../${TST_NAME} && \
	popd 1>/dev/null


${BIN_DIR}/${LIB_SUBDIR}/%.o: ${SRC_DIR}/${LIB_SUBDIR}/%.c
	gcc -pthread -fPIC -g -c -Wall -std=c11 $(addprefix -I,${INC_DIR}) -o $@ $<

${BIN_DIR}/${APP_SUBDIR}/%.o: ${SRC_DIR}/${APP_SUBDIR}/%.c
	gcc -pthread -g -Wall -std=c11 $(addprefix -I,${INC_DIR}) -c $< -o $@


${BIN_DIR}/${TST_SUBDIR}/%.o: ${SRC_DIR}/${TST_SUBDIR}/%.c
	gcc -pthread -g -Wall -std=c11 $(addprefix -I,${INC_DIR}) -c $< -o $@


mkdir-${BIN_DIR}/%:
	mkdir --parents $(subst mkdir-,,$@)


clean:
	rm --recursive --force ${BIN_DIR}


clean-%: clean-${BIN_DIR}/%
	rm --force ${BIN_DIR}/$($(shell tr '[:lower:]' '[:upper:]' <<< $(subst clean-${BIN_DIR}/,,$<))_NAME)


clean-${BIN_DIR}/%:
	rm --recursive --force $(subst clean-,,$@)

