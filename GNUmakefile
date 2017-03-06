################################################################################
#  Copyright (C) 2016, 2017  Sergey Morozov <sergey94morozov@gmail.com>        #
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
MAJOR_VER := 0
MINOR_VER := 3
PATCH_VER := 0


### names
NAME         := cloudtiering
app_NAME     := ${NAME}-daemon

lib_LNK_NAME := lib${NAME}.so
lib_SONAME   := ${lib_LNK_NAME}.${MAJOR_VER}
lib_NAME     := ${lib_SONAME}.${MINOR_VER}.${PATCH_VER}

tst_NAME     := ${NAME}-test


### directories
INC_DIR    := inc
BIN_DIR    := bin
SRC_DIR    := src
app_SUBDIR := app
lib_SUBDIR := lib
tst_SUBDIR := tst
com_SUBDIR := com


### lists of source files
src_func  = $(wildcard ${SRC_DIR}/$($(1)_SUBDIR)/*.c)
com_SRC  := $(call src_func,com)
lib_SRC  := $(call src_func,lib) ${com_SRC}
app_SRC  := $(call src_func,app) ${com_SRC}
tst_SRC  := $(call src_func,tst) \
            $(filter-out ${SRC_DIR}/${app_SUBDIR}/daemon.c,${app_SRC})

lib_MAP  := $(wildcard ${SRC_DIR}/${lib_SUBDIR}/*.map)

### lists of produced objects
obj_func  = $(subst ${com_SUBDIR},${$(1)_SUBDIR},\
                    $(subst ${SRC_DIR},${BIN_DIR},$($(1)_SRC:.c=.o)))

lib_OBJ  := $(call obj_func,lib)
app_OBJ  := $(call obj_func,app)
tst_OBJ  := $(call obj_func,tst)


# dependencies
lib_DEP := dl rt
app_DEP := dotconf s3 rt
tst_DEP := ${app_DEP}


### compiler
CC                   := gcc-5
CC_FLAGS_CMPL_COMMON := -g -c -Wall -pthread -std=c11 -O3 -I${INC_DIR}
CC_FLAGS_LNK_COMMON  := -g -pthread

lib_CC_FLAGS_CMPL := ${CC_FLAGS_CMPL_COMMON} -fPIC
app_CC_FLAGS_CMPL := ${CC_FLAGS_CMPL_COMMON}
tst_CC_FLAGS_CMPL := ${CC_FLAGS_CMPL_COMMON}

lib_CC_FLAGS_LNK  := \
        ${CC_FLAGS_LNK_COMMON} $(addprefix -l,${lib_DEP}) \
        -shared -Wl,-Bsymbolic,-soname,${lib_SONAME},--version-script,${lib_MAP}
app_CC_FLAGS_LNK  := ${CC_FLAGS_LNK_COMMON} $(addprefix -l,${app_DEP})
tst_CC_FLAGS_LNK  := ${CC_FLAGS_LNK_COMMON} $(addprefix -l,${tst_DEP})


### helper functions

# find source file for *.o file
# (e.g. for bin/app/bar.o: src/app/bar.c or src/com/bar.c ?)
choose_file = $(wildcard ${SRC_DIR}/$(1)/$(2).c ${SRC_DIR}/${com_SUBDIR}/$(2).c)


### targets
.SECONDEXPANSION:
all: app lib tst validate


app lib tst: %: $${$$@_OBJ} ${BIN_DIR}/%/%.out
	@ln --force ${BIN_DIR}/$@/$@.out ${BIN_DIR}/${$@_NAME}


${BIN_DIR}/%.o: $$(call choose_file,$$(*D),$$(*F)) | ${BIN_DIR}/$$(*D)
	${CC} ${$(*D)_CC_FLAGS_CMPL} -o $@ $<


${BIN_DIR}/%.out: $${$$(*D)_OBJ} | ${BIN_DIR}/$$(*D)
	${CC} ${$(*D)_CC_FLAGS_LNK} -o $@ $^


$(addprefix ${BIN_DIR}/,app lib tst validate):
	mkdir --parents $@


.PHONY:
validate: ${BIN_DIR}/$$@ app lib tst
	@ln --symbolic --force ${lib_NAME} ${BIN_DIR}/${lib_SONAME}
	@ln --symbolic --force ${lib_SONAME} ${BIN_DIR}/${lib_LNK_NAME}
	@pushd ${BIN_DIR}/validate 1>/dev/null && \
	LD_LIBRARY_PATH=../ ../${tst_NAME} && \
	popd 1>/dev/null


.PHONY:
clean:
	rm --recursive --force ${BIN_DIR}


.PHONY:
clean-%:
	rm --force --recursive ${BIN_DIR}/$($*_NAME) ${BIN_DIR}/$*

